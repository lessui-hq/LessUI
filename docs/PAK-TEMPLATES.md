# Pak Template System

As of the prebuilt cores migration, LessUI uses a **template-based system** to generate platform-specific `.pak` directories. This eliminates duplication and makes it easy to add new cores or platforms.

## Overview

**Problem**: Previously, each core's `.pak` directory was duplicated across 12 platforms (111 pak directories total), with only minor variations between them.

**Solution**: Single source of truth in `skeleton/TEMPLATES/` that generates all platform-specific paks during the build process.

## Directory Structure

```
skeleton/
├── TEMPLATES/
│   ├── minarch-paks/           # Template system for libretro core paks
│   │   ├── platforms.json      # Platform metadata (nice prefix, default settings)
│   │   ├── cores.json          # Core definitions (emu_exe, bundled status)
│   │   ├── launch.sh.template  # Launch script template (shared by all cores)
│   │   └── configs/            # Config templates (one per core)
│   │       ├── GB.cfg
│   │       ├── GBA.cfg
│   │       ├── VB.cfg
│   │       └── ...
│   └── paks/                   # Direct paks (copied as-is to all platforms)
│       └── PAK.pak/
│           └── launch.sh
└── SYSTEM/
    └── (generated during build)
```

## How It Works

### 1. Platform Metadata (`platforms.json`)

Defines platform-specific variations:

```json
{
  "platforms": {
    "miyoomini": {
      "nice_prefix": "nice -20 ",
      "default_minarch_setting": "minarch_screen_scaling = Native"
    },
    "tg5040": {
      "nice_prefix": "",
      "default_minarch_setting": "minarch_cpu_speed = Powersave"
    }
  }
}
```

**Fields:**
- `nice_prefix`: CPU priority prefix for `minarch.elf` (some platforms need `nice -20`, others don't)
- `default_minarch_setting`: First line of `default.cfg` (platform-specific performance setting)

### 2. Core Definitions (`cores.json`)

Defines which cores to build and their properties:

```json
{
  "stock_cores": {
    "FC": {
      "emu_exe": "fceumm"
    },
    "GB": {
      "emu_exe": "gambatte"
    },
    "GBA": {
      "emu_exe": "gpsp"
    }
  },
  "extra_cores": {
    "VB": {
      "emu_exe": "mednafen_vb",
      "bundled_core": true
    },
    "N64": {
      "emu_exe": "mupen64plus_next",
      "bundled_core": true
    }
  }
}
```

**Fields:**
- `emu_exe`: Core library name (becomes `${emu_exe}_libretro.so`)
- `bundled_core`: (optional) If true, sets `CORES_PATH=$(dirname "$0")` to use bundled core from pak directory

**Core Types:**
- `stock_cores`: Installed in `SYSTEM/<platform>/paks/Emus/` (base install)
- `extra_cores`: Installed in `EXTRAS/Emus/<platform>/` (optional download)

### 3. Launch Script Template (`launch.sh.template`)

Template with placeholders:

```bash
#!/bin/sh

EMU_EXE={{EMU_EXE}}
{{CORES_PATH_OVERRIDE}}
###############################

EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
HOME="$USERDATA_PATH"
cd "$HOME"
{{NICE_PREFIX}}minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" &> "$LOGS_PATH/$EMU_TAG.txt"
```

**Placeholders:**
- `{{EMU_EXE}}`: Replaced with core name (e.g., `gambatte`)
- `{{NICE_PREFIX}}`: Replaced with `nice -20 ` or empty string
- `{{CORES_PATH_OVERRIDE}}`: Replaced with `CORES_PATH=$(dirname "$0")` for bundled cores, or empty

### 4. Config Templates (`paks/configs/*.cfg`)

Core-specific configuration with single platform placeholder:

```ini
{{PLATFORM_MINARCH_SETTING}}

gambatte_gb_colorization = internal
gambatte_gb_internal_palette = TWB64 - Pack 1
gambatte_gb_bootloader = disabled

bind Up = UP
bind Down = DOWN
# ... etc
```

**Placeholder:**
- `{{PLATFORM_MINARCH_SETTING}}`: Replaced with platform's `default_minarch_setting`

## Usage

### Generate All Paks

Paks are automatically generated during `make setup`:

```bash
make setup
# Generates all paks for all platforms in build/
```

Or manually:

```bash
./scripts/generate-paks.sh all
```

### Generate for Specific Platform

```bash
./scripts/generate-paks.sh miyoomini
```

### Generate Specific Cores

```bash
./scripts/generate-paks.sh miyoomini GB GBA
```

## Adding a New Core

1. **Add to `cores.json`**:
   ```json
   "NEWCORE": {
     "emu_exe": "newcore_name",
     "bundled_core": true  // optional - if EXTRAS core with bundled .so
   }
   ```

2. **Create config template** at `skeleton/TEMPLATES/minarch-paks/configs/NEWCORE.cfg`:
   ```ini
   {{PLATFORM_MINARCH_SETTING}}

   # Core-specific options
   newcore_option = value

   # Input bindings
   bind Up = UP
   bind Down = DOWN
   # ... etc
   ```

3. **Regenerate paks**:
   ```bash
   ./scripts/generate-paks.sh all
   ```

That's it! The core will now be generated for all platforms automatically.

## Adding a New Platform

1. **Add to `platforms.json`**:
   ```json
   "newplatform": {
     "nice_prefix": "",
     "default_minarch_setting": "minarch_cpu_speed = Powersave"
   }
   ```

2. **Regenerate paks**:
   ```bash
   ./scripts/generate-paks.sh all
   ```

All existing cores will now be generated for the new platform.

## Modifying a Core's Config

1. **Edit the template** at `skeleton/TEMPLATES/paks/configs/<CORE>.cfg`

2. **Regenerate paks**:
   ```bash
   ./scripts/generate-paks.sh all <CORE>
   # Or just regenerate the specific core for all platforms
   ```

Changes will apply to all platforms automatically.

## Modifying Platform Settings

1. **Edit `skeleton/TEMPLATES/platforms.json`**

2. **Regenerate paks**:
   ```bash
   ./scripts/generate-paks.sh all
   ```

## Scripts

### `scripts/generate-paks.sh`

Main generation script. Reads templates and metadata, generates paks.

**Arguments:**
- `platform`: Platform to generate (or `all`)
- `cores...`: Optional list of specific cores to generate

### `scripts/extract-config-templates.sh`

One-time extraction script that created the initial templates from `skeleton/SYSTEM/miyoomini/paks/Emus/`. Not needed during normal development.

## Git Workflow

**Committed to Git:**
- `skeleton/TEMPLATES/` - Templates and metadata (source of truth)
- `skeleton/SYSTEM/*/paks/` - Original paks (for reference during migration)
- `skeleton/EXTRAS/Emus/` - Original paks (for reference during migration)

**Generated (not committed):**
- `build/SYSTEM/*/paks/` - Generated during build
- `build/EXTRAS/Emus/` - Generated during build

**After Migration:**
Once the template system is fully validated, we can:
1. Delete the old paks from `skeleton/SYSTEM/` and `skeleton/EXTRAS/`
2. Update `.gitignore` to ignore generated paks
3. Keep only templates in Git

## Benefits

1. **Single Source of Truth**: Edit once, applies everywhere
2. **Easy to Add Cores**: Just add to `cores.json` and create config template
3. **Easy to Add Platforms**: Just add to `platforms.json`
4. **Clear Separation**: Shared config vs platform-specific settings
5. **Maintainable**: Clear where each variation comes from
6. **Scalable**: Adding 13th platform doesn't require duplicating 19 paks

## Migration Notes

This system was introduced during the prebuilt cores migration (commit 330c136). The original duplicated paks remain in `skeleton/` for validation, but are now ignored by the build system in favor of generated paks.

To verify generated paks match originals:

```bash
./scripts/generate-paks.sh miyoomini GB
diff skeleton/SYSTEM/miyoomini/paks/Emus/GB.pak/launch.sh \
     build/SYSTEM/miyoomini/paks/Emus/GB.pak/launch.sh
```

Should show no differences.
