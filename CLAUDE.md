# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LessUI is a focused, custom launcher and libretro frontend for retro handheld gaming devices. It provides a simple, distraction-free interface for playing retro games across multiple hardware platforms (Miyoo Mini, Trimui Smart, Anbernic RG35xx series, etc.).

**Key Design Philosophy:**
- Simplicity: No configuration, no boxart, no themes
- Cross-platform: Single SD card works across multiple devices
- Consistency: Same interface and behavior on all supported hardware

## Architecture

### Multi-Platform Build System

LessUI uses a **platform abstraction layer** to support 15+ different handheld devices with a single codebase:

```
workspace/
├── all/                    # Platform-independent code (the core)
│   ├── common/            # Shared utilities (utils.c, api.c, collections.c)
│   ├── minui/             # Launcher + modules (minui.c, minui_*.c)
│   ├── minarch/           # Emulator frontend + modules (minarch.c, minarch_*.c)
│   ├── clock/             # Clock app
│   ├── minput/            # Input configuration
│   └── say/               # Text-to-speech wrapper
│
└── <platform>/            # Platform-specific implementations
    ├── platform/
    │   ├── platform.h     # Hardware definitions (buttons, screen size)
    │   └── makefile.*     # Platform build configuration
    └── keymon/            # Keypress monitoring daemon
```

**Key Concept:** Code in `workspace/all/` is shared across all platforms. Platform-specific details (screen resolution, button mappings, hardware quirks) are defined in each `workspace/<platform>/platform/platform.h`.

### Platform Abstraction Pattern

Each platform defines hardware-specific constants in `platform.h`:

```c
#define PLATFORM "miyoomini"
#define FIXED_WIDTH 640
#define FIXED_HEIGHT 480
#define SDCARD_PATH "/mnt/SDCARD"
#define BUTTON_A    SDL_SCANCODE_SPACE
#define BUTTON_B    SDL_SCANCODE_LCTRL
// ... etc
```

The common code in `workspace/all/common/defines.h` uses these to create derived constants:

```c
#define ROMS_PATH SDCARD_PATH "/Roms"
#define SYSTEM_PATH SDCARD_PATH "/.system/" PLATFORM
```

### Component Responsibilities

**minui** (the launcher):
- File browser for ROMs
- Recently played list
- Tools/apps launcher
- Handles display names (strips region codes, parentheses)

**minarch** (libretro frontend):
- Loads and runs libretro cores
- Save state management (auto-resume on slot 9)
- In-game menu (save states, disc changing, core options)
- Video scaling and filtering

**Common utilities** (`workspace/all/common/`):
- `utils.c` - String manipulation, file I/O, path handling
- `api.c` - Graphics (GFX_*), Sound (SND_*), Input (PAD_*), Power (PWR_*)
- `scaler.c` - NEON-optimized pixel scaling for various screen sizes

## Build System

### Docker-Based Cross-Compilation

LessUI uses Docker containers with platform-specific toolchains to cross-compile for ARM devices:

```bash
# Enter platform build environment
make PLATFORM=miyoomini shell

# Inside docker container, build everything
cd /root/workspace/all/minui
make

# Or build from host (runs docker internally)
make PLATFORM=miyoomini build
```

### Build Process Flow

1. `Makefile` (host) - Orchestrates multi-platform builds
2. `makefile.toolchain` - Launches Docker containers
3. Inside container: Platform makefiles build components
4. `Makefile` target `system` - Copies binaries to `build/` directory
5. `Makefile` target `package` - Creates release ZIP files

### Available Platforms

Active platforms (as of most recent): miyoomini, trimuismart, rg35xx, rg35xxplus, my355, tg5040, zero28, rgb30, m17, my282, magicmini

### Pak Systems

LessUI uses **four systems** for platform-specific `.pak` directories:

```
workspace/all/paks/
├── Tools/                    # Tool paks (Clock, Input, Wifi, etc.)
├── Emus/                     # Emulator paks (template-based)
├── MinUI/                    # System pak (template-based)
└── makefile                  # Build orchestration
```

**1. Tool Paks** (`workspace/all/paks/Tools/`) - Self-contained cross-platform tools:
- Each pak has its own directory with `pak.json`, `launch.sh`, and optional `src/`
- Native code in `src/` is cross-compiled per platform
- Constructed during `make system` (not `make setup`)
- Examples: `Clock/`, `Input/`, `Bootlogo/`, `Files/`, `Wifi/`
- Platform-specific resources supported via `<platform>/` directories
- Hybrid pattern supported (native for some platforms, shell-only for others)

**2. Emulator Paks** (`workspace/all/paks/Emus/`) - Template-based for libretro cores:
- `platforms.json` - Platform metadata (nice prefix, default settings)
- `cores.json` - Core definitions (43 cores, all included in base install)
- `launch.sh.template` - Shared launch script template
- `configs/` - Config templates for all supported cores
- `cores-override/` - Local core zips for development

**3. MinUI System Pak** (`workspace/all/paks/MinUI/`) - Template-based launcher init:
- `platforms/<platform>/` - Each platform has its own directory with all files:
  - `config.sh` - Platform configuration (SDCARD_PATH, CPU speeds, features)
  - `pre-init.sh` - LCD/backlight initialization (optional)
  - `post-env.sh` - Model detection, audio, GPIO setup (optional)
  - `daemons.sh` - Start keymon and platform daemons (optional)
  - `poweroff-handler.sh` - Physical power switch handling (optional)
  - `loop-hook.sh` - Per-iteration hooks like HDMI export (optional)
- Generated by `scripts/generate-minui-pak.sh` during `make setup`
- Output: `build/SYSTEM/<platform>/paks/MinUI.pak/launch.sh`

**4. Direct Paks** (`skeleton/TEMPLATES/paks/`) - Copied as-is for special cases:
- PAK.pak - Native application launcher (copied to all platforms)

**Generation:**
```bash
# All paks generated during setup
make setup  # Generates MinUI paks, emulator paks, and direct paks

# Tool paks constructed during system phase
make build PLATFORM=miyoomini   # Compiles tool pak binaries
make system PLATFORM=miyoomini  # Constructs complete tool paks
```

**Adding a new tool pak:**
1. Create directory `workspace/all/paks/Tools/<Name>/`
2. Create `pak.json` with name, platforms, build type
3. Create `launch.sh` (cross-platform entry point)
4. For native code: create `src/` with source and makefile
5. Test: `make build PLATFORM=miyoomini && make system PLATFORM=miyoomini`

**Adding a new platform to MinUI:**
1. Create `workspace/all/paks/MinUI/platforms/<platform>/` directory
2. Create `config.sh` with platform variables (SDCARD_PATH, CPU config, features)
3. Create optional hook scripts (`pre-init.sh`, `post-env.sh`, `daemons.sh`, etc.)
4. Run `./scripts/generate-minui-pak.sh <platform>` to test
5. Add platform to `pak.json` platforms array

**Adding a new emulator core:**
1. Build core in external [minarch-cores repository](https://github.com/nchapman/minarch-cores)
2. Add to `workspace/all/paks/Emus/cores.json`
3. Create `workspace/all/paks/Emus/configs/base/<CORE>/default.cfg`
4. Run `./scripts/generate-paks.sh all`

See `docs/cross-platform-paks.md` for comprehensive tool pak documentation.

## Development Commands

### macOS Native Development (makefile.dev)

For rapid UI development on macOS, use native builds instead of Docker cross-compilation:

```bash
# First-time setup
brew install sdl2 sdl2_image sdl2_ttf

# Development workflow
make dev           # Build minui for macOS (native, with AddressSanitizer)
make dev-run       # Build and run minui in SDL2 window (4x3 default)
make dev-run-4x3   # Run in 4:3 aspect ratio (640×480)
make dev-run-16x9  # Run in 16:9 aspect ratio (854×480)
make dev-clean     # Clean macOS build artifacts
```

**How it works:**
- Compiles minui natively on macOS using system gcc/clang
- Links against Homebrew SDL2 libraries
- Runs in SDL2 window (640×480 for 4x3, 854×480 for 16x9)
- Uses fake SD card at `workspace/desktop/FAKESD/` instead of actual device storage
- Keyboard input: Arrow keys (D-pad), A/S/W/Q (face buttons), Enter (Start), 4 (Select), Space (Menu)
- Quit: Hold Backspace/Delete

**Setting up test ROMs:**
```bash
# Create console directories
mkdir -p workspace/desktop/FAKESD/Roms/GB
mkdir -p workspace/desktop/FAKESD/Roms/GBA

# Add test ROMs
cp ~/Downloads/game.gb workspace/desktop/FAKESD/Roms/GB/
```

**Use cases:**
- UI iteration (instant feedback vs. SD card deploy)
- Visual testing of menus, text rendering, graphics
- Debugging with sanitizers (-fsanitize=address)
- Integration testing with file I/O and ROM browsing

**Limitations:**
- **minui (launcher) only** - Cannot test minarch (libretro cores)
- Hardware features stubbed (brightness, volume, power management)
- Performance differs from ARM devices
- Path handling: SDCARD_PATH is `../../macos/FAKESD` relative to `workspace/all/minui/` working directory

**Implementation details:**
- Source files: Same as production minui build (from `workspace/all/minui/makefile`)
- Platform code: `workspace/desktop/platform/platform.{h,c}` provides macOS-specific stubs
- Build output: `workspace/all/minui/build/macos/minui` binary
- See `workspace/desktop/FAKESD/README.md` for SD card structure

### Quality Assurance (makefile.qa)

```bash
# Quick commands (recommended)
make test                          # Run all tests (uses Docker)
make lint                          # Run static analysis
make format                        # Format code

# Additional QA targets
make -f makefile.qa docker-shell   # Enter container for debugging
make -f makefile.qa test-native    # Run natively (not recommended on macOS)
make -f makefile.qa lint-full      # Lint entire workspace (verbose)
make -f makefile.qa lint-shell     # Lint shell scripts
make -f makefile.qa format-check   # Check formatting only
make -f makefile.qa clean-tests    # Clean test artifacts
```

**Note:** Tests run in Docker by default, using an Ubuntu 24.04 container. This eliminates macOS-specific build issues and ensures consistency across all development environments.

### Test Organization

Tests follow a **mirror structure** matching the source code:

```
tests/
├── unit/                           # Unit tests (mirrors workspace/)
│   └── all/
│       └── common/
│           └── test_utils.c        # Tests for workspace/all/common/utils.c
├── integration/                    # End-to-end tests
├── fixtures/                       # Test data
└── support/                        # Test infrastructure
    ├── unity/                      # Unity test framework
    └── platform.h                  # Platform stubs for testing
```

**Testing Philosophy:**
- Test `workspace/all/` code (platform-independent)
- Mock external dependencies (SDL, hardware)
- Focus on business logic, not I/O
- See `tests/README.md` for comprehensive guide
- See `docs/testing-checklist.md` for testing roadmap

### Clean Build

To ensure all build artifacts are removed and force a complete rebuild:

```bash
make clean  # Removes all build artifacts (./build, workspace build dirs, boot outputs)
make setup  # Prepares fresh build directory and copies assets
```

The `clean` target removes:
- `./build/` - Final release staging directory
- `workspace/all/*/build/` - Component build directories
- `workspace/*/boot/output/` - Platform boot asset outputs
- Copied boot assets (*.bmp files in workspace)

**Note:** Boot asset generation scripts (`workspace/*/boot/build.sh`) always regenerate output files, even if they exist. This ensures asset updates are always picked up during builds.

### Git Workflow

```bash
# Commit format (see commits.sh)
git commit -m "Brief description.

Detailed explanation if needed."
```

## Important Patterns and Conventions

### String Safety

**CRITICAL:** When manipulating strings where source and destination overlap, use `memmove()` not `strcpy()`:

```c
// WRONG - crashes if tmp points within out_name
strcpy(out_name, tmp);

// CORRECT - safe for overlapping memory
memmove(out_name, tmp, strlen(tmp) + 1);
```

This pattern appears in `getEmuName()` and was the source of a critical bug.

### Display Name Processing

LessUI automatically cleans up ROM filenames for display:
- Removes file extensions (`.gb`, `.nes`, `.p8.png`)
- Strips region codes and version info in parentheses: `Game (USA) (v1.2)` → `Game`
- Trims whitespace
- Handles sorting metadata: `001) Game Name` → `Game Name`

See `getDisplayName()` in `utils.c` for implementation.

### Platform-Specific Code

When adding platform-specific code:

1. **Prefer abstraction** - Add to `workspace/all/common/api.h` with `PLAT_*` prefix
2. **Platform implements** - Each platform provides implementation in their directory
3. **Use weak symbols** - Mark fallback implementations with `FALLBACK_IMPLEMENTATION`

Example:
```c
// In api.h
#define GFX_clear PLAT_clearVideo

// Platform provides PLAT_clearVideo() implementation
// Or uses weak fallback if available
```

### Function Naming Conventions

**MinArch modules use consistent naming:** `MinArch[Module]_functionName`

All extracted MinArch modules follow a standardized naming pattern where the module name appears between `MinArch` and the function name:

| Module | Prefix | Example Functions |
|--------|--------|-------------------|
| minarch_config | `MinArchConfig_` | `MinArchConfig_getValue()`, `MinArchConfig_getPath()` |
| minarch_options | `MinArchOptions_` | `MinArchOptions_find()`, `MinArchOptions_setValue()` |
| minarch_paths | `MinArchPaths_` | `MinArchPaths_getSRAM()`, `MinArchPaths_getState()` |
| minarch_memory | `MinArchMemory_` | `MinArchMemory_readSRAM()`, `MinArchMemory_write()` |
| minarch_state | `MinArchState_` | `MinArchState_read()`, `MinArchState_autoSave()` |
| minarch_utils | `MinArchUtils_` | `MinArchUtils_getCoreName()`, `MinArchUtils_replaceString()` |
| minarch_zip | `MinArchZip_` | `MinArchZip_copy()`, `MinArchZip_inflate()` |
| minarch_input | `MinArchInput_` | `MinArchInput_getButton()`, `MinArchInput_collectButtons()` |
| minarch_core | `MinArchCore_` | `MinArchCore_buildGameInfo()`, `MinArchCore_processAVInfo()` |
| minarch_menu | `MinArchMenu_` | `MinArchMenu_init()`, `MinArchMenuNav_navigate()` |
| minarch_env | `MinArchEnv_` | `MinArchEnv_setRotation()`, `MinArchEnv_handleGeometry()` |
| minarch_cpu | `MinArchCPU_` | `MinArchCPU_update()`, `MinArchCPU_detectFrequencies()` |
| minarch_game | `MinArchGame_` | `MinArchGame_parseExtensions()`, `MinArchGame_detectM3uPath()` |
| minarch_scaler | `MinArchScaler_` | `MinArchScaler_calculate()` |

**Type naming:** Types follow the same pattern with `MinArch[Module]TypeName`:
- `MinArchCPUState`, `MinArchCPUConfig`, `MinArchCPUDecision`
- `MinArchOption`, `MinArchOptionList`
- `MinArchMemoryResult`, `MinArchStateResult`

**Constants:** Module-specific constants use `MINARCH_MODULE_` prefix:
- `MINARCH_CPU_MAX_FREQUENCIES`
- `MINARCH_CPU_DEFAULT_WINDOW_FRAMES`
- `MINARCH_MEM_OK`, `MINARCH_STATE_OK`

This standardization makes it immediately clear which module owns each function and prevents naming collisions as the codebase grows.

### Memory Management

- Stack allocate when size is known and reasonable (< 512 bytes)
- Use `MAX_PATH` (512) for path buffers
- `allocFile()` returns malloc'd memory - **caller must free**
- SDL surfaces are reference counted - use `SDL_FreeSurface()`

### File Paths

All paths use forward slashes (`/`), even for Windows cross-compilation. Platform-specific path construction should use the `*_PATH` macros from `defines.h`:

```c
#define ROMS_PATH SDCARD_PATH "/Roms"
#define USERDATA_PATH SDCARD_PATH "/.userdata/" PLATFORM
#define RECENT_PATH SHARED_USERDATA_PATH "/.minui/recent.txt"
```

### Code Style

- **Tabs for indentation** (not spaces) - TabWidth: 4
- **Braces on same line** - `if (x) {` not `if (x)\n{`
- **Left-aligned pointers** - `char* name` not `char *name`
- **100 character line limit**
- Run `make -f makefile.qa format` before committing

See `.clang-format` for complete style definition.

## Common Gotchas

1. **Platform macros required** - Code in `workspace/all/` needs `PLATFORM`, `SDCARD_PATH`, etc. defined. For testing, use `tests/support/platform.h` stub.

2. **Build in Docker** - Don't try to compile ARM binaries directly on macOS/Linux host. Use `make PLATFORM=<platform> shell`.

3. **Test directory structure** - Tests must mirror source structure for consistency. Create `tests/unit/all/common/test_foo.c` for `workspace/all/common/foo.c`.

4. **libretro-common is third-party** - Don't modify files in `workspace/all/minarch/libretro-common/`. This is upstream code.

5. **Static analysis warnings** - clang-tidy may report warnings about code patterns. Configuration is in `.clang-tidy`. Most warnings for legacy code patterns are already suppressed.

6. **Shell scripts** - Use `.shellcheckrc` configuration for linting. Many legacy scripts have disabled warnings; new scripts should be cleaner.

7. **Editing files with tabs** - This codebase uses **tabs for indentation**. When using the Edit tool, ensure your `old_string` matches the exact whitespace (tabs, not spaces) from the file. If Edit fails with "String to replace not found":
   - Use `sed -n 'X,Yp' file.c | od -c` to see actual whitespace characters (tabs show as `\t`)
   - Copy the exact text from Read tool output (preserving tabs after line numbers)
   - If multiple identical blocks exist, use `replace_all: true` parameter
   - Never use Python scripts or sed for editing - use the Edit or Write tools only

## File Locations Reference

| Purpose | Location |
|---------|----------|
| Main launcher | `workspace/all/minui/minui.c` |
| Libretro frontend | `workspace/all/minarch/minarch.c` |
| Utility functions | `workspace/all/common/utils.c` |
| Platform API | `workspace/all/common/api.c` |
| MinArch internal types | `workspace/all/minarch/minarch_internal.h` |
| MinArch context/state | `workspace/all/minarch/minarch_context.c` |
| MinArch menu system | `workspace/all/minarch/minarch_menu.c` |
| MinArch utilities | `workspace/all/minarch/minarch_utils.c` |
| MinArch config utilities | `workspace/all/minarch/minarch_config.c` |
| MinArch option management | `workspace/all/minarch/minarch_options.c` |
| MinArch ZIP extraction | `workspace/all/minarch/minarch_zip.c` |
| MinArch game file handling | `workspace/all/minarch/minarch_game.c` |
| MinArch video scaler | `workspace/all/minarch/minarch_scaler.c` |
| MinArch core AV processing | `workspace/all/minarch/minarch_core.c` |
| MinArch memory persistence | `workspace/all/minarch/minarch_memory.c` |
| MinArch save states | `workspace/all/minarch/minarch_state.c` |
| MinArch CPU scaling | `workspace/all/minarch/minarch_cpu.c` |
| MinArch input handling | `workspace/all/minarch/minarch_input.c` |
| MinArch save paths | `workspace/all/minarch/minarch_paths.c` |
| MinUI Entry type | `workspace/all/minui/minui_entry.c` |
| MinUI launcher commands | `workspace/all/minui/minui_launcher.c` |
| MinUI state persistence | `workspace/all/minui/minui_state.c` |
| MinUI file utilities | `workspace/all/minui/minui_file_utils.c` |
| MinUI misc utilities | `workspace/all/minui/minui_utils.c` |
| MinUI directory building | `workspace/all/minui/minui_directory.c` |
| MinUI context (state mgmt) | `workspace/all/minui/minui_context.c` |
| MinUI navigation | `workspace/all/minui/minui_navigation.c` |
| Directory indexing | `workspace/all/minui/directory_index.c` |
| Collection parsing | `workspace/all/minui/collection_parser.c` |
| Recent games | `workspace/all/minui/recent_file.c` |
| Effect system | `workspace/all/common/effect_system.c` |
| Platform variant detection | `workspace/all/common/platform_variant.c` |
| Platform definitions | `workspace/<platform>/platform/platform.h` |
| Common definitions | `workspace/all/common/defines.h` |
| Tool paks | `workspace/all/paks/Tools/` |
| Emulator pak templates | `workspace/all/paks/Emus/` |
| MinUI pak templates | `workspace/all/paks/MinUI/` |
| Emulator pak generation | `scripts/generate-paks.sh` |
| MinUI pak generation | `scripts/generate-minui-pak.sh` |
| Test suite | `tests/unit/all/common/` |
| Refactoring guide | `docs/minarch-refactoring.md` |
| Build orchestration | `Makefile` (host-side) |
| QA tools | `makefile.qa` |

## Documentation

- `README.md` - Project overview, supported devices
- `docs/paks-architecture.md` - Tool pak architecture guide
- `docs/creating-paks.md` - Third-party pak creation guide
- `tests/README.md` - Comprehensive testing guide
- `docs/testing-checklist.md` - Testing roadmap and priorities

## Current Test Coverage

**Total: 1253 tests across 41 test suites** ✅

### Extracted and Tested Modules

To enable comprehensive testing, complex logic has been extracted from large files into focused, testable modules:

| Module | Tests | Extracted From | Purpose |
|--------|-------|----------------|---------|
| utils.c (split into 6 modules) | 123 | (original) | String, file, name, date, math utilities |
| minui_thumbnail.c | 72 | minui.c | Thumbnail cache, fade animation, preload hints |
| minarch_env.c | 51 | minarch.c | Libretro environment callback handlers |
| minarch_game.c | 46 | minarch.c | ZIP parsing, extension matching, M3U detection |
| minarch_scaler.c | 26 | minarch.c | Video scaling geometry calculations |
| minarch_core.c | 23 | minarch.c | Core AV info processing, aspect ratio calculation |
| effect_system.c | 43 | platform files | Visual effect state management |
| minarch_cpu.c | 42 | minarch.c | Auto CPU scaling algorithm |
| minarch_utils.c | 41 | minarch.c | Core name extraction, string utilities |
| minarch_menu.c | 41 | minarch.c | In-game menu, context pattern validation |
| nointro_parser.c | 39 | (original) | No-Intro ROM naming conventions |
| directory_index.c | 38 | minui.c | Alias application, duplicate detection, alpha indexing |
| minui_directory.c | 32 | minui.c | Console detection, entry types, collation, scanning |
| minarch_options.c | 36 | minarch.c | Option list search and manipulation |
| pad.c | 36 | api.c | Button state machine, analog input |
| minui_launcher.c | 32 | minui.c | Shell command construction, quote escaping |
| gfx_text.c | 32 | api.c | Text truncation, wrapping, sizing |
| collections.c | 30 | minui.c | Array, Hash data structures |
| str_compare.c | 28 | (original) | Natural string sorting |
| minui_state.c | 27 | minui.c | Path decomposition, collation, resume |
| minui_entry.c | 25 | minui.c | Entry type, array operations, IntArray |
| minarch_input.c | 24 | minarch.c | Input state queries, button mapping |
| map_parser.c | 22 | minui.c/minarch.c | ROM display name aliasing |
| m3u_parser.c | 20 | minui.c | M3U playlist parsing |
| audio_resampler.c | 20 | api.c | Sample rate conversion |
| ui_layout.c | 20 | api.c | UI layout calculations (DP system) |
| minarch_config.c | 19 | minarch.c | Config path generation, option mapping |
| minui_file_utils.c | 18 | minui.c | File/dir checking utilities |
| recent_file.c | 31 | minui.c | Recent games I/O + Recent struct & array ops |
| minui_navigation.c | 30 | minui.c | Navigation logic, entry dispatch, auto-launch, mock context tests |
| minui_utils.c | 17 | minui.c | Index char, console dir detection |
| minarch_memory.c | 16 | minarch.c | SRAM/RTC persistence with mock core |
| minarch_state.c | 16 | minarch.c | Save state read/write, auto-resume |
| minarch_paths.c | 16 | minarch.c | Save file path generation |
| platform_variant.c | 14 | (original) | Platform variant detection |
| minarch_zip.c | 13 | minarch.c | ZIP extraction (copy, deflate) |
| binary_file_utils.c | 12 | minarch.c | Binary file read/write |
| collection_parser.c | 11 | minui.c | Custom ROM list parsing |
| integration_workflows | 22 | - | End-to-end workflow tests |

### Testing Technologies

- **fff (Fake Function Framework)** - Header-only library for mocking SDL functions
- **GCC --wrap** - Link-time file system function mocking (read operations)
- **Real temp files** - For write operations and binary I/O (mkstemp, mkdtemp)
- **Unity** - Test framework with assertions and test runner
- **Docker** - Ubuntu 24.04 container for consistent testing environment

All tests run in Docker by default to ensure consistency across all development environments.

See `tests/README.md` for comprehensive testing guide and examples.
