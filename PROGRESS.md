# CRT Effects Implementation Progress

## Status: In Progress - Overlay-Based Tiling System

### ✅ Phase 1: Pattern Design (Complete)

**Simplified Effect Menu:**
- Reduced from 6 effects to 4: None, Lines, Grid, CRT
- Removed EFFECT_APERTURE, EFFECT_SLOT, EFFECT_DOT from api.h
- Added single EFFECT_CRT
- Updated all 11 platform implementations
- Updated menu labels and description in minarch.c

**Pattern Files (all shadow-based):**
- line.png: 1×3 - horizontal scanlines with shadow gradient
- grid.png: 2×2 - pixel grid (unchanged)
- crt.png: 6×1 - vertical stripes (aperture-style)

**Pattern Generator Script:**
- `scripts/generate-effect-patterns.py` - generates all 3 patterns
- Easy tuning via parameters at top of script

**Cleanup:**
- Removed old patterns: aperture.png, slot.png, dot.png, slotmask.png
- Removed pre-scaled patterns: line-2.png through grid-11.png
- Removed old extraction scripts: extract-overlay-pattern.py, extract-pattern.py

### ✅ Phase 2: Overlay Tiling System (Complete)

**New Architecture:**
- All effects now use overlay compositing instead of scaler-based effects
- `effect_surface.c/h` - CPU-based pattern tiling (no scaling needed)
- Pattern files are pre-sized for each pixel scale, tiled at 1:1

**Pattern System:**
- **Pre-scaled patterns**: pattern-N.png for each scale N (2, 3, 4, 5, 6, 8)
- **No runtime scaling**: Patterns tiled at 1:1 to match scaled game pixels
- **Opaque shadows**: All patterns use alpha=255, visibility controlled via global opacity
- **One shadow per scanline**: Ensures authentic CRT simulation

**Pattern Examples:**
- `line-3.png`: 1×3 pixels (shadow on row 0, transparent rows 1-2)
- `grid-3.png`: 6×6 pixels (base 2×2 pattern scaled 3×)
- `crt-3.png`: 6×3 pixels (6-column phosphor pattern, 3 pixels tall)

**Key Changes:**
1. **miyoomini**: Removed old scaler-based effects, uses MI_GFX hardware blitter for overlay
2. **rg35xxplus**: Updated to use pre-scaled patterns instead of runtime scaling
3. Both platforms use identical opacity curves scaled by pixel scale

**Rendering Flow (miyoomini):**
```
1. Game renders at native resolution (e.g., 256×224)
2. PLAT_blitRenderer scales to vid.screen using NEON scalers
3. GFX_BlitSurfaceExec hardware-blits vid.screen to vid.video
4. updateEffectOverlay() loads pattern-{scale}.png and tiles to FIXED_WIDTH×FIXED_HEIGHT
5. GFX_BlitSurfaceExec composites overlay with MI_GFX alpha blending
6. SDL_Flip presents the final frame
```

**Opacity Scaling:**
All effects use the same opacity curve to keep fine patterns visible:
- scale=2: opacity=64 (wide spacing, subtle)
- scale=3-4: opacity=112-160
- scale=5+: opacity=112-144 (tight spacing, stronger for visibility)

### ✅ Completed Platforms

- [x] miyoomini - MI_GFX hardware blitter with ION memory
- [x] rg35xxplus - SDL2 texture rendering

### 🔄 Remaining Work

- [ ] Apply to other SDL1.2 platforms (trimuismart, rg35xx, tg5040, etc.)
- [ ] Apply to other SDL2 platforms
- [ ] Test on various games at different scales
- [ ] Fine-tune opacity values based on user feedback

### Key Discoveries

#### 1. Pattern Scaling Was The Root Issue

**Initial Problem:**
- Effects appeared too dark on miyoomini vs rg35xxplus
- Suspected MI_GFX alpha blending incompatibility

**Investigation:**
- Added diagnostic logging to measure before/after pixel values
- MI_GFX alpha blending tested at 98.6% accurate (compensation factor 1.015)
- Side-by-side photos revealed scanlines were different **thickness**, not just darkness

**Root Cause:**
- Original system: Pattern was scaled by pixel scale, creating N×(pattern_height) repeating unit
- Result: Scanlines every 9 pixels instead of every 3 pixels (for scale=3)
- This spread the same shadow alpha over 3× the area, making it appear darker

**Solution:**
- Pre-scale patterns to match each pixel scale (pattern-N.png)
- Tile at 1:1 (no runtime scaling)
- Result: One scanline shadow per scaled game pixel - perfect alignment

#### 2. Shadow-Based Approach

**Problem with colored phosphor patterns:**
- Alpha blending + bright colored pixels = washed out tint
- Colored overlays (CRT_Basic, etc.) designed for multiplicative blending

**Solution: Dark shadows instead of colored phosphors:**
- All black pixels with opaque alpha (255)
- Global opacity controls visibility per pixel scale
- Creates CRT structure through darkening, not tinting

### Current Pattern Design

All patterns use **opaque black** (alpha=255) for shadows, with visibility controlled via global opacity.

**Lines (pattern-N.png):**
- 1×N pixels: shadow row + (N-1) transparent rows
- Example line-3.png: `[α255, α0, α0]`
- Ensures one scanline shadow per scaled game pixel

**Grid (pattern-N.png):**
- (2×scale)×(2×scale) pixels
- Base 2×2 pattern (3 opaque corners, 1 transparent) scaled up
- Example grid-3.png: 6×6 with 3×3 quadrants

**CRT (pattern-N.png):**
- 6×N pixels: alternating shadow/transparent columns
- Columns 0,2,4: opaque (α255), Columns 1,3,5: transparent (α0)
- Simulates aperture grille phosphor structure

### Pattern Generation

Edit `scripts/generate-effect-patterns.py` to adjust shadow strength:
```python
LINES_SHADOW = 255  # Opaque black for scanlines
GRID_ALPHA = 255    # Opaque black for grid
CRT_SHADOW = 255    # Opaque black for CRT columns
```

Generate patterns for scales 2-8:
```bash
python3 scripts/generate-effect-patterns.py
```

Then tune visibility per-scale by adjusting opacity values in platform code.

### Pattern Sources Investigated

1. **CRT_Basic (aperture)** - 6×2 RGB stripes, α=65 - washed out
2. **CRT_Checkerboard** - 4×4 blocks - not a real slot mask
3. **CRT_Matrix (dot)** - 6×4 colored dots - washed out
4. **Perfect_CRT** - 3×3, α=5-83 - too subtle
5. **Dark shadow approach** - final solution, all black with varying alpha

### File Changes Summary

- api.h: Simplified effect enum (4 effects instead of 6)
- minarch.c: Updated labels and count
- All 11 platform/*.c files: Replaced APERTURE/SLOT/DOT with CRT
- skeleton/SYSTEM/res/: 3 pattern files
- scripts/generate-effect-patterns.py: New pattern generator
