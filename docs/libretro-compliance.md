# LessUI Libretro API Compliance Report

This document analyzes LessUI's Player module implementation against the libretro API specification (as defined in `libretro.h`).

## Summary

**Overall Compliance: GOOD** - LessUI implements the core libretro frontend requirements correctly with a few minor deviations that don't affect compatibility with most cores.

---

## Core Lifecycle

### ✅ Callback Registration (Fully Compliant)

The libretro spec requires callbacks be set **before** `retro_init()`:

| Callback                       | Spec Requirement           | LessUI Implementation                        |
| ------------------------------ | -------------------------- | -------------------------------------------- |
| `retro_set_environment`        | Before `retro_init()`      | ✅ Set in `Core_load()` before `Core_init()` |
| `retro_set_video_refresh`      | Before first `retro_run()` | ✅ Set in `Core_load()`                      |
| `retro_set_audio_sample`       | Before first `retro_run()` | ✅ Set in `Core_load()`                      |
| `retro_set_audio_sample_batch` | Before first `retro_run()` | ✅ Set in `Core_load()`                      |
| `retro_set_input_poll`         | Before first `retro_run()` | ✅ Set in `Core_load()`                      |
| `retro_set_input_state`        | Before first `retro_run()` | ✅ Set in `Core_load()`                      |

**Implementation:** `player.c:3629-3668`

### ✅ Core Lifecycle Functions (Fully Compliant)

| Function                     | Usage                           | Compliance                 |
| ---------------------------- | ------------------------------- | -------------------------- |
| `retro_init()`               | Called once after callbacks set | ✅ `Core_init()`           |
| `retro_load_game()`          | Called to load content          | ✅ `Game_load()`           |
| `retro_get_system_info()`    | Called for core metadata        | ✅ Called in `Core_load()` |
| `retro_get_system_av_info()` | Called after game load          | ✅ Called in `Game_load()` |
| `retro_run()`                | Called each frame               | ✅ Main loop               |
| `retro_unload_game()`        | Called before deinit            | ✅ `Game_close()`          |
| `retro_deinit()`             | Called on shutdown              | ✅ `Core_unload()`         |
| `retro_reset()`              | Soft reset                      | ✅ Available via menu      |

---

## Video Callback

### ✅ `retro_video_refresh_t` (Fully Compliant)

**Spec Requirements:**

1. Accept `data`, `width`, `height`, `pitch` parameters
2. Support frame duping (NULL data) when `GET_CAN_DUPE` returns true
3. Support pixel formats: 0RGB1555 (default), RGB565, XRGB8888

**LessUI Implementation:** `player.c:3486-3491`

```c
void video_refresh_callback(const void* data, unsigned width, unsigned height, size_t pitch) {
    if (!data)
        return;  // Frame dupe - previous frame is re-presented
    video_refresh_callback_main(data, width, height, pitch);
}
```

| Feature                         | Status                                                |
| ------------------------------- | ----------------------------------------------------- |
| NULL data handling (frame dupe) | ✅ Returns early, vsync still presents previous frame |
| 0RGB1555 format                 | ✅ Supported with conversion                          |
| RGB565 format                   | ✅ Native format, no conversion                       |
| XRGB8888 format                 | ✅ Supported with conversion                          |
| Packed pitch recommendation     | ✅ Handled via `PlayerScaler_calculate()`             |

---

## Audio Callbacks

### ✅ `retro_audio_sample_t` (Fully Compliant)

**Spec:** Renders a single stereo sample (left, right as int16_t).

**Implementation:** `player.c:3507-3510`

```c
static void audio_sample_callback(int16_t left, int16_t right) {
    if (!fast_forward)
        SND_batchSamples(&(const SND_Frame){left, right}, 1);
}
```

### ✅ `retro_audio_sample_batch_t` (Fully Compliant)

**Spec:** Renders multiple audio frames. Returns number processed.

**Implementation:** `player.c:3525-3530`

```c
static size_t audio_sample_batch_callback(const int16_t* data, size_t frames) {
    if (!fast_forward)
        return SND_batchSamples((const SND_Frame*)data, frames);
    else
        return frames;  // Pretend we processed them during fast-forward
}
```

| Feature                          | Status                                        |
| -------------------------------- | --------------------------------------------- |
| Interleaved stereo format        | ✅ `SND_Frame` is `{int16_t left, right}`     |
| Returns frames processed         | ✅ Returns actual count or `frames` during FF |
| Only one callback used per frame | ✅ Core chooses which to use                  |

---

## Input Callbacks

### ✅ `retro_input_poll_t` (Fully Compliant)

**Spec:** Must be called at least once per `retro_run()`.

**Implementation:** `player.c:2210-2329`

```c
static void input_poll_callback(void) {
    if (input_polled_this_frame)
        return;  // Guard against multiple calls
    input_polled_this_frame = 1;
    PAD_poll();
    // ... handle shortcuts, menu, etc.
}
```

**Fallback Mechanism:** The main loop also calls `input_poll_callback()` after `core.run()` to ensure input is polled even if a core doesn't call it (e.g., during error screens). The guard prevents double execution.

### ✅ `retro_input_state_t` (Fully Compliant)

**Spec:** Query input state for port/device/index/id.

**Implementation:** `player.c:2330-2378`

```c
static int16_t input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port == 0 && device == RETRO_DEVICE_JOYPAD && index == 0) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return buttons;  // Bitmask optimization
        return (buttons >> id) & 1;
    }
    // Analog stick support
    if (device == RETRO_DEVICE_ANALOG) {
        // ... returns analog values
    }
    return 0;
}
```

| Feature                       | Status                                                 |
| ----------------------------- | ------------------------------------------------------ |
| `RETRO_DEVICE_JOYPAD`         | ✅ Full support                                        |
| `RETRO_DEVICE_ANALOG`         | ✅ Full support                                        |
| `RETRO_DEVICE_ID_JOYPAD_MASK` | ✅ Bitmask optimization supported                      |
| Multi-port                    | ⚠️ Only port 0 fully implemented (single-player focus) |

---

## Environment Callback

### ✅ Core Environment Commands (player.c:2469-2732)

#### Video/Display Commands

| Command              | ID  | Status | Notes                                 |
| -------------------- | --- | ------ | ------------------------------------- |
| `SET_ROTATION`       | 1   | ✅     | Stores rotation state                 |
| `GET_OVERSCAN`       | 2   | ✅     | Returns `true`                        |
| `GET_CAN_DUPE`       | 3   | ✅     | Returns `true`                        |
| `SET_PIXEL_FORMAT`   | 10  | ✅     | Supports all 3 formats                |
| `SET_GEOMETRY`       | 37  | ✅     | Forces scaler recalculation           |
| `SET_SYSTEM_AV_INFO` | 32  | ✅     | Updates timing, aspect, reinits audio |

#### Input Commands

| Command                         | ID  | Status | Notes                                        |
| ------------------------------- | --- | ------ | -------------------------------------------- |
| `SET_INPUT_DESCRIPTORS`         | 11  | ✅     | Parses for control mapping                   |
| `GET_INPUT_DEVICE_CAPABILITIES` | 24  | ✅     | Returns JOYPAD \| ANALOG                     |
| `GET_INPUT_BITMASKS`            | 51  | ✅     | Returns `true`                               |
| `SET_CONTROLLER_INFO`           | 35  | ✅     | Detects custom controllers (e.g., DualShock) |
| `GET_RUMBLE_INTERFACE`          | 23  | ✅     | Provides rumble callback                     |

#### Directory/Path Commands

| Command                | ID  | Status | Notes                    |
| ---------------------- | --- | ------ | ------------------------ |
| `GET_SYSTEM_DIRECTORY` | 9   | ✅     | Returns `core.bios_dir`  |
| `GET_SAVE_DIRECTORY`   | 31  | ✅     | Returns `core.saves_dir` |

#### Core Options Commands

| Command                                    | ID  | Status | Notes                                  |
| ------------------------------------------ | --- | ------ | -------------------------------------- |
| `GET_VARIABLE`                             | 15  | ✅     | Returns option values                  |
| `SET_VARIABLES`                            | 16  | ✅     | Legacy variable support                |
| `GET_VARIABLE_UPDATE`                      | 17  | ✅     | Returns change flag                    |
| `GET_CORE_OPTIONS_VERSION`                 | 52  | ✅     | Returns version 1                      |
| `SET_CORE_OPTIONS`                         | 53  | ✅     | Options v1 support                     |
| `SET_CORE_OPTIONS_INTL`                    | 54  | ✅     | Internationalization support           |
| `SET_CORE_OPTIONS_DISPLAY`                 | 55  | ✅     | Sets option visibility dynamically     |
| `SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK` | 69  | ✅     | Invoked on menu open and option change |
| `SET_VARIABLE`                             | 70  | ✅     | Core-initiated variable changes        |

#### Disk Control Commands

| Command                              | ID  | Status | Notes                 |
| ------------------------------------ | --- | ------ | --------------------- |
| `SET_DISK_CONTROL_INTERFACE`         | 13  | ✅     | Basic disk control    |
| `GET_DISK_CONTROL_INTERFACE_VERSION` | 57  | ✅     | Returns version 1     |
| `SET_DISK_CONTROL_EXT_INTERFACE`     | 58  | ✅     | Extended disk control |

#### Audio Commands

| Command                            | ID  | Status | Notes                         |
| ---------------------------------- | --- | ------ | ----------------------------- |
| `SET_AUDIO_CALLBACK`               | 22  | ⚠️     | Acknowledged, not implemented |
| `SET_AUDIO_BUFFER_STATUS_CALLBACK` | 62  | ✅     | Reports buffer occupancy      |

#### Timing/Performance Commands

| Command                   | ID  | Status | Notes                                  |
| ------------------------- | --- | ------ | -------------------------------------- |
| `SET_FRAME_TIME_CALLBACK` | 21  | ✅     | Called before each `core.run()`        |
| `GET_PERF_INTERFACE`      | 28  | ✅     | CPU features (NEON), timing, profiling |
| `GET_FASTFORWARDING`      | 49  | ✅     | Returns fast-forward state             |
| `GET_TARGET_REFRESH_RATE` | 50  | ✅     | Returns core FPS                       |
| `GET_THROTTLE_STATE`      | 71  | ✅     | Returns throttle mode and rate         |
| `GET_AUDIO_VIDEO_ENABLE`  | 47  | ✅     | Returns VIDEO \| AUDIO enabled         |

#### Hardware Rendering Commands

| Command                   | ID  | Status | Notes                            |
| ------------------------- | --- | ------ | -------------------------------- |
| `SET_HW_RENDER`           | 14  | ✅     | Gracefully rejects, logs request |
| `GET_PREFERRED_HW_RENDER` | 56  | ✅     | Returns `RETRO_HW_CONTEXT_NONE`  |

#### Other Commands

| Command                     | ID  | Status | Notes                                    |
| --------------------------- | --- | ------ | ---------------------------------------- |
| `SET_MESSAGE`               | 6   | ✅     | Logs message                             |
| `SET_MESSAGE_EXT`           | 60  | ✅     | Extended messages with priority/duration |
| `SET_PERFORMANCE_LEVEL`     | 8   | ⚠️     | Acknowledged, not used                   |
| `GET_LOG_INTERFACE`         | 27  | ✅     | Provides logging callback                |
| `GET_LANGUAGE`              | 39  | ✅     | Returns `RETRO_LANGUAGE_ENGLISH`         |
| `SET_SUPPORT_NO_GAME`       | 18  | ⚠️     | Acknowledged, not used                   |
| `SET_CONTENT_INFO_OVERRIDE` | 65  | ⚠️     | Acknowledged, not used                   |

---

## Memory Interface

### ✅ SRAM/RTC Persistence (Fully Compliant)

**Spec:** Frontend calls `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` and `retro_get_memory_size()` to read/write battery-backed save RAM.

**Implementation:** `player_memory.c`

```c
PlayerMemoryResult PlayerMemory_read(const char* filepath, unsigned memory_type,
                                     PlayerGetMemorySizeFn get_size,
                                     PlayerGetMemoryDataFn get_data) {
    size_t mem_size = get_size(memory_type);
    if (!mem_size) return PLAYER_MEM_NO_SUPPORT;

    FILE* file = fopen(filepath, "rb");
    // ... reads directly into core memory
}
```

| Memory Type               | Status                                |
| ------------------------- | ------------------------------------- |
| `RETRO_MEMORY_SAVE_RAM`   | ✅ Read on game load, written on exit |
| `RETRO_MEMORY_RTC`        | ✅ Supported for cores that use it    |
| `RETRO_MEMORY_SYSTEM_RAM` | ❌ Not exposed (not required)         |
| `RETRO_MEMORY_VIDEO_RAM`  | ❌ Not exposed (not required)         |

### ✅ Save States (Fully Compliant)

**Spec:** `retro_serialize_size()`, `retro_serialize()`, `retro_unserialize()` for save states.

**Implementation:** `player_state.c`

```c
PlayerStateResult PlayerState_read(const char* filepath, const PlayerStateCore* core) {
    size_t state_size = core->serialize_size();
    if (!state_size) return PLAYER_STATE_NO_SUPPORT;

    void* state_buffer = calloc(1, state_size);
    // ... reads and calls core->unserialize()
}
```

| Feature                  | Status                            |
| ------------------------ | --------------------------------- |
| `retro_serialize_size()` | ✅ Called to get buffer size      |
| `retro_serialize()`      | ✅ Used for saving                |
| `retro_unserialize()`    | ✅ Used for loading               |
| Auto-resume (slot 9)     | ✅ Saves on exit, loads on launch |
| Manual slots (0-8)       | ✅ Via in-game menu               |

---

## Main Loop Compliance

### ✅ Frame Timing (Fully Compliant)

**Spec Requirements:**

1. `retro_run()` executes one video frame
2. `retro_input_poll_t` must be called at least once per `retro_run()`
3. Frame time callback (if registered) should be called before `retro_run()`

**Implementation:** `player_loop_vsync.inc`

```c
while (!quit) {
    input_polled_this_frame = 0;  // Reset at start

    if (should_run_core) {
        // Frame time callback (per spec)
        if (video_state.frame_time_cb) {
            video_state.frame_time_cb(delta);
        }

        // Audio buffer status callback
        if (core.audio_buffer_status) {
            core.audio_buffer_status(true, occupancy, occupancy < 25);
        }

        core.run();  // Core calls input_poll internally
    }

    GFX_flip(screen);  // Vsync

    // Fallback input poll (for misbehaving cores)
    input_poll_callback();
}
```

---

## Notable Deviations

### 1. Single-Player Focus

LessUI only implements port 0 for input. Multi-player support would require:

- Implementing `input_state_callback` for ports 1-3
- Supporting `retro_set_controller_port_device` per port

**Impact:** Low - Target devices are single-player handhelds.

### 2. Software Rendering Only

LessUI properly handles hardware rendering requests:

- `SET_HW_RENDER` - Gracefully rejects with logging (cores fall back to software)
- `GET_PREFERRED_HW_RENDER` - Returns `RETRO_HW_CONTEXT_NONE`
- `GET_HW_RENDER_INTERFACE` - Not implemented (not needed when rejecting HW render)

**Impact:** Low - Software rendering is appropriate for target hardware. Cores fall back gracefully.

### 3. Core Options v2 Not Fully Supported

While v1 options are supported, the newer v2 category system (`SET_CORE_OPTIONS_V2`) is not implemented. Cores fall back to v1.

**Impact:** Low - All cores support v1 as fallback.

### 4. VFS Not Implemented

The Virtual File System interface (`GET_VFS_INTERFACE`) is not provided. Cores use standard file I/O.

**Impact:** Low - Most cores don't require VFS.

### 5. No Network/Achievements

- `RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS` - Not implemented
- Netplay-related callbacks - Not implemented

**Impact:** None - Not relevant for offline handheld use.

---

## Recommendations

### Should Implement

| Callback                        | ID  | Priority | Rationale                                                                                                           |
| ------------------------------- | --- | -------- | ------------------------------------------------------------------------------------------------------------------- |
| `SET_CORE_OPTIONS_V2`           | 67  | Medium   | Would improve menu organization for cores with many options (e.g., PCSX ReARMed). Cores fall back to v1 gracefully. |
| `GET_MESSAGE_INTERFACE_VERSION` | 59  | Medium   | Allows cores to know `SET_MESSAGE_EXT` is supported before using it. Simple to implement.                           |
| `SET_MINIMUM_AUDIO_LATENCY`     | 63  | Medium   | Useful for cores that do frameskip based on audio buffer fullness.                                                  |

### Nice to Have

| Feature                            | Priority | Rationale                                                                                                                 |
| ---------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------- |
| **Multi-port input**               | Low      | For multiplayer when external controllers are connected. Would require implementing `input_state_callback` for ports 1-3. |
| **OSD message display**            | Low      | `SET_MESSAGE_EXT` is implemented (logs messages), but a visual OSD would improve user feedback for core notifications.    |
| `SET_FASTFORWARDING_OVERRIDE` (64) | Low      | Allows cores to control fast-forward (useful for loading screens).                                                        |
| `GET_VFS_INTERFACE` (45)           | Low      | Virtual filesystem interface. Most cores don't require it.                                                                |

### Not Planned

| Feature                            | Rationale                                                                           |
| ---------------------------------- | ----------------------------------------------------------------------------------- |
| Hardware rendering (OpenGL/Vulkan) | Target hardware is software-rendering only. Cores fall back gracefully.             |
| Achievements support               | Not relevant for offline handheld use.                                              |
| Netplay                            | Not relevant for single-player handheld devices.                                    |
| Core Options v2 categories UI      | Even with v2 parsing, small screens make category navigation overhead questionable. |

---

## Conclusion

LessUI's libretro implementation is **spec-compliant** for all core functionality required to run games. The implementation correctly handles:

- ✅ Core lifecycle (init/load/run/unload/deinit)
- ✅ All frontend callbacks (video, audio, input)
- ✅ Essential environment callbacks (47+ commands)
- ✅ Memory persistence (SRAM, RTC, save states)
- ✅ Frame timing and pacing
- ✅ Hardware rendering rejection (cores fall back to software)
- ✅ Core option visibility callbacks
- ✅ Extended message interface
- ✅ CPU feature detection (NEON, VFP, ASIMD) via performance interface

The deviations from the full spec are intentional omissions for features not relevant to the target platform (single-player handhelds with software rendering). No compatibility issues have been reported with the 43+ cores supported by LessUI.
