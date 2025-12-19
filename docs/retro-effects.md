# Retro Display Effects

LessUI includes physically-motivated CRT and LCD display effects that add authentic retro character to emulated games.

## Overview

All effects use a **shadow-based approach**: black pixels with varying alpha values create darkening without color washing. Global opacity is controlled dynamically by `EFFECT_getOpacity(scale)` based on the current pixel scaling.

## Effect Types

### Line (Horizontal Scanlines)

Simulates CRT raster scan lines - the horizontal lines visible on CRT displays as the electron beam sweeps across.

**Physical basis:** Electron beam scans horizontally, exciting phosphors row by row. Phosphors glow and fade, creating visible gaps between scan lines.

**Pattern (scale 3+):**

```
Row 0: alpha 30  (light lead-in - phosphor warming)
Row 1: alpha 80  (main scanline - peak brightness)
Row 2: alpha 10  (light trail - phosphor decay)
Rows 3+: transparent
```

**Scale 2:** Simple single-row scanline (alpha 255) - no room for falloff.

**Use case:** Classic CRT look for any retro system.

---

### Grid (LCD Pixel Borders)

Simulates the visible pixel grid on LCD handheld screens (Game Boy, GBA, etc.).

**Physical basis:** LCD displays have physical borders between pixels where the subpixel matrix structure is visible.

**Pattern (all scales):**

```
Row 0: alpha 255 (top border)
Column 0: alpha 255 (left border)
Rest: transparent (pixel interior where light passes)
```

**Special feature:** Supports color tinting via `PLAT_setEffectColor()` for Game Boy DMG color palettes. When a color is set, the black borders are tinted to match the palette.

**Use case:** Handheld LCD authenticity, especially for Game Boy games.

---

### Slot (Slot Mask)

Simulates consumer CRT shadow masks - metal plate with vertical slot openings, staggered between rows to reduce moiré.

**Physical basis:** Electron beam scans horizontally through vertical slots in a metal mask. Slots are staggered row-by-row (brick pattern) to distribute the mask structure evenly.

**Pattern (scale 3+):**

```
Top row:
  Row 0: alpha 255 (horizontal border)
  Row 1: alpha 60 (glow - beam falloff from border)
  Rows 2+: transparent (slot opening)

Bottom row (staggered):
  Row mid: alpha 255 (horizontal border)
  Row mid+1: alpha 60 (glow - beam falloff from border)
  Rows mid+2+: transparent (slot opening)

Left border: column 0 alpha 255 (top row only)
Right border: column (scale-1) alpha 255 (bottom row only)
```

**Scale 2:** Simple staggered slots (no glow).

**Tile size:** `scale × (scale * 2)` - Two rows to show the stagger pattern.

**Use case:** Consumer TV CRT look.

---

## Pattern Generation

All patterns are procedurally generated at runtime by `effect_generate.c`.

**Design principles:**

1. **Pixel-grid alignment:** All tile dimensions are multiples of `scale`
2. **Shadow-only:** Black pixels (RGB 0,0,0) with alpha variations
3. **Physical accuracy:** Glow patterns match real display physics
4. **Scale-adaptive:** Simple at low scales, dimensional at scale 3+

**Alpha value guide:**

- `255`: Full shadow (primary structures - borders, bars, main scanlines)
- `60-80`: Medium shadow (phosphor glow, secondary scanlines)
- `10-30`: Light shadow (subtle falloff, trails)
- `0`: Transparent (light passes through)

## Implementation

### Pattern Files

All effects are now procedurally generated at runtime - no PNG files required.

### Shared Code

**Effect state management:** `workspace/all/common/effect_system.{c,h}`

- `EFFECT_usesGeneration(type)` - Returns true for all effect types
- `EFFECT_getGeneratedOpacity(type)` - Returns 255 (per-pixel alpha does the work)
- `EffectState` - Tracks current effect type, scale, and color

**Procedural generation:** `workspace/all/common/effect_generate.{c,h}`

- `EFFECT_generateLine()` - Horizontal scanline pattern
- `EFFECT_generateGrid()` - LCD pixel border pattern
- `EFFECT_generateCRT()` - RGB phosphor aperture grille + scanlines
- `EFFECT_generateSlot()` - Staggered brick/slot mask pattern

**SDL2 rendering:** `workspace/all/common/effect_utils.{c,h}`

- `EFFECT_createGeneratedTexture()` - Creates texture from procedural generation
- Used by all SDL2 platforms (tg5040, rg35xxplus, rgb30, my282, my355, zero28, magicmini)

**SDL1 rendering:** `workspace/all/common/effect_surface.{c,h}`

- `EFFECT_createTiledSurfaceWithColor()` - CPU-based tiling for SDL1
- Used by SDL1 platforms (miyoomini, trimuismart, rg35xx)

### Color Tinting

The grid effect supports color tinting for Game Boy DMG palettes:

```c
PLAT_setEffect(EFFECT_GRID);
PLAT_setEffectColor(0x739C);  // Game Boy green
```

When `effect_state.color` is non-zero and effect type is `EFFECT_GRID`, the black border pixels are replaced with the specified RGB565 color while preserving alpha.

## Usage

Effects are set via the platform API:

```c
PLAT_setEffect(EFFECT_LINE);     // Enable scanlines
PLAT_setEffect(EFFECT_GRID);     // Enable LCD grid
PLAT_setEffect(EFFECT_CRT);      // Enable CRT phosphor effect
PLAT_setEffect(EFFECT_SLOT);     // Enable slot mask
PLAT_setEffect(EFFECT_NONE);     // Disable effects
```

The effect scale is automatically determined by the current rendering scale (game resolution vs screen resolution).

## Technical Notes

### Why Shadow-Only?

Early implementations used colored overlays (red/green tints) to simulate phosphor colors, but this caused color washing and reduced contrast. Pure black shadows preserve the original game colors while adding authentic display structure.

### Per-Pixel Alpha

All effects use per-pixel alpha values baked into the generated pattern. This allows fine control over intensity across different parts of the pattern (e.g., darker scanline gaps, lighter phosphor regions in CRT effect).

### Pixel Grid Alignment

All patterns tile at exact multiples of the game pixel scale:

- Width: multiple of `scale`
- Height: multiple of `scale` (may be `scale * 2` for staggered patterns)

This ensures effects stay locked to game pixels with no drift or moiré.

### Performance

Patterns are generated once per effect change and cached. Runtime cost is minimal - just one additional texture overlay per frame.
