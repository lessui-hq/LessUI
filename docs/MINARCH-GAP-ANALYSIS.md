# MinArch Libretro Implementation Gap Analysis

This document analyzes the MinArch libretro frontend implementation against the full libretro API specification, identifying implemented features, gaps, and recommendations.

## Table of Contents

- [Overview](#overview)
- [Core Functions](#core-functions)
- [Callbacks Provided to Cores](#callbacks-provided-to-cores)
- [Environment Callback Analysis](#environment-callback-analysis)
- [Gap Summary](#gap-summary)
- [Recommendations](#recommendations)

---

## Overview

**MinArch** is a lightweight libretro frontend focused on retro handheld gaming devices. It prioritizes simplicity and performance over feature completeness.

**Implementation Location:** `workspace/all/minarch/minarch.c`

### Design Philosophy
- Single-threaded by default (optional threaded video)
- Software rendering only (no hardware acceleration)
- RGB565/XRGB8888 pixel formats
- Focus on joypad and analog input (no keyboard/mouse)

---

## Core Functions

### ✅ Fully Implemented

| Function | Status | Notes |
|----------|--------|-------|
| `retro_api_version` | ✅ | Checked during load |
| `retro_init` | ✅ | Called via `core.init()` |
| `retro_deinit` | ✅ | Called via `core.deinit()` |
| `retro_get_system_info` | ✅ | Used to populate core metadata |
| `retro_get_system_av_info` | ✅ | Used to configure video/audio |
| `retro_set_controller_port_device` | ✅ | Supports gamepad type selection |
| `retro_reset` | ✅ | Called via `core.reset()` |
| `retro_run` | ✅ | Main emulation loop |
| `retro_serialize_size` | ✅ | Used for save states |
| `retro_serialize` | ✅ | Save state creation |
| `retro_unserialize` | ✅ | Save state loading |
| `retro_load_game` | ✅ | With ZIP extraction support |
| `retro_load_game_special` | ✅ | Loaded but likely unused |
| `retro_unload_game` | ✅ | Called on exit |
| `retro_get_region` | ✅ | Loaded via dlsym |
| `retro_get_memory_data` | ✅ | Used for SRAM/RTC |
| `retro_get_memory_size` | ✅ | Used for SRAM/RTC |

### ❌ Not Implemented

| Function | Status | Impact |
|----------|--------|--------|
| `retro_cheat_reset` | ❌ | No cheat support |
| `retro_cheat_set` | ❌ | No cheat support |

---

## Callbacks Provided to Cores

### ✅ Fully Implemented

| Callback | Function | Notes |
|----------|----------|-------|
| `retro_environment_t` | `environment_callback()` | Main frontend interface |
| `retro_video_refresh_t` | `video_refresh_callback()` | With rotation and scaling |
| `retro_audio_sample_t` | `audio_sample_callback()` | Single sample support |
| `retro_audio_sample_batch_t` | `audio_sample_batch_callback()` | Batch audio (more efficient) |
| `retro_input_poll_t` | `input_poll_callback()` | Called each frame |
| `retro_input_state_t` | `input_state_callback()` | Button and analog queries |

---

## Environment Callback Analysis

The environment callback is the primary interface for core-frontend communication. MinArch implements a subset focused on its target use case.

### ✅ Implemented (Functional)

| ID | Command | Implementation Quality |
|----|---------|----------------------|
| 1 | `SET_ROTATION` | ✅ Full - Software rotation (0°/90°/180°/270°) |
| 2 | `GET_OVERSCAN` | ✅ Returns `true` |
| 3 | `GET_CAN_DUPE` | ✅ Returns `true` (frame duping supported) |
| 6 | `SET_MESSAGE` | ⚠️ Logs to console only (no OSD) |
| 9 | `GET_SYSTEM_DIRECTORY` | ✅ Returns BIOS directory |
| 10 | `SET_PIXEL_FORMAT` | ✅ RGB565 native, XRGB8888 with conversion |
| 11 | `SET_INPUT_DESCRIPTORS` | ✅ Used for button name mapping |
| 13 | `SET_DISK_CONTROL_INTERFACE` | ✅ Basic disk swap support |
| 15 | `GET_VARIABLE` | ✅ Full core options support |
| 16 | `SET_VARIABLES` | ✅ Legacy options format |
| 17 | `GET_VARIABLE_UPDATE` | ✅ Tracks option changes |
| 18 | `SET_SUPPORT_NO_GAME` | ✅ Acknowledged but unused |
| 21 | `SET_FRAME_TIME_CALLBACK` | ✅ Full implementation with delta tracking |
| 22 | `SET_AUDIO_CALLBACK` | ⚠️ Acknowledged but no action taken |
| 23 | `GET_RUMBLE_INTERFACE` | ✅ Full rumble support via `set_rumble_state()` |
| 24 | `GET_INPUT_DEVICE_CAPABILITIES` | ✅ Reports JOYPAD and ANALOG |
| 27 | `GET_LOG_INTERFACE` | ✅ Full logging support |
| 31 | `GET_SAVE_DIRECTORY` | ✅ Returns saves directory |
| 32 | `SET_SYSTEM_AV_INFO` | ✅ Full - reinits audio if sample rate changes |
| 35 | `SET_CONTROLLER_INFO` | ⚠️ Partial - only detects DualShock |
| 37 | `SET_GEOMETRY` | ✅ Updates viewport without reinit |
| 47 | `GET_AUDIO_VIDEO_ENABLE` | ✅ Returns both enabled (fixes FBNeo) |
| 49 | `GET_FASTFORWARDING` | ✅ Returns fast-forward state |
| 50 | `GET_TARGET_REFRESH_RATE` | ✅ Returns core FPS |
| 51 | `GET_INPUT_BITMASKS` | ✅ Returns `true` |
| 52 | `GET_CORE_OPTIONS_VERSION` | ✅ Returns 1 (v1 options) |
| 53 | `SET_CORE_OPTIONS` | ✅ Full v1 options support |
| 54 | `SET_CORE_OPTIONS_INTL` | ✅ Uses US locale options |
| 55 | `SET_CORE_OPTIONS_DISPLAY` | ⚠️ Stub - no visibility control |
| 57 | `GET_DISK_CONTROL_INTERFACE_VERSION` | ✅ Returns 1 |
| 58 | `SET_DISK_CONTROL_EXT_INTERFACE` | ✅ Full extended disk control |
| 62 | `SET_AUDIO_BUFFER_STATUS_CALLBACK` | ✅ Full - reports buffer occupancy for frameskip |
| 65 | `SET_CONTENT_INFO_OVERRIDE` | ⚠️ Stub - acknowledged only |
| 70 | `SET_VARIABLE` | ✅ Allows cores to set options |
| 71 | `GET_THROTTLE_STATE` | ✅ Full - reports FF/vsync/unblocked |

### ❌ Not Implemented (Returns False)

#### High Priority Gaps

| ID | Command | Impact | Affected Cores |
|----|---------|--------|----------------|
| 14 | `SET_HW_RENDER` | No OpenGL/Vulkan cores | Mupen64Plus, Beetle PSX HW, PPSSPP |
| 36 | `SET_MEMORY_MAPS` | No achievement support | All (for RetroAchievements) |
| 42 | `SET_SUPPORT_ACHIEVEMENTS` | No achievement support | All |
| 67 | `SET_CORE_OPTIONS_V2` | No option categories | Modern cores |

#### Medium Priority Gaps

| ID | Command | Impact | Affected Cores |
|----|---------|--------|----------------|
| 7 | `SHUTDOWN` | Core can't request exit | Standalone games |
| 12 | `SET_KEYBOARD_CALLBACK` | No keyboard input | Computer emulators (DOSBox, etc.) |
| 34 | `SET_SUBSYSTEM_INFO` | No subsystem support | Super Game Boy, BSX |
| 39 | `GET_LANGUAGE` | No localization | Cores with translations |
| 45 | `GET_VFS_INTERFACE` | No virtual filesystem | Cores using VFS |
| 59 | `GET_MESSAGE_INTERFACE_VERSION` | No extended messages | Modern cores |
| 60 | `SET_MESSAGE_EXT` | No OSD messages | Modern cores |
| 63 | `SET_MINIMUM_AUDIO_LATENCY` | No latency control | mGBA |
| 69 | `SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK` | No dynamic visibility | Gambatte, fceumm |

#### Low Priority Gaps (Niche Features)

| ID | Command | Impact |
|----|---------|--------|
| 8 | `SET_PERFORMANCE_LEVEL` | No CPU demand hints |
| 19 | `GET_LIBRETRO_PATH` | Core can't find its own path |
| 25 | `GET_SENSOR_INTERFACE` | No accelerometer/gyroscope |
| 26 | `GET_CAMERA_INTERFACE` | No camera support |
| 28 | `GET_PERF_INTERFACE` | No performance counters |
| 29 | `GET_LOCATION_INTERFACE` | No GPS support |
| 30 | `GET_CORE_ASSETS_DIRECTORY` | No asset directory |
| 33 | `SET_PROC_ADDRESS_CALLBACK` | No custom extensions |
| 38 | `GET_USERNAME` | No username support |
| 40 | `GET_CURRENT_SOFTWARE_FRAMEBUFFER` | Core can't render to frontend buffer |
| 41 | `GET_HW_RENDER_INTERFACE` | No HW render interface |
| 43 | `SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE` | No HW context negotiation |
| 44 | `SET_SERIALIZATION_QUIRKS` | No quirk acknowledgment |
| 46 | `GET_LED_INTERFACE` | No LED control |
| 48 | `GET_MIDI_INTERFACE` | No MIDI support |
| 56 | `GET_PREFERRED_HW_RENDER` | Always software rendering |
| 61 | `GET_INPUT_MAX_USERS` | Always assumes 1 user |
| 64 | `SET_FASTFORWARDING_OVERRIDE` | Core can't control FF |
| 66 | `GET_GAME_INFO_EXT` | No extended game info |
| 68 | `SET_CORE_OPTIONS_V2_INTL` | No v2 options with translations |
| 72 | `GET_SAVESTATE_CONTEXT` | No savestate context |
| 74 | `GET_JIT_CAPABLE` | No JIT indication |
| 75 | `GET_MICROPHONE_INTERFACE` | No microphone support |
| 77 | `GET_DEVICE_POWER` | No battery info to cores |
| 78 | `SET_NETPACKET_INTERFACE` | No netplay support |

---

## Gap Summary

### By Category

| Category | Implemented | Partial | Missing | Notes |
|----------|-------------|---------|---------|-------|
| Video | 5 | 1 | 5 | No HW rendering |
| Audio | 4 | 1 | 2 | Buffer status callback added |
| Input | 5 | 1 | 3 | No keyboard/mouse/lightgun |
| Core Options | 6 | 1 | 2 | No v2 categories |
| Save States | 2 | 0 | 1 | No quirks support |
| Disk Control | 3 | 0 | 0 | **Complete** |
| Logging | 2 | 0 | 0 | **Complete** |
| Directories | 2 | 0 | 2 | Missing assets/libretro paths |
| Misc | 4 | 2 | 15+ | Many niche features |

### Critical for Specific Cores

| Core Type | Missing Features |
|-----------|------------------|
| N64 (Mupen64Plus) | `SET_HW_RENDER` |
| PSX HW (Beetle) | `SET_HW_RENDER` |
| PSP (PPSSPP) | `SET_HW_RENDER`, `SET_SUBSYSTEM_INFO` |
| Computer emulators | `SET_KEYBOARD_CALLBACK` |
| mGBA (audio sync) | `SET_MINIMUM_AUDIO_LATENCY` |
| RetroAchievements | `SET_MEMORY_MAPS`, `SET_SUPPORT_ACHIEVEMENTS` |

---

## Recommendations

### High Priority (Broad Impact)

1. **SET_CORE_OPTIONS_V2 (67)**
   - Many modern cores use v2 options for better organization
   - Implementation: Parse `retro_core_options_v2` structure, flatten categories for display
   - Effort: Medium

2. **SET_MESSAGE_EXT (60) + GET_MESSAGE_INTERFACE_VERSION (59)**
   - Enables on-screen notifications from cores
   - Implementation: Add OSD text rendering, message queue
   - Effort: Medium

### Medium Priority (Core Compatibility)

4. **SET_CONTROLLER_INFO (35) - Full Implementation**
   - Current implementation only checks for "dualshock"
   - Should properly expose available controller types
   - Effort: Low

5. **SET_CORE_OPTIONS_DISPLAY (55) - Full Implementation**
   - Currently a stub; should track visibility per option
   - Affects cores with conditional options (Gambatte palettes, etc.)
   - Effort: Low

6. **GET_LANGUAGE (39)**
   - Simple to implement, enables core localization
   - Return `RETRO_LANGUAGE_ENGLISH` or detect from system
   - Effort: Very Low

7. **SET_PERFORMANCE_LEVEL (8)**
   - Could be used to adjust CPU speed dynamically
   - Effort: Low

### Low Priority (Niche but Valuable)

8. **SET_SUBSYSTEM_INFO (34) / retro_load_game_special**
   - Enables Super Game Boy, Transfer Pak, etc.
   - Complex but enables unique features
   - Effort: High

9. **SET_MEMORY_MAPS (36) + SET_SUPPORT_ACHIEVEMENTS (42)**
   - Required for RetroAchievements integration
   - Would need external achievement service integration
   - Effort: High

### Not Recommended for MinArch

- **SET_HW_RENDER (14)** - Would require OpenGL/Vulkan support, contrary to MinArch's software-only design
- **GET_CAMERA_INTERFACE (26)** - Not applicable to handheld gaming devices
- **GET_LOCATION_INTERFACE (29)** - Not applicable
- **GET_MIDI_INTERFACE (48)** - Very niche

---

## Implementation Status Summary

```
Total Environment Commands: ~80
Implemented (Full):         32 (40%)
Implemented (Partial):       6 (7%)
Not Implemented:           ~42 (53%)
```

MinArch implements the core functionality needed for most retro gaming scenarios. The primary gaps are:
1. Hardware rendering (by design)
2. Modern core options (v2)
3. On-screen messages
4. Achievement support

For MinArch's target use case (handheld retro gaming with software rendering), the current implementation covers the essential libretro features. The recommended improvements focus on better core compatibility rather than adding fundamentally new capabilities.
