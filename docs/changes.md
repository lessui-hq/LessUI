# Changes from MinUI

LessUI is a fork of [MinUI](https://github.com/shauninman/MinUI) by Shaun Inman. This document summarizes the major changes made since forking in October 2025.

## Overview

Since forking, LessUI has seen 412 commits across several major areas:

- Comprehensive test suite with 1400+ tests
- Major codebase refactoring for maintainability
- New audio and video systems
- Automatic CPU frequency scaling
- Resolution-independent UI
- Cross-platform pak architecture
- Modern developer tooling

---

## Testing Infrastructure

**Added a comprehensive test suite from scratch.**

MinUI had no automated tests. LessUI now has 1470 tests across 47 test suites covering the core business logic.

Key additions:

- Unity test framework for C unit testing
- fff (Fake Function Framework) for mocking SDL functions
- GCC `--wrap` for filesystem function mocking
- Docker-based test runner (Ubuntu 24.04) for consistent cross-platform results
- Code coverage support with lcov
- Integration tests for end-to-end workflows

The testing approach focuses on extracting pure logic into testable modules while mocking hardware dependencies (SDL, filesystem, libretro).

---

## Code Refactoring

**Extracted monolithic files into focused, testable modules.**

The main files `minarch.c` (peaked at ~7200 lines) and `minui.c` (peaked at ~2900 lines) have been systematically refactored, extracting reusable logic into 50+ focused modules with standardized naming conventions.

### MinArch Modules (libretro frontend)

| Module          | Purpose                                        |
| --------------- | ---------------------------------------------- |
| minarch_config  | Config path generation, option mapping         |
| minarch_options | Option list search and manipulation            |
| minarch_paths   | Save file path generation                      |
| minarch_memory  | SRAM/RTC persistence with injectable callbacks |
| minarch_state   | Save state read/write, auto-resume             |
| minarch_utils   | Core name extraction, string utilities         |
| minarch_zip     | ZIP extraction (copy, deflate)                 |
| minarch_input   | Input state queries, button mapping            |
| minarch_core    | Core AV info processing, aspect ratio          |
| minarch_menu    | In-game menu system                            |
| minarch_env     | Libretro environment callback handlers         |
| minarch_cpu     | Auto CPU scaling algorithm                     |
| minarch_game    | ZIP parsing, extension matching, M3U detection |
| minarch_scaler  | Video scaling geometry calculations            |
| minarch_archive | 7z/ZIP archive extraction                      |

### MinUI Modules (launcher)

| Module            | Purpose                                                |
| ----------------- | ------------------------------------------------------ |
| minui_entry       | Entry type, array operations, IntArray                 |
| minui_launcher    | Shell command construction, quote escaping             |
| minui_state       | Path decomposition, resume handling                    |
| minui_file_utils  | File/directory checking utilities                      |
| minui_utils       | Index char, console detection                          |
| minui_directory   | Console detection, entry scanning, collation           |
| minui_navigation  | Navigation logic, entry dispatch, auto-launch          |
| minui_thumbnail   | Thumbnail cache, fade animation, preload hints         |
| minui_context     | Global state management                                |
| minui_m3u         | M3U playlist parsing                                   |
| minui_map         | ROM display name aliasing                              |
| minui_str_compare | Natural string sorting                                 |
| directory_index   | Alias application, duplicate detection, alpha indexing |
| collection_parser | Custom ROM list parsing                                |
| recent_file       | Recent games I/O and array operations                  |

### Common Modules

| Module           | Purpose                                                 |
| ---------------- | ------------------------------------------------------- |
| collections      | Array and Hash data structures (khash-backed StringMap) |
| effect_system    | Visual effect state management                          |
| platform_variant | Hardware variant detection                              |
| nointro_parser   | No-Intro ROM naming conventions                         |
| gfx_text         | Text truncation, wrapping, sizing                       |
| pad              | Button state machine, analog input                      |
| audio_resampler  | Sample rate conversion with linear interpolation        |
| ui_layout        | Resolution-independent layout (DP system)               |
| render_common    | Destination rect calculation, color conversion          |
| render_sdl2      | Unified SDL2 rendering backend (8 platforms)            |
| frame_pacer      | Bresenham-style frame pacing                            |

All modules follow standardized naming: `MinArch[Module]_functionName()` or `MinUI[Module]_functionName()`.

### Rendering Refactor

Extracted duplicated rendering code from platform files into shared modules, with a net reduction of ~3,400 lines. SDL2 platforms (8 devices) now share `render_sdl2` backend; SDL1 platforms use `effect_system` for consistent effect state management.

---

## Audio System

**Replaced basic audio handling with adaptive resampling and rate control.**

- **Linear interpolation resampling** for smooth audio at any sample rate
- **Dual-timescale PI controller** for stable audio synchronization
  - Smooths error signal (0.9) to filter jitter
  - Quadratic integral weighting for faster convergence far from 50%
  - Integral clamped to ±1% for persistent hardware drift correction
- **Dynamic rate control** that adjusts playback speed to prevent buffer underruns
  - Parameters tuned for handheld timing variance (d=0.010, 5-frame buffer)
  - `SND_newFrame()` updates integral once per frame to prevent over-accumulation
- **Audio buffer status callback** enabling cores to implement frameskip
- **Dual sync modes** with compile-time selection:
  - **Vsync mode** (default): Frame pacing via Bresenham accumulator, non-blocking audio writes
  - **Audioclock mode** (M17): Audio hardware clock drives timing, blocking writes when buffer full

The audioclock mode fixes audio stuttering on devices with unstable vsync (like M17).

### Removed Legacy Audio Code

- Removed "Prevent Tearing" (vsync) and "Prioritize Audio" (threaded video) settings
- Removed `coreThread()` and all pthread synchronization code
- Removed `SAMPLES=400` overrides from 5 platforms (workarounds for pre-rate-control underruns)

---

## Video System

**Added modern rendering features and optimizations.**

### NEON Optimizations

- **XRGB8888→RGB565 color conversion** processing 8 pixels per iteration
- **0RGB1555→RGB565 conversion** for legacy cores (like mame2003+)
- **RGB565 rotation** using 4x4 block transpose for vertical arcade games

### Libretro Support

- **Screen rotation** for vertical arcade games
- **NTSC/PAL support** via video timing callbacks
- **Frame time callback** properly passing delta time per libretro spec

### Frame Pacing

Decouples emulation from display refresh rate using Bresenham-style pacing:

- Q16.16 fixed-point accumulator (no float drift)
- Direct mode bypass for mismatches ≤2% (handled by audio rate control)
- 2% tolerance based on RetroArch rate control research (Arntzen 2012)
- Handles 60fps on 72Hz (M17), 50fps PAL on 60Hz, etc.

### Visual Effects

- Configurable scanline patterns with scale-specific assets (line-N.png, grid-N.png, crt-N.png)
- Unified linear opacity formula: `opacity = 40 + (scale * 20)`
- Consistent effect state management across SDL1 and SDL2 platforms

### Thumbnail System

- 3-item LRU cache with direction-aware preloading
- Background loading makes sequential navigation instant
- Fade-in animations
- Auto-invalidation on eviction to prevent dangling pointers

---

## CPU Frequency Scaling

**Implemented automatic CPU scaling based on emulation demand.**

### Auto CPU Mode

Monitors audio buffer stress to automatically adjust CPU frequency:

- Uses rate control stress metric (0-1 normalized, buffer-size independent)
- Window-based averaging with hysteresis (1s boost, 1.5s reduce)
- Background thread applies changes without blocking emulation
- Startup grace period avoids false positives during buffer fill

### Panic Tracking Failsafe

- Tracks panics per-frequency and blocks frequencies that fail repeatedly (3+ panics)
- When a frequency is blocked, all lower frequencies are also blocked
- Blocked frequencies are skipped during reduction
- Panic counts reset on menu entry

### Granular Frequency Detection

- Detects available frequencies from sysfs at runtime
- Scales through all available steps instead of just 3 fixed levels
- Conservative step-down limits (max 2 at once)
- 75% predicted utilization safety margin
- 8-window panic cooldown prevents oscillation

### CPU Preset System

Replaced 3-level presets with 4-level system:

- **IDLE** (20% of max): Tools and utilities
- **POWERSAVE** (55%): Light games
- **NORMAL** (80%): Standard games
- **PERFORMANCE** (100%): Demanding games
- **Auto** (default): Dynamic scaling based on demand

---

## Resolution-Independent UI

**Implemented a density-independent pixel (DP) system for consistent UI across screen sizes.**

### DP System

- Baseline of 144 PPI calibrated for handheld gaming devices (held at 12-18")
- Provides ~11% larger UI elements than Android's 160 PPI baseline
- Single constant works across all 12 supported platforms and their variants

### Dynamic Row Fitting

New algorithm calculates optimal rows from screen space:

- Treats footer as another row (simpler model)
- Prefers even pixels but accepts odd as fallback
- 98.5% average screen fill (was ~88% with gaps)
- 5-13 rows depending on screen size

### Asset Scaling

- Bilinear-scaled assets for quality at any resolution
- +1 tier asset loading for better antialiasing
- RGBA8888 intermediate surfaces for proper alpha handling

### Text Rendering

- `GFX_centerTextY()` for perfect vertical centering using TTF glyph metrics
- Fancy single-glyph centering for button labels

---

## Cross-Platform Pak Architecture

**Redesigned the pak system for better cross-platform support.**

### Four Complementary Systems

1. **Tool Paks** (`workspace/all/paks/Tools/`)
   - Self-contained cross-platform tools with native code support
   - Each pak has `pak.json`, `launch.sh`, and optional `src/`
   - Platform-specific resources via `<platform>/` directories

2. **Emulator Paks** (`workspace/all/paks/Emus/`)
   - Template-based generation from `cores.json` definitions
   - Build-time config merge: base + platform (reduces configs by 95%)
   - Per-core configuration with platform-specific overrides

3. **MinUI System Pak** (`workspace/all/paks/MinUI/`)
   - Template system with platform-specific configs and hooks
   - Replaces 11 redundant skeleton files (79-162 lines each)
   - Adding new platform requires only config + hook scripts

4. **Direct Paks** - Simple paks copied as-is

### New Tool Paks

- **File Server** - Web-based file transfer
- **System Report** - Comprehensive hardware detection:
  - Storage: MMC/SD/eMMC enumeration, boot device, secondary mounts
  - WiFi: Interfaces, chipset, signal strength, wpa_supplicant state
  - Bluetooth: Controller details, paired devices, RF kill
  - HDMI: State detection, audio routing, EDID
  - Power management, kernel config, USB modes, watchdog
- **Benchmark** - Performance testing and frequency debugging
- **Wifi** - Network configuration with on-screen keyboard

### Shell UI (shui)

New shell-based UI utility for pak interfaces:

- Progress indicators
- Confirmation dialogs
- Text input with full-screen keyboard
- Subtext support
- Persistent state across screens
- Auto-sleep management

---

## Core/Emulator System

**Migrated to prebuilt cores with comprehensive system support.**

### Architecture Changes

- Cores built separately in [LessUI-Cores](https://github.com/lessui-hq/LessUI-Cores) repository
- Template-based pak generation from `cores.json` definitions
- 16 unique cores supporting 26 systems (smart copying avoids duplicates)
- Switched from a7/a53 to arm32/arm64 terminology

### ROM Name Maps

- Replaced linear-search Hash with khash-backed StringMap
- Bundled arcade game name maps:
  - MAME: 49,000 entries
  - FBNeo: 24,000 entries
- User maps merged with pak maps (user overrides take precedence)
- `Map_loadForDirectory()` for efficient batch aliasing

### Performance Optimizations

- **EmuCache** for O(1) emulator lookups at startup (~100 syscalls eliminated)
- **strnatcasecmp_presorted()** for pre-stripped sort keys (~120k calls saved)

### 7z Integration

Replaced unzip with 7z across all operations:

- Update payload: 183MB → 120MB (34% smaller with LZMA2)
- ROM archive extraction: 90 lines of ZIP code → 20 lines
- Shared `bin/{arm,arm64}/7z` binaries for all platforms
- Supports both .zip and .7z formats uniformly

### Error Handling

- User-friendly error display when games fail to load
- Segfault recovery during core load
- Libretro error log capture for display
- Fallback input polling for misbehaving cores

### Integer Scaling

- Data-driven config generation (`generate-scaling-configs.py`)
- 95% fill threshold for large screens, 100% for small (<3")
- Sharp pixels forced for Native/Cropped modes automatically

---

## Developer Experience

**Added modern development tooling and workflows.**

### macOS Native Development

- Native SDL2 builds for rapid UI iteration
- AddressSanitizer integration for memory debugging
- Fake SD card environment (`workspace/desktop/FAKESD/`)
- Keyboard input mapping (arrows, A/S/W/Q, Enter, Space)

### Build System

- Docker-based cross-compilation with prebuilt toolchain containers
- Incremental builds for faster iteration
- GitHub Actions CI/CD pipeline
- Dev deploy scripts for on-device testing via rsync
- Standardized optimization flags (`-O3`, no `-Ofast` or `-Os`)
- Link-time optimization (`-flto`) on supported platforms

### Static Analysis Migration

Migrated from cppcheck to clang-tidy:

- Focused checks: bugprone, clang-analyzer, performance, portability, cert
- Fixed critical bugs found during migration:
  - Memory leak in api.c (unsafe realloc pattern)
  - Memory leak in collections.c (unsafe realloc pattern)
  - Uninitialized state_file variables in minarch.c
  - Null pointer checks before strlen

### Logging System

- Crash-safe native logging (binaries write directly via `LOG_FILE` env var)
- Optional `LOG_SYNC=1` for fsync after each write
- Debug tracing through init sequences
- Per-component log files (minui.log, shui.log, etc.)

### Code Formatting

- clang-format for C code (tabs, braces on same line, 100 char limit)
- shellcheck for shell scripts
- Formatters for JSON, Markdown, and other file types

---

## Security

**Addressed unsafe string handling throughout the codebase.**

- Added `safe_strcpy()` function (BSD strlcpy-style, always null-terminates)
- Replaced 85 unsafe `strcpy()` calls across 36 files
- Added `(void)` casts for 206 intentionally ignored return values
- Fixed overlapping memory issues with `memmove()` instead of `strcpy()`
- Bounds-checked sprintf/strcpy variants in platform code
- Initialized ioctl structures to prevent kernel garbage

---

## Platform Support

**Improved platform abstraction and variant detection.**

### Platform Variant Detection

- Refactored detection into `platform_variant.c` module
- Fallback `PLAT_detectVariant()` for single-device platforms
- Runtime variant detection with proper macro patterns

### Platform Fixes

- **miyoomini**: 560p support, Flip USB audio with separate speaker/headphone volumes
- **rg35xxplus**: Fix mid-poweroff resume, crash when RGXX_MODEL unset
- **zero28**: Fix screen not turning on after sleep on some boards
- **tg5040**: Helaas-based limbo fix with direct AXP power management
- **my355**: Fix right analog stick axes, boot time on Miyoo Flip
- **my282**: Fix wrong CPU architecture (A53→A7), hardcode frequencies
- **rgb30**: Fix HDMI disabled despite hardware support

### Architecture Flags

Added proper CPU-specific compiler flags:

- miyoomini/m17: `-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve`
- tg5040/zero28: `-mtune=cortex-a53 -mcpu=cortex-a53` (AArch64)

These enable hardware integer division, NEON SIMD, and hardware floating-point for 10-30% performance improvements.

### Upstream Fixes Integrated

Ported bug fixes from MinUI:

- Miyoo Mini Flip USB audio and lid detection
- rg35xxplus poweroff fix
- zero28 sleep fix
- tg5040 limbo fix
- my355 analog stick fix

---

## Branding

- Renamed from MinUI to LessUI
- New boot assets and UI graphics
- Font changed to Inter Tight (was BPreplayBold)

---

_LessUI maintains MinUI's core philosophy of simplicity while improving code quality, adding modern features, and enabling sustainable long-term development through comprehensive testing._
