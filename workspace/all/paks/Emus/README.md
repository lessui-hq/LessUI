# Player Pak Templates

This directory contains templates for generating player libretro core paks across all platforms.

## Overview

Paks are generated from these templates via `scripts/generate-paks.sh`. The script reads `cores.json` and `platforms.json` to create platform-specific `.pak` directories containing:
- `launch.sh` - Core launcher script (from `launch.sh.template`)
- `default.cfg` - Core configuration file(s) (from `configs/`)

## Directory Structure

```
player-paks/
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

Platform configs are **merged** onto base configs at build time, using last-one-wins semantics
(matching runtime behavior). This allows platform configs to be sparse - containing only the
settings that differ from base.

**Template:**
```
configs/
  base/GBA/default.cfg           → Complete core defaults (bindings, locked options)
  tg5040/GBA/default.cfg         → Sparse overrides (just tg5040-specific settings)
  tg5040/GBA/default-brick.cfg   → Sparse brick-specific overrides
```

**Generated Pak (after merge):**
```
GBA.pak/
  launch.sh
  default.cfg          ← base/GBA/default.cfg merged with tg5040/GBA/default.cfg
  default-brick.cfg    ← base/GBA/default.cfg merged with tg5040/GBA/default-brick.cfg
```

### Build-Time Merge Logic

1. **default.cfg**: `base/{CORE}/default.cfg` + `{platform}/{CORE}/default.cfg`
2. **default-{device}.cfg**: `base/{CORE}/default.cfg` + `{platform}/{CORE}/default-{device}.cfg`

When the same key appears in both files, the platform value wins (last-one-wins).

### Key Principles

1. **Sparse Platform Overrides** - Platform configs should contain ONLY settings that differ from base
2. **Cascading Merge** - Build-time behavior matches runtime (system.cfg → default.cfg → user.cfg)
3. **Device Variants Inherit** - `default-{device}.cfg` inherits from base `default.cfg`, then applies device-specific settings

### Default Settings Philosophy

Base configs use **Aspect** scaling and **Crisp** sharpness as defaults. This follows the principle that most users want games to fill their screens as much as possible while looking as good as possible.

**Standard displays (4:3, 16:9):**
- `player_screen_scaling = Aspect` - Fill the screen while preserving aspect ratio
- `player_screen_sharpness = Crisp` - Sharp pixels with subtle smoothing

**Square displays (cube, rgb30):**
- `player_screen_scaling = Native` or `Cropped` - Integer scaling for pixel-perfect display
- `player_screen_sharpness = Sharp` - Maximum crispness for integer-scaled pixels

Platform overrides exist only when truly needed:
- **Square screens**: Different scaling for integer-pixel modes (cube, rgb30)
- **Button availability**: L2/R2 bindings for PS on devices that have those buttons (m17, trimuismart)

### Example: Sparse Platform Config

Instead of duplicating all 15 lines from base, a platform config only needs the unique lines:

```cfg
# configs/rg35xxplus/GBA/default-cube.cfg (just 2 lines!)
player_screen_scaling = Native
player_screen_sharpness = Sharp
```

The build script merges this with `base/GBA/default.cfg` to produce a complete 16-line config.

## Config Loading Hierarchy (Runtime)

When player runs, it uses a **cascading config system** with last-one-wins:

### Stage 1: Select Most Specific File Per Category

For each config category, player picks ONE file (most specific):

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
Else if:   player-{device}.cfg   (e.g., player-brick.cfg)
Else:      player.cfg
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
1. `system-brick.cfg` → Sets `player_screen_scaling = Native`
2. `GBA.pak/default-brick.cfg` → Sets `player_cpu_speed = Powersave`, `bind A = A`
3. `player-brick.cfg` (if exists) → Could override anything

**Loading order:**
```
1. Load system-brick.cfg      → screen_scaling = Native
2. Load default-brick.cfg     → cpu_speed = Powersave (screen_scaling still Native)
3. Load player-brick.cfg     → Could override screen_scaling or cpu_speed
```

**Final state:**
- `player_screen_scaling = Native` (from system-brick.cfg, not overridden)
- `player_cpu_speed = Powersave` (from default-brick.cfg)
- All button bindings from default-brick.cfg
- Any user overrides from player-brick.cfg

### Lock Prefix

Settings prefixed with `-` in config files are **locked** and won't appear in the in-game menu:

```cfg
-player_screen_sharpness = Crisp     # Locked (user can't change in menu)
player_cpu_speed = Powersave         # Unlocked (user can change in menu)
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

Device variants are detected at boot in `LessUI.pak/launch.sh`:

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

The `$DEVICE` environment variable is then used by player to load device-specific configs.

## Adding Device-Specific Configs

### When to Add Device Variants

Create `default-{device}.cfg` when a specific core needs different settings on a device variant:

- **Different screen aspect ratios** (square vs. 4:3 vs. 16:9)
- **Different scaling needs** (Native vs. Aspect)
- **Performance differences** (different CPU speeds)

### How to Add

1. **Create template:**
   ```bash
   # For GBA on RG CubeXX (square screen)
   workspace/all/paks/Emus/configs/rg35xxplus/GBA/default-cube.cfg
   ```

2. **Add device-specific settings (sparse - only what differs from base):**
   ```cfg
   player_screen_scaling = Native
   player_screen_sharpness = Sharp
   ```

3. **Regenerate paks:**
   ```bash
   ./scripts/generate-paks.sh rg35xxplus GBA
   ```

4. **Result:**
   ```
   GBA.pak/
     default.cfg         ← Base config (Aspect + Crisp)
     default-cube.cfg    ← Merged: base + cube overrides (Native + Sharp)
   ```

### Common Device Differences

**Square screens (cube, rgb30):**
- `player_screen_scaling = Native` for systems that fit perfectly (GBA on 720×720)
- `player_screen_scaling = Cropped` for 4:3 systems (FC, SFC) to maximize size
- `player_screen_sharpness = Sharp` for pixel-perfect integer scaling

**Brick (smaller 4:3 screen):**
- Uses base defaults (Aspect + Crisp) - no overrides needed

## Complete Config Hierarchy

Full cascading order from lowest to highest priority:

```
1. system.cfg                    (Platform-wide frontend defaults)
2. system-{device}.cfg           (Device-specific platform defaults)
3. default.cfg                   (Core defaults from pak)
4. default-{device}.cfg          (Device-specific core defaults from pak)
5. player.cfg                   (User console-wide preferences)
6. player-{device}.cfg          (Device-specific user preferences)
7. {game}.cfg                    (Per-game overrides)
8. {game}-{device}.cfg           (Device-specific per-game overrides)
```

Within each level, later entries override earlier entries. The most specific config always wins.

## See Also

- `docs/paks-architecture.md` - Complete pak template system documentation
- `cores.json` - Core definitions and metadata
- `platforms.json` - Platform metadata and architecture info
