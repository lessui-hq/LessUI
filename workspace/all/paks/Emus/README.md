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
            ├── device-{device}.cfg  # Device variant (additive)
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
  tg5040/GBA/device-brick.cfg    → Sparse brick-specific overrides
```

**Generated Pak (after merge):**
```
GBA.pak/
  launch.sh
  default.cfg          ← base/GBA/default.cfg merged with tg5040/GBA/default.cfg
  device-brick.cfg     ← base/GBA/default.cfg merged with tg5040/GBA/device-brick.cfg
```

### Build-Time Merge Logic

1. **default.cfg**: `base/{CORE}/default.cfg` + `{platform}/{CORE}/default.cfg`
2. **device-{device}.cfg**: `base/{CORE}/default.cfg` + `{platform}/{CORE}/device-{device}.cfg`

When the same key appears in both files, the platform value wins (last-one-wins).

### Key Principles

1. **Sparse Platform Overrides** - Platform configs should contain ONLY settings that differ from base
2. **Cascading Merge** - Build-time behavior matches runtime (system.cfg → default.cfg → user.cfg)
3. **Device Variants Inherit** - `device-{device}.cfg` inherits from base `default.cfg`, then applies device-specific settings

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
# configs/rg35xxplus/GBA/device-rgcubexx.cfg (just 2 lines!)
player_screen_scaling = Native
player_screen_sharpness = Sharp
```

The build script merges this with `base/GBA/default.cfg` to produce a complete 16-line config.

## Config Loading Hierarchy (Runtime)

When player runs, it uses a **cascading config system** with last-one-wins.

### Config Cascade

The player reads `LESSUI_VARIANT` and `LESSUI_DEVICE` environment variables to load configs in order:

**System Config (platform-wide settings):**
```
system.cfg
→ Loaded into: config.system_cfg
```

**Default Config (core-specific settings, cascaded):**
```
1. default.cfg                    (base core settings)
2. variant-{variant}.cfg          (if exists, e.g., variant-square.cfg)
3. device-{device}.cfg            (if exists, e.g., device-rgcubexx.cfg)
→ All concatenated into: config.default_cfg
```

**User Config (user preferences):**
```
If exists: {game}.cfg             (e.g., Pokemon.gbc.cfg)
Else:      player.cfg
→ Loaded into: config.user_cfg
```

### Loading Order

```c
Config_readOptionsString(config.system_cfg);    // Load first (lowest priority)
Config_readOptionsString(config.default_cfg);   // Overwrites system settings
Config_readOptionsString(config.user_cfg);      // Overwrites both (highest priority)
```

**Each setting uses "last one wins"** - if multiple configs set the same option, the last one loaded (user > default > system) takes precedence.

### Example: RG CubeXX Playing GBA

**Environment:**
- `LESSUI_PLATFORM=rg35xxplus`
- `LESSUI_VARIANT=square`
- `LESSUI_DEVICE=rgcubexx`

**Files loaded (in order):**
1. `system.cfg` → Platform-wide defaults
2. `GBA.pak/default.cfg` → Base core settings
3. `GBA.pak/device-rgcubexx.cfg` → Square screen overrides (Native + Sharp)
4. `player.cfg` (if exists) → User preferences

**Final state:**
- `player_screen_scaling = Native` (from device-rgcubexx.cfg)
- `player_screen_sharpness = Sharp` (from device-rgcubexx.cfg)
- All button bindings from default.cfg
- Any user overrides from player.cfg

### Lock Prefix

Settings prefixed with `-` in config files are **locked** and won't appear in the in-game menu:

```cfg
-player_screen_sharpness = Crisp     # Locked (user can't change in menu)
player_cpu_speed = Powersave         # Unlocked (user can change in menu)
```

## Device Variants

### Device Identification System

LessUI uses three environment variables for device identification:

- `LESSUI_PLATFORM` - Build target (e.g., "miyoomini", "rg35xxplus", "tg5040")
- `LESSUI_VARIANT` - Screen resolution/aspect (e.g., "vga", "square", "wide", "4x3")
- `LESSUI_DEVICE` - Specific hardware model (e.g., "rgcubexx", "brick", "miniv1")

These are exported by `LessUI.pak/platforms/{platform}/init.sh` at boot.

### Current Device Support

**rg35xxplus:**
- `rg35xxplus` - Standard 640x480 (default)
- `rgcubexx` - RG CubeXX (720x720 square screen)
- `rg34xx` - RG34XX (720x480 widescreen)
- `rg28xx` - RG28XX (640x480, smaller 2.8" screen)

**tg5040:**
- `smartpro` - Trimui Smart Pro (default, 1280x720)
- `brick` - Trimui Brick (1024x768, 3.2" screen)

**retroid:**
- `pocket5` / `flip2` - Full HD devices (default, 1920x1080)
- `miniv1` - Mini V1 (1280x960, 4:3 aspect)
- `miniv2` - Mini V2 (1240x1080, narrow aspect)

**miyoomini:**
- `miyoomini` - Standard (640x480, 2.8" screen)
- `miyoominiplus` - Plus model (640x480, 3.5" screen)

The `$LESSUI_DEVICE` environment variable is used by player to load device-specific configs.

## Adding Device-Specific Configs

### When to Add Device Variants

Create `device-{device}.cfg` when a specific core needs different settings on a device variant:

- **Different screen aspect ratios** (square vs. 4:3 vs. 16:9)
- **Different scaling needs** (Native vs. Aspect)
- **Small screen sizes** (screens < 3" need stricter Native scaling thresholds)

### How to Add

1. **Create template:**
   ```bash
   # For GBA on RG CubeXX (square screen)
   workspace/all/paks/Emus/configs/rg35xxplus/GBA/device-rgcubexx.cfg
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
     default.cfg           ← Base config (Aspect + Crisp)
     device-rgcubexx.cfg   ← Device-specific overrides (Native + Sharp)
   ```

### Config Cascade

Player loads configs in order, with each layer overriding the previous:
1. `default.cfg` - Base platform config
2. `variant-{variant}.cfg` - Variant config (if exists)
3. `device-{device}.cfg` - Device config (if exists)

### Common Device Differences

**Square screens (rgcubexx, rgb30, miniv2):**
- `player_screen_scaling = Native` for systems that fit perfectly (GBA on 720×720)
- `player_screen_scaling = Cropped` for 4:3 systems (FC, SFC) to maximize size
- `player_screen_sharpness = Sharp` for pixel-perfect integer scaling

**Small screens (rg28xx, miyoomini, brick):**
- Native scaling only when fill percentage is 100% (perfect fit)
- Stricter threshold than larger screens which allow 94%+ fill

**Brick (smaller 4:3 screen):**
- Uses base defaults (Aspect + Crisp) - no overrides needed

## Complete Config Hierarchy

Full cascading order from lowest to highest priority:

```
1. system.cfg                    (Platform-wide frontend defaults)
2. default.cfg                   (Core defaults from pak)
3. variant-{variant}.cfg         (Variant-specific core defaults, e.g., "square", "wide")
4. device-{device}.cfg           (Device-specific core defaults, e.g., "rgcubexx", "brick")
5. player.cfg                    (User console-wide preferences)
6. {game}.cfg                    (Per-game overrides)
```

Within each level, later entries override earlier entries. The most specific config always wins.

Note: `variant-{variant}.cfg` files are rarely needed since device configs handle most cases.
The variant layer exists for shared settings across devices with the same screen aspect/resolution.

## See Also

- `docs/paks-architecture.md` - Complete pak template system documentation
- `cores.json` - Core definitions and metadata
- `platforms.json` - Platform metadata and architecture info
