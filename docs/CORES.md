# LessUI Cores

How libretro cores work in LessUI - their organization, build system, runtime loading, and platform differences.

## What Are Cores?

LessUI uses **libretro cores** to emulate different gaming systems. Libretro is a simple API that allows emulator cores to be used across different frontends. LessUI's libretro frontend is called **minarch** (`workspace/all/minarch/`).

**Core**: A compiled `.so` library implementing a specific emulator (e.g., `gambatte_libretro.so` for Game Boy)

**Frontend**: MinArch loads cores dynamically and provides video/audio/input services

**Platform Abstraction**: Same cores run on all devices with platform-specific compilation flags

**Configuration**: Each emulator is packaged as a `.pak` directory with launch scripts and defaults

## Stock Cores

All platforms ship with these 6 cores in `SYSTEM/<platform>/cores/`:

| Core | Systems | Extensions |
|------|---------|------------|
| fceumm | Nintendo Entertainment System | `.nes`, `.fds`, `.unf` |
| gambatte | Game Boy / Game Boy Color | `.gb`, `.gbc`, `.dmg` |
| gpsp | Game Boy Advance | `.gba` |
| picodrive | Sega Genesis, Sega CD, 32X | `.md`, `.bin`, `.smd`, `.gen` |
| snes9x2005_plus | Super Nintendo | `.smc`, `.sfc`, `.swc`, `.fig` |
| pcsx_rearmed | PlayStation | `.cue`, `.m3u`, `.pbp`, `.chd` |

## Extra Cores

These 7 additional cores can be installed from `EXTRAS/Emus/`:

| Core | System | Extensions |
|------|--------|------------|
| mgba | Game Boy Advance (cycle-accurate) | `.gba` |
| fake-08 | PICO-8 | `.p8`, `.p8.png` |
| mednafen_pce_fast | TurboGrafx-16 / PC Engine | `.pce`, `.cue` |
| pokemini | Pokémon Mini | `.min` |
| race | Neo Geo Pocket / Neo Geo Pocket Color | `.ngp`, `.ngc` |
| mednafen_supafaust | Super Nintendo (accuracy) | `.sfc`, `.smc` |
| mednafen_vb | Virtual Boy | `.vb` |

## Directory Organization

Each platform has its own cores directory with a consistent structure:

```
workspace/<platform>/cores/
├── makefile              # Platform-specific core list and build config
├── patches/              # Platform-specific patches for cores
│   ├── gpsp.patch       # Adds platform definition to core's Makefile
│   ├── gambatte.patch
│   ├── fceumm.patch
│   └── ...
├── src/                  # Git clones of libretro core repositories (gitignored)
│   ├── fceumm/
│   ├── gambatte/
│   ├── gpsp/
│   └── ...
└── output/               # Compiled .so files (gitignored)
    ├── fceumm_libretro.so
    ├── gambatte_libretro.so
    └── ...
```

## Platform Differences

Different platforms build different sets of cores:

**Miyoo Mini** - 13 cores:
```makefile
CORES = fceumm gambatte gpsp pcsx_rearmed picodrive snes9x2005_plus
CORES+= mednafen_pce_fast mednafen_vb fake-08 mednafen_supafaust mgba pokemini race
```

**Trimui Smart** - 12 cores (fake-08 commented out by default):
```makefile
CORES = fceumm gambatte gpsp pcsx_rearmed picodrive snes9x2005_plus
CORES+= mednafen_pce_fast mednafen_vb mgba pokemini race mednafen_supafaust
```

**RG35XX Plus** - Reuses cores from RG35XX:
```makefile
# Uses cores from rg35xx to reduce build time
copy:
	cp -R ../rg35xx/cores/output ./
```
**Strategy**: Reuses cores from base platform variant

### Shared Patches

In addition to platform-specific patches, there are shared patches that apply to all platforms:

```
workspace/all/cores/patches/
└── gambatte/
    └── 001-export-dmg-grid-color-on-change.patch
```

These patches add LessUI-specific features (like DMG palette color export for color palettes).

---

## Build System

### Two-Tier Makefile Architecture

LessUI uses a templated build system with platform-specific and shared components:

#### 1. Platform-Specific Makefile

Location: `workspace/<platform>/cores/makefile`

```makefile
# Define which cores to build for this platform
CORES = fceumm gambatte gpsp pcsx_rearmed picodrive snes9x2005_plus
CORES+= mednafen_pce_fast mednafen_vb fake-08 mednafen_supafaust mgba pokemini race

# Optional per-core configuration

# Override repository URL
mednafen_pce_fast_REPO = https://github.com/libretro/beetle-pce-fast-libretro
mednafen_vb_REPO = https://github.com/libretro/beetle-vb-libretro

# Override output filename
fake-08_REPO = https://github.com/jtothebell/fake-08
fake-08_CORE = fake08_libretro.so

# Override build directory
fake-08_BUILD_PATH = fake-08/platform/libretro

# Specify custom makefile
pcsx_rearmed_MAKEFILE = Makefile.libretro

# Add build flags
snes9x2005_plus_FLAGS = USE_BLARGG_APU=1

# Include shared build logic
include ../../all/cores/makefile
```

#### 2. Shared Makefile Template

Location: `workspace/all/cores/makefile`

This file implements the actual build logic using Make's template system:

```makefile
# Template for building each core
define TEMPLATE=
# Default repository URL (can be overridden)
$1_REPO ?= https://github.com/libretro/$(1)

# Build command with platform flag
$1_MAKE ?= make $$(and $$($1_MAKEFILE),-f $$($1_MAKEFILE)) \
    platform=$(PLATFORM) $$($(1)_FLAGS)

# Clone repository
src/$(1):
	git clone $$(if $$($1_HASH),,--depth 1) --recursive $$($1_REPO) $(1)

# Apply platform-specific patch
src/$(1)/.patched: src/$(1)
	cd src/$(1) && $(PATCH) -p1 < ../../patches/$(1).patch && touch .patched

# Apply shared patches (from workspace/all/cores/patches/)
src/$(1)/.patched-all: src/$(1)
	cd src/$(1) && $$(foreach patch, $$(wildcard ../../all/cores/patches/$(1)/*.patch), \
		$(PATCH) -p1 < ../../$$(patch) &&) touch .patched-all

# Build core
output/$(1)_libretro.so: src/$(1)/.patched src/$(1)/.patched-all
	cd src/$$($1_BUILD_PATH) && $$($1_MAKE) $(PROCS)
	mv src/$$($1_BUILD_PATH)/$$(if $$($(1)_CORE),$$($(1)_CORE),$(1)_libretro.so) ./output
endef

# Generate build targets for all cores
$(foreach CORE,$(CORES),$(eval $(call TEMPLATE,$(CORE))))
```

### Build Process

The build process follows these steps:

1. **Clone** upstream libretro core repository
2. **Apply platform-specific patch** (adds platform definition to core's Makefile)
3. **Apply shared patches** (LessUI-specific features)
4. **Cross-compile** using Docker toolchain with `platform=<platform>` flag
5. **Copy** compiled `.so` file to `output/` directory

Example build command:
```bash
# Enter platform build environment
make PLATFORM=miyoomini shell

# Inside Docker container, build all cores
cd /root/workspace/miyoomini/cores
make

# Build specific core
make output/gambatte_libretro.so
```

### Patching System

Patches serve two purposes:

#### Platform-Specific Patches

These add platform definitions to each core's Makefile. Example from `miyoomini/cores/patches/gpsp.patch`:

```diff
+# MIYOOMINI
+else ifeq ($(platform), miyoomini)
+    TARGET := $(TARGET_NAME)_libretro.so
+    CC = $(CROSS_COMPILE)gcc
+    CXX = $(CROSS_COMPILE)g++
+    CFLAGS += -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
+    HAVE_NEON = 1
+    MMAP_JIT_CACHE = 1
+    HAVE_DYNAREC = 1
```

This tells the core:
- What compiler to use (`CROSS_COMPILE` is set by Docker environment)
- What CPU optimizations to enable (NEON, ARM v7, hard float)
- What features to enable (dynarec, JIT cache)

#### Shared Patches

These add LessUI-specific features across all platforms. Example: `workspace/all/cores/patches/gambatte/001-export-dmg-grid-color-on-change.patch` adds support for exporting palette color changes for color palette swapping.

### Cross-Compilation Toolchain

- **Docker-based**: Each platform has a Docker image with ARM cross-compilation toolchain
- **Compiler**: Uses `CROSS_COMPILE` environment variable (e.g., `arm-linux-gnueabihf-gcc`)
- **Platform flag**: Cores are built with `platform=<platform>` Make variable
- **Optimization**: Platform-specific compiler flags in patches enable hardware features

---

## Runtime Core Loading

### Core Structure in Memory

When MinArch loads a core, it creates a global `core` structure:

```c
static struct Core {
    // State
    int initialized;
    int need_fullpath;  // Core requires file path vs ROM data

    // Metadata
    const char tag[8];          // Platform tag: "GB", "NES", "PS"
    const char name[128];       // Core name: "gambatte", "fceumm"
    const char version[128];    // e.g., "Gambatte (v0.5.0)"
    const char extensions[128]; // "gb|gbc|dmg"

    // Directory paths
    const char config_dir[MAX_PATH];  // Platform-specific config
    const char states_dir[MAX_PATH];  // Save states (architecture-shared)
    const char saves_dir[MAX_PATH];   // SRAM saves
    const char bios_dir[MAX_PATH];    // BIOS files

    // A/V parameters
    double fps;
    double sample_rate;
    double aspect_ratio;

    // Dynamic library handle
    void* handle;  // dlopen() handle

    // Libretro API function pointers (20+ functions)
    void (*init)(void);
    void (*deinit)(void);
    void (*get_system_info)(struct retro_system_info* info);
    void (*run)(void);  // Run one frame
    size_t (*serialize_size)(void);
    bool (*serialize)(void* data, size_t size);
    bool (*unserialize)(const void* data, size_t size);
    bool (*load_game)(const struct retro_game_info* game);
    // ... etc
} core;
```

### Core Loading Process

Function: `Core_open()` in `workspace/all/minarch/minarch.c:3399`

```c
void Core_open(const char* core_path, const char* tag_name) {
    // 1. Load .so file
    core.handle = dlopen(core_path, RTLD_LAZY);

    // 2. Resolve all libretro API functions (20+ function pointers)
    core.init = dlsym(core.handle, "retro_init");
    core.deinit = dlsym(core.handle, "retro_deinit");
    core.get_system_info = dlsym(core.handle, "retro_get_system_info");
    core.run = dlsym(core.handle, "retro_run");
    core.serialize = dlsym(core.handle, "retro_serialize");
    // ... etc

    // 3. Set up callbacks (core calls these during emulation)
    set_environment_callback = dlsym(core.handle, "retro_set_environment");
    set_environment_callback(environment_callback);
    set_video_refresh_callback(video_refresh_callback);
    set_audio_sample_batch_callback(audio_sample_batch_callback);
    set_input_poll_callback(input_poll_callback);

    // 4. Get core metadata
    struct retro_system_info info = {};
    core.get_system_info(&info);
    Core_getName(core_path, core.name);  // Extract "gambatte" from path
    sprintf(core.version, "%s (%s)", info.library_name, info.library_version);
    strcpy(core.tag, tag_name);  // e.g., "GB"
    strcpy(core.extensions, info.valid_extensions);

    // 5. Set up directories
    sprintf(core.config_dir, USERDATA_PATH "/%s-%s", core.tag, core.name);
    sprintf(core.states_dir, SHARED_USERDATA_PATH "/%s-%s", core.tag, core.name);
    sprintf(core.saves_dir, SDCARD_PATH "/Saves/%s", core.tag);
    sprintf(core.bios_dir, SDCARD_PATH "/Bios/%s", core.tag);

    // 6. Create directories
    system("mkdir -p \"$config_dir\"; mkdir -p \"$states_dir\"");
}
```

### Directory Path Examples

For Game Boy with Gambatte core on Miyoo Mini:

| Purpose | Path | Shared Across Platforms? |
|---------|------|--------------------------|
| Config | `/mnt/SDCARD/.userdata/miyoomini/GB-gambatte/` | No (platform-specific) |
| Save States | `/mnt/SDCARD/.userdata/shared/GB-gambatte/` | **Yes** (architecture-compatible) |
| SRAM Saves | `/mnt/SDCARD/Saves/GB/` | Yes (universal format) |
| BIOS | `/mnt/SDCARD/Bios/GB/` | Yes (universal format) |

**Key Insight**: Save states are stored in `.userdata/shared/` so they work across all platforms with the same CPU architecture!

### Game Loading Lifecycle

```c
// 1. Initialize core
Core_init() {
    core.init();
    core.initialized = 1;
}

// 2. Load game
Core_load() {
    struct retro_game_info game_info;
    game_info.path = game.path;
    game_info.data = game.data;  // NULL if need_fullpath=true
    game_info.size = game.size;

    core.load_game(&game_info);

    SRAM_read();  // Load battery saves
    RTC_read();   // Load real-time clock data

    // Get A/V info AFTER loading game (some cores need game loaded first)
    core.get_system_av_info(&av_info);
    core.fps = av_info.timing.fps;
    core.aspect_ratio = av_info.geometry.aspect_ratio;
}

// 3. Main emulation loop
while (!quit) {
    core.run();  // Emulate one frame

    // Core calls these callbacks during run():
    // - video_refresh_callback(frame_data, width, height, pitch)
    // - audio_sample_batch_callback(audio_data, frame_count)
}

// 4. Cleanup
Core_quit() {
    SRAM_write();  // Save battery RAM
    RTC_write();   // Save real-time clock
    core.unload_game();
    core.deinit();
}

Core_close() {
    dlclose(core.handle);
}
```

---

## Configuration System

### .pak Directory Structure

Each emulator is packaged as a `.pak` directory:

```
GB.pak/
├── launch.sh       # Shell script that launches minarch with core
└── default.cfg     # Default core options and input mappings
```

These directories live in:
- **Stock emulators**: `SYSTEM/<platform>/paks/Emus/GB.pak/`
- **Extra emulators**: `EXTRAS/Emus/<platform>/GB.pak/`

### Launch Script

Example: `GB.pak/launch.sh`

```bash
#!/bin/sh

EMU_EXE=gambatte  # Core library name (becomes gambatte_libretro.so)

# Extract tag from .pak directory name (GB.pak -> GB)
EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"

# Set up directories
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
HOME="$USERDATA_PATH"
cd "$HOME"

# Launch minarch with core and ROM
nice -20 minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" \
    &> "$LOGS_PATH/$EMU_TAG.txt"
```

Environment variables (set by `LessUI.pak/launch.sh`):
- `PLATFORM`: `"miyoomini"`, `"trimuismart"`, `"rgb30"`, etc.
- `SDCARD_PATH`: `"/mnt/SDCARD"`
- `SYSTEM_PATH`: `"$SDCARD_PATH/.system/$PLATFORM"`
- `CORES_PATH`: `"$SYSTEM_PATH/cores"`
- `USERDATA_PATH`: `"$SDCARD_PATH/.userdata/$PLATFORM"`
- `SHARED_USERDATA_PATH`: `"$SDCARD_PATH/.userdata/shared"`
- `BIOS_PATH`: `"$SDCARD_PATH/Bios"`
- `SAVES_PATH`: `"$SDCARD_PATH/Saves"`

### Default Configuration

Example: `GB.pak/default.cfg`

```ini
# MinArch settings (frontend)
minarch_screen_scaling = Native

# Core-specific options (passed to gambatte core)
gambatte_gb_colorization = internal
gambatte_gb_internal_palette = TWB64 - Pack 1
gambatte_gb_palette_twb64_1 = TWB64 038 - Pokemon mini Ver.
gambatte_gb_bootloader = disabled

# Input mappings (libretro button -> platform button)
bind Up = UP
bind Down = DOWN
bind A Button = A
bind B Button = B
bind A Turbo = NONE:X
bind B Turbo = NONE:Y
bind Prev. Palette = NONE:L1
bind Next Palette = NONE:R1
```

### Configuration Hierarchy

MinArch uses a three-tier configuration system:

1. **System-wide defaults** (`SYSTEM/<platform>/system.cfg`):
   ```ini
   -minarch_screen_sharpness = Crisp
   -minarch_prevent_tearing = Lenient
   -minarch_thread_video = Off
   ```
   (The `-` prefix means "hidden from in-game menu")

2. **Emulator defaults** (`.pak/default.cfg`):
   - Core-specific options
   - Input bindings
   - Screen scaling preferences

3. **User overrides** (`.userdata/<platform>/<tag>-<core>/`):
   - Saved per-game or globally
   - Takes precedence over all defaults

---

## Platform Differences

### Build-Time Differences

Different platforms use different CPU optimization flags in their patches:

**Miyoo Mini** (Cortex-A7, NEON):
```makefile
CFLAGS += -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
```

**Trimui Smart** (Cortex-A7, NEON):
```makefile
CFLAGS += -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -mcpu=cortex-a7
```
(Note: Uses `-mcpu` instead of `-march` - slightly different approach)

**RGB30** (Different SoC):
- May use different ARM optimization flags
- May have different hardware accelerations available

### Core Selection Differences

**Stock cores** (6 cores across all platforms):
- fceumm, gambatte, gpsp, picodrive, snes9x2005_plus, pcsx_rearmed
- Installed by default in `SYSTEM/<platform>/cores/`

**Extra cores** (varies by platform):
- Most platforms: 7 extra cores (mgba, fake-08, mednafen_pce_fast, pokemini, race, mednafen_supafaust, mednafen_vb)
- Some platforms exclude certain cores:
  - Trimui Smart: Comments out fake-08 by default
  - GKD Pixel: Skips fake-08 entirely

### Runtime Path Differences

All paths are constructed from environment variables, allowing the same SD card to work on multiple devices:

```
/mnt/SDCARD/
├── .system/
│   ├── miyoomini/          # Miyoo Mini binaries
│   │   ├── cores/
│   │   └── paks/
│   ├── trimuismart/        # Trimui Smart binaries
│   │   ├── cores/
│   │   └── paks/
│   └── rgb30/              # RGB30 binaries
│       ├── cores/
│       └── paks/
├── .userdata/
│   ├── miyoomini/          # Miyoo Mini-specific data
│   ├── trimuismart/        # Trimui Smart-specific data
│   ├── rgb30/              # RGB30-specific data
│   └── shared/             # Shared save states (architecture-compatible!)
├── Roms/                   # Universal ROM storage
├── Saves/                  # Universal save files
└── Bios/                   # Universal BIOS files
```

**Result**: Insert the same SD card in a Miyoo Mini or RGB30, and it "just works" - each device uses its own binaries but shares your games, saves, and save states!

---

## Libretro API Integration

### The Libretro Interface

Header: `workspace/all/minarch/libretro-common/include/libretro.h`

Libretro defines a **callback-based API** between cores (emulators) and frontends (MinArch):

### Core Functions (Exported by .so)

These functions are implemented by the core and called by MinArch:

```c
void retro_init(void);
void retro_deinit(void);
void retro_get_system_info(struct retro_system_info* info);
bool retro_load_game(const struct retro_game_info* game);
void retro_unload_game(void);
void retro_run(void);  // Emulate one frame

// Save states
size_t retro_serialize_size(void);
bool retro_serialize(void* data, size_t size);
bool retro_unserialize(const void* data, size_t size);

// Core options
void retro_set_environment(retro_environment_t callback);
void retro_set_video_refresh(retro_video_refresh_t callback);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback);
void retro_set_input_poll(retro_input_poll_t callback);
void retro_set_input_state(retro_input_state_t callback);
```

### Frontend Callbacks (Provided by MinArch)

These callbacks are implemented by MinArch and called by the core during `retro_run()`:

```c
// Video: Core calls this to display a frame
typedef void (*retro_video_refresh_t)(
    const void* data,      // Frame buffer (RGB565 in LessUI)
    unsigned width,
    unsigned height,
    size_t pitch           // Bytes per scanline
);

// Audio: Core calls this to output audio samples
typedef size_t (*retro_audio_sample_batch_t)(
    const int16_t* data,   // Stereo interleaved samples
    size_t frames          // Number of stereo frames
);

// Input: Core calls these to check button states
typedef void (*retro_input_poll_t)(void);
typedef int16_t (*retro_input_state_t)(
    unsigned port,         // Player number (0-3)
    unsigned device,       // Controller type
    unsigned index,        // Sub-device index
    unsigned id            // Button/axis ID
);
```

### Environment Callback

The environment callback is the **core's way to communicate with the frontend**:

```c
static bool environment_callback(unsigned cmd, void* data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        // Core asks: "Where are my BIOS files?"
        *(const char**)data = core.bios_dir;
        return true;

    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        // Core asks: "Where should I save SRAM?"
        *(const char**)data = core.saves_dir;
        return true;

    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        // Core says: "I want to output RGB565"
        if (*(enum retro_pixel_format*)data != RETRO_PIXEL_FORMAT_RGB565)
            return false;  // LessUI only supports RGB565!
        return true;

    case RETRO_ENVIRONMENT_GET_VARIABLE:
        // Core asks: "What's the value of this option?"
        struct retro_variable* var = (struct retro_variable*)data;
        var->value = OptionList_getOptionValue(&config.core, var->key);
        return true;

    case RETRO_ENVIRONMENT_SET_VARIABLES:
        // Core says: "Here's my list of options"
        const struct retro_variable* vars = (const struct retro_variable*)data;
        OptionList_parseOptions(&config.core, vars);
        return true;

    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        // Core says: "Here's what my buttons are called"
        const struct retro_input_descriptor* desc = data;
        Input_init(desc);
        return true;

    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        // Core says: "I support disc changing" (for PSX multi-disc games)
        const struct retro_disk_control_callback* disk = data;
        Disk_init(disk);
        return true;
    }
}
```

### Core-Frontend Communication Flow

```
[Core .so file]                    [MinArch Frontend]
     |                                     |
     |<--------- dlopen() -----------------|
     |                                     |
     |-- retro_set_environment() --------->| (register environment callback)
     |-- retro_set_video_refresh() ------->| (register video callback)
     |-- retro_set_audio_sample_batch() -->| (register audio callback)
     |-- retro_set_input_poll() ---------->| (register input poll callback)
     |-- retro_set_input_state() --------->| (register input state callback)
     |                                     |
     |<-------- retro_init() --------------|
     |                                     |
     |-- environment_callback() ---------->| (query capabilities, options)
     |<-------- [data] --------------------|
     |                                     |
     |<-------- retro_load_game() ---------|
     |                                     |
  [Main Loop]                              |
     |<-------- retro_run() ---------------|
     |                                     |
     |-- video_refresh(frame_data) ------->| (display frame)
     |-- audio_sample_batch(audio) ------->| (play audio)
     |-- input_poll() -------------------->| (sample input)
     |-- input_state(port, button) ------->| (check button state)
     |                                     |
  [User saves state]                       |
     |<-------- retro_serialize() ---------|
     |                                     |
  [User loads state]                       |
     |<-------- retro_unserialize() -------|
     |                                     |
  [User quits]                             |
     |<-------- retro_unload_game() -------|
     |<-------- retro_deinit() ------------|
     |                                     |
     |<--------- dlclose() ----------------|
```

### Key LessUI-Specific Requirements

1. **Pixel Format**: LessUI **only** supports RGB565 (16-bit color)
   - Cores that output RGB888 or XRGB8888 are rejected
   - This is for performance on low-end ARM devices

2. **Sample Rate**: LessUI resamples audio to platform-native sample rate
   - Most cores output 32040 Hz or 48000 Hz
   - LessUI's audio resampler converts to device sample rate (often 44100 Hz)

3. **Save State Paths**: LessUI manages save state paths
   - Slot 9 is special: auto-resume slot (saved on quit, loaded on launch)
   - Slots 0-8 are user-accessible save slots

4. **Fast-Forward**: LessUI implements fast-forward by calling `retro_run()` multiple times per frame
   - Skips video refresh callback for intermediate frames
   - Plays audio at accelerated rate

---

## Key Files Reference

### Build System

| File | Purpose |
|------|---------|
| `Makefile` | Main orchestration makefile |
| `workspace/all/cores/makefile` | Shared core build logic (templated) |
| `workspace/<platform>/cores/makefile` | Platform-specific core list and overrides |
| `workspace/<platform>/cores/patches/*.patch` | Platform-specific patches for cores |
| `workspace/all/cores/patches/<core>/*.patch` | Shared patches across all platforms |

### Core Loading and Management

| File | Lines | Purpose |
|------|-------|---------|
| `workspace/all/minarch/minarch.c` | 3399-3518 | `Core_open()` - Dynamic loading of .so files |
| `workspace/all/minarch/minarch.c` | 2241-2680 | `environment_callback()` - Core-frontend communication |
| `workspace/all/minarch/minarch.c` | 3951-4052 | `Core_init()`, `Core_load()` - Initialization |
| `workspace/all/minarch/minarch.c` | 4530-4580 | Main loop - calls `core.run()` |

### Libretro API

| File | Purpose |
|------|---------|
| `workspace/all/minarch/libretro-common/include/libretro.h` | Official libretro API specification |

### Configuration

| File | Purpose |
|------|---------|
| `skeleton/SYSTEM/<platform>/paks/Emus/<tag>.pak/launch.sh` | Launch script for emulator |
| `skeleton/SYSTEM/<platform>/paks/Emus/<tag>.pak/default.cfg` | Default options and bindings |
| `skeleton/SYSTEM/<platform>/system.cfg` | System-wide frontend defaults |

### Utilities

| File | Lines | Purpose |
|------|-------|---------|
| `workspace/all/common/utils.c` | 429-467 | `getEmuName()` - Extract emulator tag from ROM path |
| `workspace/all/common/utils.c` | 469-525 | `getEmuPath()` - Find .pak directory for emulator |

### Platform Definitions

| File | Purpose |
|------|---------|
| `workspace/<platform>/platform/platform.h` | Hardware constants (buttons, screen size) |
| `workspace/all/common/defines.h` | Derived path constants |

## Adding a Core to LessUI

Quick steps for adding a new core to the official LessUI distribution:

1. **Decide placement** - Stock (base install) or extras (optional download)
2. **Add to platform makefiles** - Edit `workspace/<platform>/cores/makefile`
   - Add core name to `CORES` list
   - Set repo URL, build path, or flags if non-standard
3. **Create platform patches** - Add `workspace/<platform>/cores/patches/<core>.patch`
   - Adds platform definition to core's Makefile
   - Sets compiler flags (CPU, NEON, dynarec, etc.)
4. **Build and test** - Use Docker to compile for each platform
   - `make PLATFORM=<platform> shell` then `cd cores && make`
   - Verify core builds without errors
5. **Add to skeleton** - Create `.pak` in `skeleton/SYSTEM/<platform>/paks/Emus/` (stock) or `skeleton/EXTRAS/Emus/` (optional)
   - `launch.sh` - Launches minarch with core
   - `default.cfg` - Core options and input bindings
6. **Test on hardware** - Verify save states, auto-resume, and in-game menu work correctly
7. **Repeat for other platforms** - Each platform needs patches and paks

Core quality matters: Cores should reliably support save states, have acceptable performance, and integrate cleanly with LessUI's features (auto-resume, in-game menu, consistent behavior).

## Potential Improvements

Some areas that could be simplified or improved:

**Build System**
- Templated makefile system is powerful but hard to debug
- Maintaining patches for 13+ cores across platforms is tedious
- Upstream libretro changes can break patches
- Could use wrapper makefiles or build scripts instead

**Core Discovery**
- Hardcoded lists in platform makefiles
- Could auto-discover cores or use metadata files

**Configuration**
- Three-tier system works but could be more explicit
- Better validation and error reporting would help

**Environment Callback**
- Large switch statement (440 lines in `minarch.c:2241-2680`)
- Could split into separate handler functions

**Core Metadata**
- Extracted from core at runtime
- Pre-generating could speed up startup

**Multi-Core Support**
- Some systems have multiple cores (e.g., mgba vs gpsp for GBA)
- Currently requires duplicate .pak directories
- Could use a core selection system

## Resources

- [Architecture Guide](ARCHITECTURE.md) - How LessUI works internally
- [Development Guide](DEVELOPMENT.md) - Building and testing
- [Pak Development](PAKS.md) - Creating custom emulator paks
- [CLAUDE.md](../CLAUDE.md) - Comprehensive technical reference
