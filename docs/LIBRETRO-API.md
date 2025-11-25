# Libretro API Reference

This document provides a comprehensive overview of the libretro API as defined in `libretro.h`. The libretro API is a simple, well-documented interface for creating portable emulator cores.

## Table of Contents

- [Core Functions](#core-functions)
- [Callback Types](#callback-types)
- [Environment Callbacks](#environment-callbacks)
- [Input Devices](#input-devices)
- [Memory Types](#memory-types)
- [Pixel Formats](#pixel-formats)
- [Key Structures](#key-structures)

---

## Core Functions

These are the functions that every libretro core must implement. They are loaded dynamically via `dlsym()`.

### Lifecycle Functions

| Function | Description |
|----------|-------------|
| `retro_api_version()` | Returns `RETRO_API_VERSION` (currently 1) |
| `retro_init()` | Initialize the core. Called once before any other functions |
| `retro_deinit()` | Deinitialize the core. Called when unloading |
| `retro_reset()` | Reset the emulated system |
| `retro_run()` | Run one frame of emulation |

### Information Functions

| Function | Description |
|----------|-------------|
| `retro_get_system_info(info)` | Fills `retro_system_info` with core name, version, valid extensions |
| `retro_get_system_av_info(info)` | Fills `retro_system_av_info` with video geometry and timing info |
| `retro_get_region()` | Returns `RETRO_REGION_NTSC` (0) or `RETRO_REGION_PAL` (1) |

### Game Management

| Function | Description |
|----------|-------------|
| `retro_load_game(game)` | Load a game. `retro_game_info` contains path and/or data |
| `retro_load_game_special(type, info, num_info)` | Load game via subsystem (e.g., Super Game Boy) |
| `retro_unload_game()` | Unload the current game |

### Serialization (Save States)

| Function | Description |
|----------|-------------|
| `retro_serialize_size()` | Returns size needed for save state buffer |
| `retro_serialize(data, size)` | Saves current state to buffer |
| `retro_unserialize(data, size)` | Loads state from buffer |

### Memory Access

| Function | Description |
|----------|-------------|
| `retro_get_memory_data(id)` | Returns pointer to memory region (SRAM, RTC, etc.) |
| `retro_get_memory_size(id)` | Returns size of memory region |

### Cheats (Optional)

| Function | Description |
|----------|-------------|
| `retro_cheat_reset()` | Clear all active cheats |
| `retro_cheat_set(index, enabled, code)` | Set a cheat code |

### Controller

| Function | Description |
|----------|-------------|
| `retro_set_controller_port_device(port, device)` | Set input device type for a port |

---

## Callback Types

The frontend provides these callbacks to the core during initialization:

### `retro_environment_t`
```c
typedef bool (*retro_environment_t)(unsigned cmd, void *data);
```
Main interface for core-frontend communication. See [Environment Callbacks](#environment-callbacks).

### `retro_video_refresh_t`
```c
typedef void (*retro_video_refresh_t)(const void *data, unsigned width,
    unsigned height, size_t pitch);
```
Called by core to output a video frame.
- `data`: Pixel buffer (or `NULL` to duplicate previous frame)
- `width`, `height`: Frame dimensions
- `pitch`: Bytes per row (may be larger than width × pixel size)

### `retro_audio_sample_t`
```c
typedef void (*retro_audio_sample_t)(int16_t left, int16_t right);
```
Called to output a single stereo audio sample.

### `retro_audio_sample_batch_t`
```c
typedef size_t (*retro_audio_sample_batch_t)(const int16_t *data, size_t frames);
```
Called to output multiple stereo samples at once. More efficient than `retro_audio_sample_t`.
- `data`: Interleaved stereo samples (L, R, L, R, ...)
- `frames`: Number of stereo frames
- Returns: Number of frames consumed

### `retro_input_poll_t`
```c
typedef void (*retro_input_poll_t)(void);
```
Called once per frame before `retro_input_state_t` queries.

### `retro_input_state_t`
```c
typedef int16_t (*retro_input_state_t)(unsigned port, unsigned device,
    unsigned index, unsigned id);
```
Query the state of an input.
- `port`: Controller port (0-based)
- `device`: Device type (`RETRO_DEVICE_*`)
- `index`: Sub-device index (e.g., which analog stick)
- `id`: Button/axis ID

---

## Environment Callbacks

Environment callbacks are the primary way cores request features from the frontend. Called via `retro_environment_t` with a command ID.

### Video/Display

| ID | Command | Description |
|----|---------|-------------|
| 1 | `SET_ROTATION` | Request screen rotation (0=0°, 1=90°, 2=180°, 3=270° CCW) |
| 2 | `GET_OVERSCAN` | Query if overscan should be cropped (deprecated) |
| 3 | `GET_CAN_DUPE` | Query if frontend supports frame duping (NULL to video_refresh) |
| 10 | `SET_PIXEL_FORMAT` | Set pixel format (RGB565, XRGB8888, 0RGB1555) |
| 32 | `SET_SYSTEM_AV_INFO` | Update video/audio parameters (may reinit drivers) |
| 37 | `SET_GEOMETRY` | Update geometry without reinitializing |
| 40 | `GET_CURRENT_SOFTWARE_FRAMEBUFFER` | Get frontend-managed framebuffer (experimental) |

### Audio

| ID | Command | Description |
|----|---------|-------------|
| 22 | `SET_AUDIO_CALLBACK` | Set async audio callback |
| 62 | `SET_AUDIO_BUFFER_STATUS_CALLBACK` | Monitor audio buffer occupancy |
| 63 | `SET_MINIMUM_AUDIO_LATENCY` | Request minimum audio latency in ms |

### Input

| ID | Command | Description |
|----|---------|-------------|
| 11 | `SET_INPUT_DESCRIPTORS` | Describe core's input bindings |
| 12 | `SET_KEYBOARD_CALLBACK` | Set keyboard event callback |
| 23 | `GET_RUMBLE_INTERFACE` | Get rumble/vibration interface |
| 24 | `GET_INPUT_DEVICE_CAPABILITIES` | Query supported device types |
| 25 | `GET_SENSOR_INTERFACE` | Get accelerometer/gyroscope interface (experimental) |
| 35 | `SET_CONTROLLER_INFO` | Declare supported controller types |
| 51 | `GET_INPUT_BITMASKS` | Query support for bitmask input polling |
| 61 | `GET_INPUT_MAX_USERS` | Get number of active input devices |

### Core Options

| ID | Command | Description |
|----|---------|-------------|
| 15 | `GET_VARIABLE` | Get value of a core option |
| 16 | `SET_VARIABLES` | Set available options (deprecated, use v2) |
| 17 | `GET_VARIABLE_UPDATE` | Check if options changed since last query |
| 52 | `GET_CORE_OPTIONS_VERSION` | Get supported options API version (0, 1, or 2) |
| 53 | `SET_CORE_OPTIONS` | Set options (v1 format) |
| 54 | `SET_CORE_OPTIONS_INTL` | Set options with translations (v1) |
| 55 | `SET_CORE_OPTIONS_DISPLAY` | Show/hide specific options |
| 67 | `SET_CORE_OPTIONS_V2` | Set options with categories (v2 format) |
| 68 | `SET_CORE_OPTIONS_V2_INTL` | Set options with categories and translations |
| 69 | `SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK` | Register visibility update callback |
| 70 | `SET_VARIABLE` | Programmatically change an option value |

### Paths & Directories

| ID | Command | Description |
|----|---------|-------------|
| 9 | `GET_SYSTEM_DIRECTORY` | Get BIOS/system files directory |
| 19 | `GET_LIBRETRO_PATH` | Get path to the core .so file |
| 30 | `GET_CORE_ASSETS_DIRECTORY` | Get directory for core assets |
| 31 | `GET_SAVE_DIRECTORY` | Get directory for save data |

### Disk Control

| ID | Command | Description |
|----|---------|-------------|
| 13 | `SET_DISK_CONTROL_INTERFACE` | Set disk swap interface (deprecated) |
| 57 | `GET_DISK_CONTROL_INTERFACE_VERSION` | Get supported disk control version |
| 58 | `SET_DISK_CONTROL_EXT_INTERFACE` | Set extended disk control interface |

### Messages & Logging

| ID | Command | Description |
|----|---------|-------------|
| 6 | `SET_MESSAGE` | Display message to user (deprecated) |
| 27 | `GET_LOG_INTERFACE` | Get logging callback |
| 59 | `GET_MESSAGE_INTERFACE_VERSION` | Get message interface version |
| 60 | `SET_MESSAGE_EXT` | Display message with full control |

### Performance & Timing

| ID | Command | Description |
|----|---------|-------------|
| 8 | `SET_PERFORMANCE_LEVEL` | Hint about core's CPU demands |
| 21 | `SET_FRAME_TIME_CALLBACK` | Get notified of frame timing |
| 28 | `GET_PERF_INTERFACE` | Get performance counters interface |
| 49 | `GET_FASTFORWARDING` | Query if frontend is fast-forwarding |
| 50 | `GET_TARGET_REFRESH_RATE` | Get frontend's target refresh rate |
| 64 | `SET_FASTFORWARDING_OVERRIDE` | Control fast-forward state |
| 71 | `GET_THROTTLE_STATE` | Get detailed throttle information |

### Hardware Rendering

| ID | Command | Description |
|----|---------|-------------|
| 14 | `SET_HW_RENDER` | Request hardware rendering context (OpenGL, Vulkan, etc.) |
| 41 | `GET_HW_RENDER_INTERFACE` | Get hardware render interface |
| 43 | `SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE` | Configure HW context parameters |
| 44 | `SET_HW_SHARED_CONTEXT` | Request shared HW context |
| 56 | `GET_PREFERRED_HW_RENDER` | Query frontend's preferred rendering API |

### Serialization

| ID | Command | Description |
|----|---------|-------------|
| 44 | `SET_SERIALIZATION_QUIRKS` | Declare serialization limitations |
| 72 | `GET_SAVESTATE_CONTEXT` | Get context for how savestate will be used |

### Subsystems & Special Features

| ID | Command | Description |
|----|---------|-------------|
| 18 | `SET_SUPPORT_NO_GAME` | Declare core can run without content |
| 33 | `SET_PROC_ADDRESS_CALLBACK` | Expose core functions to frontend |
| 34 | `SET_SUBSYSTEM_INFO` | Declare subsystems (e.g., Super Game Boy) |
| 36 | `SET_MEMORY_MAPS` | Expose memory map for debugging/achievements |
| 42 | `SET_SUPPORT_ACHIEVEMENTS` | Declare achievement support |
| 65 | `SET_CONTENT_INFO_OVERRIDE` | Override content loading behavior |
| 66 | `GET_GAME_INFO_EXT` | Get extended game info |

### Miscellaneous

| ID | Command | Description |
|----|---------|-------------|
| 7 | `SHUTDOWN` | Request frontend shutdown |
| 26 | `GET_CAMERA_INTERFACE` | Get camera interface (experimental) |
| 29 | `GET_LOCATION_INTERFACE` | Get GPS/location interface |
| 38 | `GET_USERNAME` | Get frontend's configured username |
| 39 | `GET_LANGUAGE` | Get frontend's configured language |
| 45 | `GET_VFS_INTERFACE` | Get virtual file system interface |
| 46 | `GET_LED_INTERFACE` | Get LED control interface |
| 47 | `GET_AUDIO_VIDEO_ENABLE` | Query which A/V outputs are needed |
| 48 | `GET_MIDI_INTERFACE` | Get MIDI interface |
| 74 | `GET_JIT_CAPABLE` | Query if JIT compilation is available |
| 75 | `GET_MICROPHONE_INTERFACE` | Get microphone interface |
| 77 | `GET_DEVICE_POWER` | Get device battery/power state |
| 78 | `SET_NETPACKET_INTERFACE` | Set netplay packet interface |

---

## Input Devices

### Device Types

| ID | Constant | Description |
|----|----------|-------------|
| 0 | `RETRO_DEVICE_NONE` | No input |
| 1 | `RETRO_DEVICE_JOYPAD` | Standard RetroPad (SNES-style) |
| 2 | `RETRO_DEVICE_MOUSE` | Mouse with buttons and wheel |
| 3 | `RETRO_DEVICE_KEYBOARD` | Full keyboard |
| 4 | `RETRO_DEVICE_LIGHTGUN` | Light gun (Guncon-style) |
| 5 | `RETRO_DEVICE_ANALOG` | RetroPad with analog sticks |
| 6 | `RETRO_DEVICE_POINTER` | Touch/pointer input |

### RetroPad Buttons (RETRO_DEVICE_ID_JOYPAD_*)

| ID | Button | SNES Equivalent |
|----|--------|-----------------|
| 0 | B | South face button |
| 1 | Y | West face button |
| 2 | SELECT | Left-center |
| 3 | START | Right-center |
| 4 | UP | D-pad up |
| 5 | DOWN | D-pad down |
| 6 | LEFT | D-pad left |
| 7 | RIGHT | D-pad right |
| 8 | A | East face button |
| 9 | X | North face button |
| 10 | L | Left shoulder |
| 11 | R | Right shoulder |
| 12 | L2 | Left trigger |
| 13 | R2 | Right trigger |
| 14 | L3 | Left stick click |
| 15 | R3 | Right stick click |
| 256 | MASK | Bitmask of all buttons |

### Analog Indices

| Index | Constant | Description |
|-------|----------|-------------|
| 0 | `RETRO_DEVICE_INDEX_ANALOG_LEFT` | Left analog stick |
| 1 | `RETRO_DEVICE_INDEX_ANALOG_RIGHT` | Right analog stick |
| 2 | `RETRO_DEVICE_INDEX_ANALOG_BUTTON` | Analog button values |

Analog axis IDs: `RETRO_DEVICE_ID_ANALOG_X` (0), `RETRO_DEVICE_ID_ANALOG_Y` (1)

---

## Memory Types

| ID | Constant | Description |
|----|----------|-------------|
| 0 | `RETRO_MEMORY_SAVE_RAM` | Battery-backed save RAM |
| 1 | `RETRO_MEMORY_RTC` | Real-time clock data |
| 2 | `RETRO_MEMORY_SYSTEM_RAM` | Main system RAM |
| 3 | `RETRO_MEMORY_VIDEO_RAM` | Video RAM |

---

## Pixel Formats

| ID | Constant | Description |
|----|----------|-------------|
| 0 | `RETRO_PIXEL_FORMAT_0RGB1555` | 16-bit, 5-5-5 (deprecated) |
| 1 | `RETRO_PIXEL_FORMAT_XRGB8888` | 32-bit, 8-8-8-8 |
| 2 | `RETRO_PIXEL_FORMAT_RGB565` | 16-bit, 5-6-5 (preferred) |

---

## Key Structures

### retro_system_info
```c
struct retro_system_info {
    const char *library_name;      // Core name
    const char *library_version;   // Version string
    const char *valid_extensions;  // "bin|rom|iso"
    bool need_fullpath;            // True if core needs file path
    bool block_extract;            // True if archives shouldn't be extracted
};
```

### retro_system_av_info
```c
struct retro_system_av_info {
    struct retro_game_geometry geometry;
    struct retro_system_timing timing;
};

struct retro_game_geometry {
    unsigned base_width;    // Nominal width
    unsigned base_height;   // Nominal height
    unsigned max_width;     // Maximum width
    unsigned max_height;    // Maximum height
    float aspect_ratio;     // 0.0 = use base_width/base_height
};

struct retro_system_timing {
    double fps;             // Target frames per second
    double sample_rate;     // Audio sample rate in Hz
};
```

### retro_game_info
```c
struct retro_game_info {
    const char *path;  // Path to game file (may be NULL)
    const void *data;  // Game data in memory (may be NULL)
    size_t size;       // Size of data
    const char *meta;  // Metadata hint (usually NULL)
};
```

### retro_variable (Core Options v0)
```c
struct retro_variable {
    const char *key;    // Option key
    const char *value;  // Option value or definition
};
```

### retro_disk_control_ext_callback
```c
struct retro_disk_control_ext_callback {
    retro_set_eject_state_t set_eject_state;
    retro_get_eject_state_t get_eject_state;
    retro_get_image_index_t get_image_index;
    retro_set_image_index_t set_image_index;
    retro_get_num_images_t get_num_images;
    retro_replace_image_index_t replace_image_index;
    retro_add_image_index_t add_image_index;
    retro_set_initial_image_t set_initial_image;
    retro_get_image_path_t get_image_path;
    retro_get_image_label_t get_image_label;
};
```

### retro_rumble_interface
```c
struct retro_rumble_interface {
    retro_set_rumble_state_t set_rumble_state;
};

// Usage: set_rumble_state(port, effect, strength)
// effect: RETRO_RUMBLE_STRONG or RETRO_RUMBLE_WEAK
// strength: 0-65535
```

### retro_log_callback
```c
struct retro_log_callback {
    retro_log_printf_t log;  // printf-style logging function
};

enum retro_log_level {
    RETRO_LOG_DEBUG = 0,
    RETRO_LOG_INFO,
    RETRO_LOG_WARN,
    RETRO_LOG_ERROR
};
```

### retro_frame_time_callback
```c
struct retro_frame_time_callback {
    retro_frame_time_callback_t callback;  // Called with delta time
    retro_usec_t reference;                // Expected frame time in microseconds
};
```

---

## API Version History

The libretro API uses a single version number (`RETRO_API_VERSION = 1`) to maintain backwards compatibility. New features are added through:

1. **New environment callbacks** - Added with new command IDs
2. **Extended interfaces** - V2/Ext versions of existing interfaces
3. **Experimental flag** - `RETRO_ENVIRONMENT_EXPERIMENTAL` (0x10000) for unstable features

Cores should check for feature support via environment callbacks before using them.

---

## References

- [libretro.h](../workspace/all/minarch/libretro-common/include/libretro.h) - Full API header
- [libretro documentation](https://docs.libretro.com/) - Official documentation
- [RetroArch](https://github.com/libretro/RetroArch) - Reference frontend implementation
