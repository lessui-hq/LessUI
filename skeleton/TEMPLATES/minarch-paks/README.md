# MinArch Pak Templates

This directory contains templates for generating minarch libretro core paks across all platforms.

## Overview

Paks are generated from these templates via `scripts/generate-paks.sh`. The script reads `cores.json` and `platforms.json` to create platform-specific `.pak` directories containing:
- `launch.sh` - Core launcher script (from `launch.sh.template`)
- `default.cfg` - Core configuration file(s) (from `configs/`)

## Directory Structure

```
minarch-paks/
├── cores.json              # Core definitions (43 cores)
├── platforms.json          # Platform metadata
├── launch.sh.template      # Shared launch script template
└── configs/                # Configuration templates
    ├── base/               # Universal defaults (46 cores)
    │   ├── GBA/
    │   │   └── default.cfg
    │   ├── PS/
    │   │   └── default.cfg
    │   └── ...
    │
    └── {platform}/         # Platform-specific overrides (sparse)
        └── {TAG}/
            ├── default.cfg          # Platform override
            ├── default-{device}.cfg # Device variant (additive)
            └── ...
```

## Config Template Structure

Templates mirror the pak output structure:

**Template:**
```
configs/
  base/GBA/default.cfg           → GBA.pak/default.cfg (fallback)
  tg5040/GBA/default.cfg         → GBA.pak/default.cfg (overwrites base)
  tg5040/GBA/default-brick.cfg   → GBA.pak/default-brick.cfg (adds brick variant)
```

**Generated Pak:**
```
GBA.pak/
  launch.sh
  default.cfg          ← From tg5040/GBA/default.cfg (or base/GBA/ if missing)
  default-brick.cfg    ← From tg5040/GBA/default-brick.cfg (if exists)
```

### Key Principles

1. **Sparse Platform Overrides** - Only create `{platform}/{TAG}/` when you need platform-specific settings
2. **Additive Device Variants** - `default-{device}.cfg` files are ADDITIONAL configs, not replacements
3. **Fallback to Base** - If no platform override exists, uses `base/{TAG}/default.cfg`

## Config Loading Hierarchy (Runtime)

When minarch runs, it uses a **cascading config system** with last-one-wins:

### Stage 1: Select Most Specific File Per Category

For each config category, minarch picks ONE file (most specific):

**System Config (platform-wide frontend defaults):**
```
If exists: system-{device}.cfg    (e.g., system-brick.cfg)
Else:      system.cfg
→ Loaded into: config.system_cfg
```

**Default Config (core-specific settings):**
```
If exists: default-{device}.cfg   (e.g., default-brick.cfg)
Else:      default.cfg
→ Loaded into: config.default_cfg
```

**User Config (user preferences):**
```
If exists: {game}-{device}.cfg    (e.g., Pokemon.gbc-brick.cfg)
Else if:   {game}.cfg              (e.g., Pokemon.gbc.cfg)
Else if:   minarch-{device}.cfg   (e.g., minarch-brick.cfg)
Else:      minarch.cfg
→ Loaded into: config.user_cfg
```

### Stage 2: Load All Three Sequentially (Additive)

```c
Config_readOptionsString(config.system_cfg);    // Load first (lowest priority)
Config_readOptionsString(config.default_cfg);   // Overwrites system settings
Config_readOptionsString(config.user_cfg);      // Overwrites both (highest priority)
```

**Each setting uses "last one wins"** - if multiple configs set the same option, the last one loaded (user > default > system) takes precedence.

### Example: tg5040 Brick Playing GBA

**Files selected:**
1. `system-brick.cfg` → Sets `minarch_screen_scaling = Native`
2. `GBA.pak/default-brick.cfg` → Sets `minarch_cpu_speed = Powersave`, `bind A = A`
3. `minarch-brick.cfg` (if exists) → Could override anything

**Loading order:**
```
1. Load system-brick.cfg      → screen_scaling = Native
2. Load default-brick.cfg     → cpu_speed = Powersave (screen_scaling still Native)
3. Load minarch-brick.cfg     → Could override screen_scaling or cpu_speed
```

**Final state:**
- `minarch_screen_scaling = Native` (from system-brick.cfg, not overridden)
- `minarch_cpu_speed = Powersave` (from default-brick.cfg)
- All button bindings from default-brick.cfg
- Any user overrides from minarch-brick.cfg

### Lock Prefix

Settings prefixed with `-` in config files are **locked** and won't appear in the in-game menu:

```cfg
-minarch_prevent_tearing = Lenient    # Locked (user can't change in menu)
minarch_cpu_speed = Powersave         # Unlocked (user can change in menu)
```

## Device Variants

### Current Device Support

**rg35xxplus:**
- Standard (default)
- `cube` - RG CubeXX (720x720 square screen)
- `wide` - RG34XX (widescreen)

**tg5040:**
- Standard (default)
- `brick` - Trimui Brick variant

### Device Detection

Device variants are detected at boot in `MinUI.pak/launch.sh`:

**rg35xxplus:**
```bash
export RGXX_MODEL=`strings /mnt/vendor/bin/dmenu.bin | grep ^RG`
case "$RGXX_MODEL" in
    RGcubexx)  export DEVICE="cube" ;;
    RG34xx*)   export DEVICE="wide" ;;
esac
```

**tg5040:**
```bash
export TRIMUI_MODEL=`strings /usr/trimui/bin/MainUI | grep ^Trimui`
if [ "$TRIMUI_MODEL" = "Trimui Brick" ]; then
    export DEVICE="brick"
fi
```

The `$DEVICE` environment variable is then used by minarch to load device-specific configs.

## Adding Device-Specific Configs

### When to Add Device Variants

Create `default-{device}.cfg` when a specific core needs different settings on a device variant:

- **Different screen aspect ratios** (square vs. 4:3 vs. 16:9)
- **Different scaling needs** (Native vs. Aspect)
- **Performance differences** (different CPU speeds)

### How to Add

1. **Create template:**
   ```bash
   # For GBA on Trimui Brick
   skeleton/TEMPLATES/minarch-paks/configs/tg5040/GBA/default-brick.cfg
   ```

2. **Add device-specific settings:**
   ```cfg
   minarch_screen_scaling = Aspect
   minarch_cpu_speed = Powersave

   -gpsp_save_method = libretro

   bind Up = UP
   bind Down = DOWN
   # ... (full button mapping)
   ```

3. **Regenerate paks:**
   ```bash
   ./scripts/generate-paks.sh tg5040 GBA
   ```

4. **Result:**
   ```
   GBA.pak/
     default.cfg         ← Standard TG5040
     default-brick.cfg   ← Brick variant
   ```

### Common Device Differences

**Cube (square screen):**
- `minarch_screen_scaling = Native` (integer scaling works better on square)
- `minarch_screen_sharpness = Sharp` (crisp pixels)

**Brick (smaller screen):**
- `minarch_screen_scaling = Aspect` (maximize visible area)
- May need different CPU speeds for performance

## Complete Config Hierarchy

Full cascading order from lowest to highest priority:

```
1. system.cfg                    (Platform-wide frontend defaults)
2. system-{device}.cfg           (Device-specific platform defaults)
3. default.cfg                   (Core defaults from pak)
4. default-{device}.cfg          (Device-specific core defaults from pak)
5. minarch.cfg                   (User console-wide preferences)
6. minarch-{device}.cfg          (Device-specific user preferences)
7. {game}.cfg                    (Per-game overrides)
8. {game}-{device}.cfg           (Device-specific per-game overrides)
```

Within each level, later entries override earlier entries. The most specific config always wins.

## See Also

- `docs/PAK-TEMPLATES.md` - Complete pak template system documentation
- `cores.json` - Core definitions and metadata
- `platforms.json` - Platform metadata and architecture info
