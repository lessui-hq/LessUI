# Platform Display Specifications

This document provides a comprehensive reference for all display specifications, UI scaling, and padding configurations across supported platforms.

## Overview

LessUI uses a **platform abstraction layer** where each device defines its hardware specifications in `workspace/<platform>/platform/platform.h`. The common UI code in `workspace/all/` uses these definitions to render appropriately for each device.

### Key Concepts

- **Physical Resolution**: Actual pixel dimensions of the hardware display
- **Logical Resolution**: UI coordinates before scaling (`FIXED_WIDTH / FIXED_SCALE`)
- **FIXED_SCALE**: Multiplier applied to UI coordinates (1x, 2x, or 3x)
- **PADDING**: Margin around UI elements in logical pixels
- **MAIN_ROW_COUNT**: Number of menu items visible on screen
- **PPI (Pixels Per Inch)**: Pixel density of the display

### Default Values

Unless overridden by a platform, these defaults apply (from `workspace/all/common/defines.h`):

```c
#define PILL_SIZE 30        // Height of menu item pills (logical pixels)
#define PADDING 10          // Screen padding (logical pixels)
#define MAIN_ROW_COUNT 6    // Visible menu rows
```

## Platform Specifications

### Miyoo Mini / Mini Plus (`miyoomini`)

**Hardware:**
- Screen: 2.8" IPS LCD
- Aspect Ratio: 4:3
- Variants: Standard (640×480) and 560p (752×560)

**Display Configuration:**

| Variant | Physical | Logical | Scale | Padding | Rows | PPI |
|---------|----------|---------|-------|---------|------|-----|
| Standard | 640×480 | 320×240 | 2× | 10px | 6 | 286 |
| 560p | 752×560 | 376×280 | 2× | 5px | 8 | 337 |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  (is_560p?752:640)
#define FIXED_HEIGHT (is_560p?560:480)
#define PADDING      (is_560p?5:10)
#define MAIN_ROW_COUNT (is_560p?8:6)
```

**Notes:**
- Runtime detection of 560p variant (Miyoo Mini Plus)
- 560p reduces padding from 10px to 5px to fit 8 rows
- Padding as % of width: 3.1% (standard), 1.3% (560p)
- High pixel density on a small screen

---

### Trimui Smart (`trimuismart`)

**Hardware:**
- Screen: 2.4" IPS LCD
- Resolution: 320×240 (QVGA)
- Aspect Ratio: 4:3

**Display Configuration:**

| Physical | Logical | Scale | Padding | Rows | PPI |
|----------|---------|-------|---------|------|-----|
| 320×240 | 320×240 | 1× | 10px (default) | 6 (default) | 167 |

```c
#define FIXED_SCALE  1
#define FIXED_WIDTH  320
#define FIXED_HEIGHT 240
// Uses defaults: PADDING 10, MAIN_ROW_COUNT 6
```

**Notes:**
- No scaling (1:1 pixel mapping)
- Smallest physical resolution
- Padding as % of width: 3.1%

---

### Trimui Smart Pro / Brick (`tg5040`)

**Hardware:**
- Screen: 4.95" IPS LCD (Standard), 3.2" IPS LCD (Brick)
- Variants: Standard 16:9, Brick 4:3
- Runtime detection of Brick variant

**Display Configuration:**

| Variant | Physical | Logical | Scale | Padding | Rows | PPI |
|---------|----------|---------|-------|---------|------|-----|
| **Standard (16:9)** | 1280×720 | 640×360 | 2× | 20px | 8 | 297 |
| **Brick (4:3)** | 1024×768 | 341×256 | 3× | 10px | 7 | 400 |

```c
#define FIXED_SCALE  (is_brick?3:2)
#define FIXED_WIDTH  (is_brick?1024:1280)
#define FIXED_HEIGHT (is_brick?768:720)
#define PADDING      (is_brick?10:20)
#define MAIN_ROW_COUNT (is_brick?7:8)
```

**Notes:**
- **Standard uses 20px padding** (40px physical = 3.1% of width)
  - Balanced feel matching other 4:3 devices
  - Battery icon properly positioned in corner
- **Brick uses 10px padding** (30px physical = 2.9% of width)
  - Matches standard 4:3 device feel (RG35XX, Miyoo Mini)
  - **Highest PPI (400)** - very sharp display with appropriate breathing room

---

### Anbernic RG35XX (`rg35xx`)

**Hardware:**
- Screen: 3.5" IPS LCD
- Resolution: 640×480
- Aspect Ratio: 4:3

**Display Configuration:**

| Physical | Logical | Scale | Padding | Rows | PPI |
|----------|---------|-------|---------|------|-----|
| 640×480 | 320×240 | 2× | 10px (default) | 6 (default) | 229 |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  640
#define FIXED_HEIGHT 480
// Uses defaults: PADDING 10, MAIN_ROW_COUNT 6
```

**Notes:**
- Same resolution as Miyoo Mini standard
- Padding as % of width: 1.6%

---

### Anbernic RG35XX Plus/H/SP and RG34XX (`rg35xxplus`)

**Hardware:**
- Multiple device variants with runtime detection
- Screens: 3.5" (Plus/H/SP), 3.95" (CubeXX), 3.4" (34XX)
- Aspect ratios: 4:3 (Plus/H/SP), 1:1 (CubeXX), 4:3 (34XX)

**Display Configuration:**

| Variant | Physical | Logical | Scale | Padding | Rows | PPI |
|---------|----------|---------|-------|---------|------|-----|
| **Plus/SP** | 640×480 | 320×240 | 2× | 10px | 6 | 229 |
| **H** | 640×480 | 320×240 | 2× | 10px | 6 | 229 |
| **CubeXX** | 720×720 | 360×360 | 2× | 40px | 8 | 258 |
| **34XX** | 720×480 | 360×240 | 2× | 40px | 8 | 255 |
| **HDMI** | 1280×720 | 640×360 | 2× | 40px | 8 | varies |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  (is_cubexx?720:(is_rg34xx?720:640))
#define FIXED_HEIGHT (is_cubexx?720:480)
#define PADDING      (is_cubexx||on_hdmi?40:10)
#define MAIN_ROW_COUNT (is_cubexx||on_hdmi?8:6)
```

**Notes:**
- Runtime detection of device variant and HDMI output
- CubeXX/34XX/HDMI use 40px padding (same issue as TG5040 standard)
- H uses 640×480 but gets special handling (may need clarification)
- Padding as % of width: 1.6% (Plus/H/SP), 5.6% (CubeXX/34XX)

---

### Powkiddy RGB30 (`rgb30`)

**Hardware:**
- Screen: 4.0" IPS LCD
- Resolution: 720×720 (1:1 square display)
- Aspect Ratio: 1:1

**Display Configuration:**

| Physical | Logical | Scale | Padding | Rows | PPI |
|----------|---------|-------|---------|------|-----|
| 720×720 | 360×360 | 2× | 40px | 8 | 255 |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  720
#define FIXED_HEIGHT 720
#define PADDING      40
#define MAIN_ROW_COUNT 8
```

**Notes:**
- Square display (unique aspect ratio)
- Uses 40px padding like other high-res devices
- Padding as % of width: 5.6%
- HDMI output: 1280×720

---

### Miyoo A30 (`my355`)

**Hardware:**
- Screen: 2.8" IPS LCD
- Resolution: 640×480
- Aspect Ratio: 4:3

**Display Configuration:**

| Mode | Physical | Logical | Scale | Padding | Rows | PPI |
|------|----------|---------|-------|---------|------|-----|
| **LCD** | 640×480 | 320×240 | 2× | 10px | 6 | 286 |
| **HDMI** | 1280×720 | 640×360 | 2× | 40px | 8 | varies |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  640
#define FIXED_HEIGHT 480
#define PADDING      (on_hdmi?40:10)
#define MAIN_ROW_COUNT (on_hdmi?8:6)
```

**Notes:**
- Runtime detection of HDMI output
- HDMI uses 40px padding
- Padding as % of width: 1.6% (LCD), 6.25% (HDMI)

---

### Anbernic RG28XX (`my282`)

**Hardware:**
- Screen: 2.8" IPS LCD
- Resolution: 640×480
- Aspect Ratio: 4:3

**Display Configuration:**

| Physical | Logical | Scale | Padding | Rows | PPI |
|----------|---------|-------|---------|------|-----|
| 640×480 | 320×240 | 2× | 10px (default) | 6 (default) | 286 |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  640
#define FIXED_HEIGHT 480
// Uses defaults: PADDING 10, MAIN_ROW_COUNT 6
```

**Notes:**
- Smallest screen with 640×480 resolution (highest PPI)
- Padding as % of width: 1.6%

---

### MagicX XU Mini M (`magicmini`)

**Hardware:**
- Screen: ~3.5" IPS LCD (estimated)
- Resolution: 640×480
- Aspect Ratio: 4:3

**Display Configuration:**

| Physical | Logical | Scale | Padding | Rows | PPI |
|----------|---------|-------|---------|------|-----|
| 640×480 | 320×240 | 2× | 10px (default) | 6 (default) | 229 |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  640
#define FIXED_HEIGHT 480
// Uses defaults: PADDING 10, MAIN_ROW_COUNT 6
```

**Notes:**
- Standard 640×480 configuration
- Padding as % of width: 1.6%
- **Deprecated platform**

---

### MagicX Mini Zero 28 (`zero28`)

**Hardware:**
- Screen: 2.8" IPS LCD
- Resolution: 640×480
- Aspect Ratio: 4:3

**Display Configuration:**

| Physical | Logical | Scale | Padding | Rows | PPI |
|----------|---------|-------|---------|------|-----|
| 640×480 | 320×240 | 2× | 10px | 6 | ~286 |

```c
#define FIXED_SCALE  2
#define FIXED_WIDTH  640
#define FIXED_HEIGHT 480
#define PADDING      10
#define MAIN_ROW_COUNT 6
```

**Notes:**
- Same specs as RG28XX
- Padding as % of width: 1.6%

---

### Generic M17 (`m17`)

**Hardware:**
- Screen: 4.3" LCD
- Resolution: 480×273
- Aspect Ratio: 16:9

**Display Configuration:**

| Physical | Logical | Scale | Padding | Rows | PPI |
|----------|---------|-------|---------|------|-----|
| 480×273 | 480×273 | 1× | 10px (default) | 7 | 128 |

```c
#define FIXED_SCALE  1
#define FIXED_WIDTH  480
#define FIXED_HEIGHT 273
#define MAIN_ROW_COUNT 7
// Uses default: PADDING 10
```

**Notes:**
- No scaling (1:1 pixel mapping)
- Only 16:9 device with 1× scale
- Padding as % of width: 2.1%
- **Deprecated platform**

---

## Padding Analysis

### Padding by Platform

| Platform | Padding (logical) | Physical Padding | % of Width | Feel |
|----------|-------------------|------------------|------------|------|
| RGB30 | **40px** | 80px | 5.6% | Loose |
| RG35XXH/CubeXX | **40px** | 80px | 5.6% | Loose |
| MY355 (HDMI) | **40px** | 80px | 6.25% | Loose |
| TG5040 Standard | 20px | 40px | 3.1% | Balanced ✓ |
| Miyoo Mini | 10px | 20px | 3.1% | Balanced |
| Trimui Smart | 10px | 10px | 3.1% | Balanced |
| TG5040 Brick | 10px | 30px | 2.9% | Balanced ✓ |
| M17 | 10px | 10px | 2.1% | Balanced |
| RG35XX | 10px | 20px | 1.6% | Tight |
| RG35XXPlus | 10px | 20px | 1.6% | Tight |
| MY282 | 10px | 20px | 1.6% | Tight |
| Zero28 | 10px | 20px | 1.6% | Tight |
| MY355 (LCD) | 10px | 20px | 1.6% | Tight |
| Miyoo Mini 560p | **5px** | 10px | 1.3% | Very tight |

### Recent Fixes

**1. TG5040 Standard (16:9) - Fixed ✓**
- Before: 40px logical (80px physical) = 6.25% of width (too loose)
- After: 20px logical (40px physical) = 3.1% of width
- Result: Battery icon properly in corner, balanced feel

**2. TG5040 Brick (4:3) - Fixed ✓**
- Before: 5px logical (15px physical) = 1.47% of width (too tight)
- After: 10px logical (30px physical) = 2.9% of width
- Result: Matches standard 4:3 device feel

### Remaining Considerations

**High-resolution devices with 40px padding:**
- RGB30, RG35XXH, RG35XXCubeXX all use 40px
- Consistent with each other but looser than standard devices
- May want to consider 20-30px for better balance in future updates

### Recommended Padding Formula

For consistency across platforms:

```c
// Target: 2-4% of logical width
PADDING = CLAMP(FIXED_WIDTH / FIXED_SCALE / 20, 5, 20)
```

This would yield:
- 320px wide (4:3) → 16px padding (~5% → 3.1%)
- 360px wide (1:1) → 18px padding (~5% → 3.1%)
- 640px wide (16:9) → 32px padding (clamped to 20px → 3.1%)

## Aspect Ratio Summary

| Aspect Ratio | Platforms | Notes |
|--------------|-----------|-------|
| **4:3** | Miyoo Mini, Trimui Smart, RG35XX, RG35XXPlus, RG35XXSP, MY355, MY282, Zero28, MagicMini, TG5040 Brick | Most common |
| **16:9** | TG5040 Standard, M17, MY355 HDMI, RG35XXPlus HDMI | Widescreen |
| **1:1** | RGB30, RG35XXH, RG35XXCubeXX | Square displays |

## Resolution Tiers

| Tier | Logical Width | Platforms | Typical Scale |
|------|---------------|-----------|---------------|
| **Low** | 320px | Miyoo Mini, Trimui Smart, RG35XX, MY282, Zero28 | 1-2× |
| **Medium** | 360px | RGB30, RG35XXH, TG5040 Standard | 2× |
| **High** | 341-376px | Miyoo Mini 560p, TG5040 Brick | 2-3× |
| **Widescreen** | 480-640px | M17, HDMI outputs | 1-2× |

## Testing Checklist

When testing UI on different platforms, verify:

- [ ] Battery icon is in top-right corner (not inset excessively)
- [ ] Menu items have adequate breathing room
- [ ] Text is not clipped or truncated
- [ ] Padding feels consistent with other 4:3 or 16:9 devices
- [ ] All MAIN_ROW_COUNT items fit on screen with padding
- [ ] Status indicators are visible and properly positioned

## Related Files

| File | Purpose |
|------|---------|
| `workspace/<platform>/platform/platform.h` | Platform-specific display configuration |
| `workspace/all/common/defines.h` | Default values and scaling macros |
| `workspace/all/minui/minui.c` | Launcher UI rendering (uses PADDING) |
| `workspace/all/common/api.c` | Graphics API (GFX_* functions) |
| `CLAUDE.md` | Development guide and architecture overview |

---

**Last Updated:** 2025-11-21
