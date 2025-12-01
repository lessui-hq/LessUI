# CRT Effects Implementation Progress

## Status: Complete - Shadow-Based Approach

### ✅ Completed

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

### Key Learning: Shadow-Based Approach

**Problem with colored phosphor patterns:**
- Alpha blending + bright colored pixels = washed out tint
- Colored overlays (CRT_Basic, etc.) designed for multiplicative blending

**Solution: Dark shadows instead of colored phosphors:**
- All black pixels with varying alpha
- Low alpha = subtle shadow (phosphor/lit areas)
- High alpha = strong shadow (gaps/scanlines)
- Creates CRT structure through darkening, not tinting

### Current Patterns

**Lines (1×3):**
```
Row 0: α20  (light shadow)
Row 1: α130 (strong scanline)
Row 2: α0   (gap)
```

**CRT (6×1):**
```
α30 | α100 | α30 | α100 | α30 | α100
phos | gap  | phos | gap  | phos | gap
```

**Grid (2×2):** Unchanged - black pixels with transparent corner

### Tuning

Edit `scripts/generate-effect-patterns.py`:
```python
LINES_LIGHT = 20    # α for subtle shadow row
LINES_DARK = 130    # α for strong scanline row
LINES_GAP = 0       # α for gap row

CRT_PHOSPHOR = 30   # α for phosphor columns
CRT_GAP = 100       # α for gap columns
```

Then run: `python3 scripts/generate-effect-patterns.py`

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
