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
│   ├── common/            # Utilities (utils.c, api.c)
│   ├── minui/             # Launcher UI (1,704 lines)
│   ├── minarch/           # Libretro frontend (4,830 lines)
│   ├── clock/             # Clock app
│   ├── minput/            # Input configuration
│   └── say/               # Text-to-speech wrapper
│
└── <platform>/            # Platform-specific implementations
    ├── platform/
    │   ├── platform.h     # Hardware definitions (buttons, screen size)
    │   └── makefile.*     # Platform build configuration
    ├── cores/             # Libretro cores for this platform
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

Active platforms (as of most recent): miyoomini, trimuismart, rg35xx, rg35xxplus, my355, tg5040, zero28, rgb30, m17, gkdpixel, my282, magicmini

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
- Uses fake SD card at `workspace/macos/FAKESD/` instead of actual device storage
- Keyboard input: Arrow keys (D-pad), A/S/W/Q (face buttons), Enter (Start), 4 (Select), Space (Menu)
- Quit: Hold Backspace/Delete

**Setting up test ROMs:**
```bash
# Create console directories
mkdir -p workspace/macos/FAKESD/Roms/GB
mkdir -p workspace/macos/FAKESD/Roms/GBA

# Add test ROMs
cp ~/Downloads/game.gb workspace/macos/FAKESD/Roms/GB/
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
- Platform code: `workspace/macos/platform/platform.{h,c}` provides macOS-specific stubs
- Build output: `workspace/all/minui/build/macos/minui` binary
- See `workspace/macos/FAKESD/README.md` for SD card structure

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

**Note:** Tests run in Docker by default, using a Debian Buster ARM64 container that matches the platform toolchains (GCC 8.3.0). This eliminates macOS-specific build issues and ensures consistency with the actual build environment.

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

5. **Static analysis warnings** - cppcheck may warn about unknown macros (e.g., `PLATFORM`). This is expected. Use suppressions in `.cppcheck-suppressions` if needed.

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
| Platform definitions | `workspace/<platform>/platform/platform.h` |
| Common definitions | `workspace/all/common/defines.h` |
| Test suite | `tests/unit/all/common/test_utils.c` |
| Build orchestration | `Makefile` (host-side) |
| QA tools | `makefile.qa` |

## Documentation

- `README.md` - Project overview, supported devices
- `tests/README.md` - Comprehensive testing guide
- `docs/testing-checklist.md` - Testing roadmap and priorities
- `docs/parallel-builds-analysis.md` - Build system analysis

## Current Test Coverage

**Total: 342 tests across 15 test suites** ✅

### Extracted and Tested Modules

To enable comprehensive testing, complex logic has been extracted from large files into focused, testable modules:

| Module | Tests | Extracted From | Purpose |
|--------|-------|----------------|---------|
| utils.c (split into 6 modules) | 100 | (original) | String, file, name, date, math utilities |
| pad.c | 21 | api.c | Button state machine, analog input |
| collections.c | 30 | minui.c | Array, Hash data structures |
| gfx_text.c | 32 | api.c | Text truncation, wrapping, sizing |
| audio_resampler.c | 18 | api.c | Bresenham sample rate conversion |
| minarch_paths.c | 16 | minarch.c | Save file path generation |
| minui_utils.c | 17 | minui.c | Index char, console dir detection |
| m3u_parser.c | 20 | minui.c | M3U playlist parsing |
| minui_file_utils.c | 25 | minui.c | File/dir checking utilities |
| map_parser.c | 22 | minui.c/minarch.c | ROM display name aliasing |
| collection_parser.c | 11 | minui.c | Custom ROM list parsing |
| recent_file.c | 18 | minui.c | Recent games read/write |
| binary_file_utils.c | 12 | minarch.c | Binary file read/write |

### Testing Technologies

- **fff (Fake Function Framework)** - Header-only library for mocking SDL functions
- **GCC --wrap** - Link-time file system function mocking (read operations)
- **Real temp files** - For write operations and binary I/O (mkstemp, mkdtemp)
- **Unity** - Test framework with assertions and test runner
- **Docker** - Debian Buster ARM64 container matching platform toolchains (GCC 8.3.0)

All tests run in Docker by default to ensure consistency with the cross-compilation environment.

See `tests/README.md` for comprehensive testing guide and examples.
