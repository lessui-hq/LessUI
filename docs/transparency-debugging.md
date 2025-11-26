# Asset Transparency Debugging History

**Problem:** Assets showing black backgrounds instead of transparency on rg35xxplus.

**Status:** Works on tg5040, fails on rg35xxplus

**Note:** This issue is INDEPENDENT of the +1 tier scaling change. The +1 tier change just selects a larger source asset - it doesn't affect transparency handling. We discovered this transparency bug while testing the +1 tier improvement.

---

## Platform Differences

### Working Platform: tg5040
- Resolution: 1280x720 (or 1024x768 Brick variant)
- SDL Version: SDL 1.2
- Pixel Format: TBD (need to verify)

### Failing Platform: rg35xxplus
- Resolution: 640x480 (or 720x720 CubeXX, 720x480 RG34XX)
- SDL Version: SDL 1.2
- Pixel Format: **RGB565 (16-bit, no alpha channel)**
- Key difference: No alpha mask in surface format

---

## Root Cause Analysis

When surfaces don't have an alpha channel (RGB565):
- RGBA(0,0,0,0) transparent black → RGB(0,0,0) solid black
- Alpha channel is discarded during `SDL_ConvertSurface()` to RGB565
- Transparency must be handled differently for non-alpha formats

---

## Attempted Solutions

### Attempt 0: Original +1 tier implementation (NO transparency handling)
**File:** `workspace/all/common/api.c` line 390
**Code:**
```c
int asset_scale = (int)ceilf(gfx_dp_scale) + 1; // +1 tier for quality

// Asset scaling code with NO special transparency handling
SDL_Surface* temp = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
SDL_Surface* surface = SDL_ConvertSurface(temp, loaded_assets->format, 0);
SDL_FreeSurface(temp);
// No SetAlpha, no FillRect, just basic create/convert/blit
```
**Result:** ✅ Assets visible on rg35xxplus, ❌ but backgrounds were BLACK instead of transparent

**Why it partially worked:**
- Assets loaded and displayed correctly
- But black backgrounds because SDL_CreateRGBSurface with (0,0,0,0) masks defaults to undefined/black pixels
- The issue: we were trying to FIX this black background problem

**Important:** This proves the +1 tier scaling itself works, just need proper transparency

---

### Attempt 1: SDL_SetSurfaceBlendMode() (SDL2 only)
**File:** `workspace/all/common/api.c` lines ~450, 457, 507, 518
**Code:**
```c
SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
```
**Result:** ❌ Compile error on SDL 1.2 platforms (function doesn't exist)

---

### Attempt 2: Platform-specific #ifdef blocks
**File:** `workspace/all/common/api.c` lines ~450-465, 507-534
**Code:**
```c
#ifdef USE_SDL2
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
#else
    SDL_SetAlpha(surface, 0, 0); // SDL 1.2
#endif
SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
```
**Result:** ✅ Compiles on both SDL versions, but ❌ still shows black on rg35xxplus

**Why it failed:**
- Filling with RGBA(0,0,0,0) becomes solid black when converted to RGB565
- The FillRect happens BEFORE conversion, so it fills with transparent black
- But after conversion to RGB565, transparent black → solid black

---

### Attempt 3: Use SDLX_SetAlpha() compatibility macro
**File:** `workspace/all/common/api.c` via `workspace/all/common/sdl.h`
**Code:**
```c
SDLX_SetAlpha(surface, 0, 0); // Uses sdl.h compat layer
SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
```
**Result:** ✅ Compiles cleanly, ❌ still shows black on rg35xxplus

**Why it failed:** Same issue as Attempt 2 - FillRect with transparent black becomes solid black in RGB565

---

### Attempt 4: Remove FillRect, rely on SDL blit behavior (CURRENT)
**File:** `workspace/all/common/api.c` lines 445-505
**Code:**
```c
// Create RGBA8888 surface
SDL_Surface* temp = SDL_CreateRGBSurface(0, w, h, 32, RGBA_MASK_8888);
// Convert to loaded_assets format (may be RGB565)
SDL_Surface* surface = SDL_ConvertSurface(temp, loaded_assets->format, 0);
SDL_FreeSurface(temp);
// NO FillRect - let SDL handle transparency through blitting
SDL_BlitSurface(source, &src_rect, surface, &dst_rect);
```
**Result:** ✅ Compiles, 🔄 **Testing on rg35xxplus...**

**Theory:**
- Don't pre-fill surfaces with any color
- Let SDL's blitting preserve whatever transparency exists in source
- Avoids the RGBA→RGB565 conversion issue

---

## Key Technical Details

### SDL_ConvertSurface() Behavior
When converting RGBA8888 → RGB565:
- Alpha channel is **discarded** (no space for it in RGB565)
- RGB(0,0,0) in RGB565 = solid black
- Transparency in RGB565 requires **color keying**, not alpha

### Color Keying vs Alpha Blending
**Alpha Blending** (RGBA formats):
- Per-pixel alpha values (0-255)
- Smooth gradients, antialiasing
- Used in: RGBA8888, some RGBA formats

**Color Keying** (RGB formats):
- One specific RGB color = transparent (e.g., magenta RGB(255,0,255))
- Binary transparency (either visible or invisible)
- Used in: RGB565 when alpha not available
- Set with `SDL_SetColorKey(surface, SDL_SRCCOLORKEY, color_key)`

### The rg35xxplus Problem
rg35xxplus uses RGB565, so:
- No alpha channel available
- Our PNG assets have alpha, but it's lost during conversion
- **SDL should handle this automatically** by preserving source alpha during blit
- But pre-filling with "transparent black" becomes solid black in RGB565

---

## Next Steps If Attempt 4 Fails

### Option A: Use color keying for RGB formats
Detect if surface has no alpha mask, and use a specific color as transparent:
```c
if (!surface->format->Amask) {
    // RGB format - use color keying
    uint32_t magenta = SDL_MapRGB(surface->format, 255, 0, 255);
    SDL_SetColorKey(surface, SDL_SRCCOLORKEY, magenta);
    SDL_FillRect(surface, NULL, magenta);
}
```

### Option B: Keep surfaces in RGBA until final blit to screen
Don't convert to loaded_assets format until we're done with all transformations:
```c
// Work in RGBA8888 throughout
SDL_Surface* rgba_assets = /* keep in RGBA */;
// Only convert to RGB565 when blitting to screen
```

### Option C: Check what worked before +1 tier change
Revert the +1 tier change temporarily and verify transparency works, then investigate what's different about the asset loading path for the higher tier.

---

## Test Checklist

When testing transparency fixes:

- [ ] Test on rg35xxplus (RGB565, SDL 1.2)
- [ ] Test on tg5040 (working baseline)
- [ ] Test on miyoomini (RGB565, SDL 1.2)
- [ ] Test on desktop (RGBA, SDL2)
- [ ] Verify icons (battery, wifi, brightness, volume) show transparency
- [ ] Verify pill backgrounds don't have black boxes
- [ ] Check button graphics

---

## Build Commands

```bash
# Test specific platforms
make PLATFORM=rg35xxplus build
make PLATFORM=tg5040 build
make PLATFORM=miyoomini build

# Test desktop
make dev
```

---

## References

- RGB565 format: 16-bit color, 5 bits red, 6 bits green, 5 bits blue, **0 bits alpha**
- RGBA8888 format: 32-bit color, 8 bits per channel including alpha
- SDL_ConvertSurface: Converts pixel formats, discards alpha if destination has no Amask
- SDLX_SetAlpha macro: `workspace/all/common/sdl.h:54-67`
