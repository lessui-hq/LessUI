# DP (Display Points) Best Practices

## Quick Reference

| Need                        | Use                                                   |
| --------------------------- | ----------------------------------------------------- |
| Screen dimensions for UI    | `ui.screen_width`, `ui.screen_height` (DP)            |
| Screen dimensions in pixels | `ui.screen_width_px`, `ui.screen_height_px`           |
| Convert DP to pixels        | `DP(value)` or `DP_RECT(x,y,w,h)`                     |
| Convert pixels to DP        | `PX_TO_DP(value)`                                     |
| Draw pill at DP coords      | `GFX_blitPill_DP(asset, dst, x_dp, y_dp, w_dp, h_dp)` |
| Draw rect at DP coords      | `GFX_blitRect_DP(asset, dst, x_dp, y_dp, w_dp, h_dp)` |
| Draw message at DP coords   | `GFX_blitMessage_DP(font, msg, dst, x, y, w, h)`      |
| Get text size in DP         | `TTF_SizeUTF8_DP(font, text, &w_dp, &h_dp)`           |
| Blit at DP position         | `SDL_BlitSurface_DP(src, srcrect, dst, x_dp, y_dp)`   |

## Overview

LessUI uses a DP (display points) system similar to Android's density-independent pixel system to ensure consistent UI sizing across devices with different screen densities. This document establishes best practices for working with DP correctly.

## Core Concepts

### What are DP?

- **DP (Display Points)**: Abstract units representing a consistent physical size across devices
- **Pixels**: Physical screen pixels that vary by device density
- **Conversion**: `pixels = dp × scale_factor` (via `DP()` macro)

### Why DP?

- A 30dp button looks the same physical size on a 480p screen and a 720p screen
- UI layouts work consistently across all supported devices (12+ platforms)
- Eliminates per-device manual tuning

### Why 144 PPI as Baseline?

The baseline density of 144 PPI was chosen for two key reasons:

**1. Optimal readability for handheld devices**

At typical handheld viewing distances (25-30cm), 144 PPI produces ideal text sizes:

| 16dp text height | Visual angle @ 27cm |
| ---------------- | ------------------- |
| 2.83mm           | ~36 arcmin          |

ISO recommends 20-22 arcmin minimum for comfortable reading. 36 arcmin is well within the "super readable" zone while remaining compact for small screens.

**2. Perfect typographic relationship**

144 PPI creates an elegant mapping to traditional typography:

- Typography defines 1 point = 1/72 inch
- At 144 PPI: **1 pt = 2 px** (in DP terms)
- Therefore: **1 dp = 0.5 pt**

This yields clean conversions to standard typographic sizes:

| DP  | Points | Common Use   |
| --- | ------ | ------------ |
| 16  | 8 pt   | Small text   |
| 24  | 12 pt  | Body text    |
| 32  | 16 pt  | Headers      |
| 48  | 24 pt  | Large titles |

## The Fundamental Rule

**If it needs to scale across devices, it MUST use DP. Otherwise the scaling will be wrong.**

This is non-negotiable. Any UI element that should maintain consistent physical size across devices (buttons, padding, text size, menu items, etc.) must be defined and calculated in DP.

## The Rules

### Rule 1: UI Layout MUST Use DP

**If it needs to scale across devices, it MUST use DP-based values.**

- ✅ Use `ui.screen_width` and `ui.screen_height` (in DP)
- ❌ Don't use `screen->w` and `screen->h` (raw pixels) for UI layout

### Rule 2: Pixels and DP are Compatible

**DP() converts to pixels, so you can mix them in calculations.**

```c
int total_px = DP(ui.padding) + text_width_px;  // Both are pixels ✓
```

### Rule 3: Mark Your Units Clearly

**Use variable naming to show what units you're working in.**

- `menu_height_dp` - In DP units
- `text_width_px` - In pixels
- `centered_x_px` - In pixels (result of pixel-based calculation)

### Rule 4: Screen Dimensions

**For UI layout:**

- ✅ Use `ui.screen_width` and `ui.screen_height` (DP)
- ❌ Don't use `screen->w` and `screen->h` (raw pixels)

**Exception:** Emulation rendering, video scaling, DEVICE\_\* constants can use raw pixels.

## Correct Patterns

### ✅ Pattern 1: Use DP-native wrapper functions (PREFERRED)

```c
// Calculate in DP
int menu_height_dp = ui.padding + (MENU_ITEM_COUNT * ui.pill_height);
int y_offset_dp = (ui.screen_height - menu_height_dp) / 2;
int item_y_dp = y_offset_dp + ui.padding + (i * ui.pill_height);

// Use DP wrapper - clean and clear!
GFX_blitPill_DP(ASSET_WHITE_PILL, screen, ui.padding, item_y_dp, width_dp, ui.pill_height);
```

**Why this is best:**

- Clearest code - no manual DP() conversions
- Matches Android/iOS pattern
- Impossible to accidentally mix units
- All arithmetic stays in DP units

### ✅ Pattern 2: Manual DP conversion (when wrapper not available)

```c
// Calculate in DP
int item_y_dp = y_offset_dp + ui.padding + (i * ui.pill_height);

// Convert to pixels when passing to SDL (if no DP wrapper exists)
SDL_Rect rect = {DP(ui.padding), DP(item_y_dp), DP(width_dp), DP(ui.pill_height)};
GFX_blitPill(asset, screen, &rect);
```

**When to use:**

- Wrapper function doesn't exist yet
- Working with direct SDL calls

### ✅ Pattern 2: Use ui.screen_width and ui.screen_height

```c
// CORRECT - work in DP
int centered_x_dp = (ui.screen_width - element_width_dp) / 2;
int centered_y_dp = (ui.screen_height - element_height_dp) / 2;

// Convert to pixels when rendering
SDL_Rect rect = {DP(centered_x_dp), DP(centered_y_dp), DP(element_width_dp), DP(element_height_dp)};
```

**Why this works:**

- `ui.screen_width` and `ui.screen_height` are provided in DP
- All centering math stays in DP
- Clean separation of concerns

### ✅ Pattern 3: Mixing pixels with DP (when necessary)

```c
// Get pixel measurement from SDL_TTF
int text_width_px;
TTF_SizeUTF8(font, text, &text_width_px, NULL);

// DP converts to pixels, so we can mix them
int item_width_px = text_width_px + DP(ui.button_padding * 2);  // pixels + pixels = OK

// Use DP-based screen dimensions
int centered_x_px = (DP(ui.screen_width) - item_width_px) / 2;  // pixels - pixels = OK

// Use in rendering
SDL_Rect rect = {centered_x_px, DP(y_dp), item_width_px, DP(ui.button_size)};
```

**Why this works:**

- `DP()` converts to pixels, so `DP(ui.button_padding * 2)` is pixels
- Variables clearly marked with `_px` suffix
- All pixel math is valid (pixels + pixels, pixels - pixels)
- Layout is still driven by DP values (ui.button_padding, ui.screen_width, etc.)

**Key principle:** DP values define the layout, but we can convert them to pixels early if needed for calculations involving pixel-based measurements (like text width).

## Incorrect Patterns (What NOT to Do)

### ❌ Anti-Pattern 1: Using raw screen dimensions for UI layout

```c
// WRONG - screen->w and screen->h are raw pixels, not scaled for density
int available_width = screen->w - DP(ui.padding * 2);
int centered_y = (screen->h - menu_height) / 2;

// CORRECT - use DP-based screen dimensions
int available_width_px = DP(ui.screen_width - ui.padding * 2);
int centered_y_dp = (ui.screen_height - menu_height_dp) / 2;
```

**Why the first is wrong:**

- `screen->w` and `screen->h` are raw pixel dimensions
- They don't scale properly - a calculation using raw screen pixels will give different results on 480p vs 720p
- **The fundamental rule is violated**: UI layout must scale, so it must use DP
- Use `ui.screen_width` and `ui.screen_height` instead (these are in DP)

### ❌ Anti-Pattern 2: Converting pixels back to DP

```c
// WRONG - converting pixels to DP via division
int offset_px = (screen->h - DP(menu_height_dp)) / 2;
int offset_dp = offset_px / DP(1);  // Hacky!

// CORRECT - stay in DP
int offset_dp = (ui.screen_height - menu_height_dp) / 2;
```

**Why the first is wrong:**

- Introduces rounding errors
- Convoluted logic
- Signal that you started with wrong units

### ❌ Anti-Pattern 3: Wrapping already-converted values

```c
// WRONG - double conversion
int oy_px = DP(ui.padding);  // Now in pixels
SDL_Rect rect = {0, DP(oy_px + ui.padding), ...};  // Treats pixels as DP!

// CORRECT - convert once at the end
int oy_dp = ui.padding;
SDL_Rect rect = {0, DP(oy_dp + ui.padding), ...};
```

**Why the first is wrong:**

- Double conversion causes incorrect scaling
- Variables with `_px` suffix shouldn't go inside DP()

## Variable Naming Convention

To avoid confusion, use clear suffixes:

```c
int menu_height_dp;      // In DP units
int menu_height_px;      // In pixels
int centered_x_dp;       // In DP units
int text_width_px;       // In pixels (from TTF_SizeUTF8)
```

**When suffix is omitted**, the default assumption should be DP for layout calculations.

## Special Cases

### Text Rendering (SDL_TTF)

TTF functions return pixel values, not DP:

```c
SDL_Surface* text = TTF_RenderUTF8_Blended(font, str, color);
int text_width_px = text->w;   // This is in pixels!
int text_height_px = text->h;  // This is in pixels!

// When positioning text, need to be careful
int x_dp = ui.padding;
SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){DP(x_dp), DP(y_dp), 0, 0});
// text->w and text->h are used by SDL_BlitSurface internally (already pixels)
```

### Centering Text

When you need to center text that's measured in pixels:

```c
// Option 1: Convert DP container to pixels, center in pixel space
int container_width_px = DP(container_width_dp);
int text_x_px = (container_width_px - text->w) / 2;
SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){text_x_px, DP(y_dp), 0, 0});

// Option 2: Convert text width to DP (less common)
// Note: This introduces rounding, may not center perfectly
int text_width_dp = (int)(text->w / gfx_dp_scale + 0.5f);
int text_x_dp = (container_width_dp - text_width_dp) / 2;
SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){DP(text_x_dp), DP(y_dp), 0, 0});
```

**Recommendation**: When interfacing with SDL_TTF, it's acceptable to work in pixels for that specific calculation block, but clearly document it with variable names.

### Emulation Rendering vs UI Layout

**Important distinction in minarch.c:**

```c
// EMULATION RENDERING - Uses pixels directly (core video output)
void video_refresh_callback(const void* data, unsigned width, unsigned height, size_t pitch) {
    // width, height are in pixels - this is core video, not UI
    int dst_x = (screen->w - scaled_width) / 2;  // Pixels are correct here
    int dst_y = (screen->h - scaled_height) / 2;
    // ... blit emulation video ...
}

// UI LAYOUT - Should use DP
void renderMenu() {
    // UI elements should use DP
    int menu_y_dp = (ui.screen_height - menu_height_dp) / 2;
    SDL_Rect rect = {DP(ui.padding), DP(menu_y_dp), DP(width_dp), DP(height_dp)};
}
```

**Rule**: If it's rendering game/emulation content, pixels are fine. If it's UI layout (menus, buttons, text), use DP.

## Migration Strategy

When converting existing code:

1. **Identify the variable's purpose**: Is it for UI layout or emulation rendering?
2. **For UI layout**:
   - Replace `screen->w` with `ui.screen_width` (DP)
   - Replace `screen->h` with `ui.screen_height` (DP)
   - Work through all calculations in DP
   - Only wrap final values in `DP()` for SDL
3. **For emulation rendering**: Keep using `screen->w` and `screen->h` (pixels)
4. **Add comments**: Document which variables are DP vs pixels if it's not obvious

## Available DP Resources

### UI_Layout Struct (api.h)

```c
extern UI_Layout ui;

// Screen dimensions in DP
ui.screen_width      // Screen width in DP
ui.screen_height     // Screen height in DP

// Screen dimensions in pixels (cached for convenience)
ui.screen_width_px   // Screen width in pixels
ui.screen_height_px  // Screen height in pixels

// Layout parameters in DP
ui.pill_height       // Menu pill height in DP
ui.padding           // Edge padding in DP
ui.text_baseline     // Text vertical offset in DP
ui.button_size       // Button size in DP
ui.button_padding    // Button padding in DP
```

All values in `ui` are in DP units except the `*_px` variants. Use DP values directly in calculations.

### DP Conversion Macros

```c
// DP to pixels (most common)
DP(x)                          // Convert single DP value to pixels
DP2(a, b)                      // Convert two DP values: DP(a), DP(b)
DP3(a, b, c)                   // Convert three DP values
DP4(a, b, c, d)                // Convert four DP values
DP_RECT(x, y, w, h)            // Create SDL_Rect from DP coordinates

// Pixels to DP (for SDL_TTF results)
PX_TO_DP(px)                   // Convert pixels to DP

// Raw scale factor
gfx_dp_scale                   // e.g., 1.5 for 720p, 1.0 for 480p
```

### DP-Native Wrapper Functions

```c
// Use these instead of manual DP() conversions (cleaner code)
GFX_blitPill_DP(asset, dst, x_dp, y_dp, w_dp, h_dp);
GFX_blitRect_DP(asset, dst, x_dp, y_dp, w_dp, h_dp);
GFX_blitAsset_DP(asset, src_rect, dst, x_dp, y_dp);
GFX_blitBattery_DP(dst, x_dp, y_dp);
GFX_blitMessage_DP(font, msg, dst, x_dp, y_dp, w_dp, h_dp);

// SDL helpers
TTF_SizeUTF8_DP(font, text, &w_dp, &h_dp);  // Get text size in DP
SDL_BlitSurface_DP(src, srcrect, dst, x_dp, y_dp);  // Blit at DP position
```

## Examples

### Example 1: Centering a menu

```c
// Calculate menu dimensions in DP
int menu_width_dp = ui.screen_width - (ui.padding * 2);
int menu_height_dp = ui.padding + (item_count * ui.pill_height);

// Center in DP space
int menu_x_dp = ui.padding;  // Left-aligned with padding
int menu_y_dp = (ui.screen_height - menu_height_dp) / 2;

// Render items
for (int i = 0; i < item_count; i++) {
    int item_y_dp = menu_y_dp + ui.padding + (i * ui.pill_height);

    SDL_Rect rect = {
        DP(menu_x_dp),
        DP(item_y_dp),
        DP(menu_width_dp),
        DP(ui.pill_height)
    };

    GFX_blitPill(asset, screen, &rect);
}
```

### Example 2: Calculating available space

```c
// WRONG
int available_h = screen->h - DP(ui.padding * 2);

// CORRECT
int available_h_dp = ui.screen_height - (ui.padding * 2);
int available_h_px = DP(available_h_dp);
```

### Example 3: Positioning text with dynamic width

```c
// Text width comes from SDL_TTF (pixels)
SDL_Surface* text = TTF_RenderUTF8_Blended(font, str, color);
int text_width_px = text->w;

// Option A: Work in pixel space for this calculation
int container_width_px = DP(ui.screen_width - ui.padding * 2);
int text_x_px = ui.padding + (container_width_px - text_width_px) / 2;
SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){text_x_px, DP(y_dp), 0, 0});

// Option B: Convert text width to DP (may have rounding issues)
int text_width_dp = (int)(text_width_px / gfx_dp_scale + 0.5f);
int container_width_dp = ui.screen_width - ui.padding * 2;
int text_x_dp = ui.padding + (container_width_dp - text_width_dp) / 2;
SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){DP(text_x_dp), DP(y_dp), 0, 0});
```

**Recommendation**: Option A is cleaner when dealing with SDL_TTF.

## Common Mistakes and Fixes

### Mistake 1: Screen dimensions in calculations

```c
// BEFORE (wrong)
int oy = (screen->h - DP(menu_height_dp)) / 2 / DP(1);

// AFTER (correct)
int oy_dp = (ui.screen_height - menu_height_dp) / 2;
```

### Mistake 2: Double-wrapping DP values

```c
// BEFORE (wrong)
int height = DP((ui.padding * 2) + (count * ui.pill_height));

// AFTER (correct)
int height_dp = (ui.padding * 2) + (count * ui.pill_height);
int height_px = DP(height_dp);
```

### Mistake 3: Inconsistent units in expressions

```c
// BEFORE (wrong - mixes pixel screen->w with DP-converted padding)
int width = screen->w - DP(ui.padding * 2);
int centered = (screen->w - width) / 2;  // What units is width in?

// AFTER (correct - stay in DP)
int width_dp = ui.screen_width - (ui.padding * 2);
int centered_dp = (ui.screen_width - width_dp) / 2;
// Convert to pixels when rendering
SDL_Rect rect = {DP(centered_dp), DP(y_dp), DP(width_dp), DP(height_dp)};
```

## When to Use `screen->w` and `screen->h` (Raw Pixels)

**ONLY use raw screen pixel dimensions for:**

1. **Emulation video rendering** - Core output is in native pixels, not UI
2. **Backing up/restoring screen state** - Saving actual pixel dimensions
3. **Setting DEVICE_WIDTH/DEVICE_HEIGHT** - Platform constants
4. **Low-level pixel operations** - Direct framebuffer access, scaling operations

**For ALL UI layout, use `ui.screen_width` and `ui.screen_height` instead.**

## When Working in Pixels is OK

You can work in pixels for calculations when:

1. **You have pixel measurements** from SDL_TTF, surface->w/h, etc.
2. **The calculation involves only pixel values** (no DP mixing)
3. **The layout is still driven by DP** (DP values converted to pixels early)

**Example:**

```c
// Layout driven by DP, calculations in pixels (acceptable)
int text_width_px;
TTF_SizeUTF8(font, text, &text_width_px, NULL);
int item_width_px = text_width_px + DP(ui.button_padding * 2);  // DP converted to pixels
int x_px = (DP(ui.screen_width) - item_width_px) / 2;  // All pixels now

// DP values (ui.button_padding, ui.screen_width) drive the layout ✓
```

**Key**: If you're working in pixels, clearly mark variables with `_px` and ensure the layout is still fundamentally driven by DP values.

## Implementation Checklist

When writing or reviewing UI code:

- [ ] Are all layout dimensions calculated in DP?
- [ ] Do you use `ui.screen_width` and `ui.screen_height` instead of `screen->w/h`?
- [ ] Are `DP()` calls only at the final rendering step?
- [ ] Are variable names clear about their units (`_dp` or `_px` suffix)?
- [ ] If mixing pixels (e.g., from SDL_TTF), is the boundary clearly documented?

## FAQ

### Q: When should I use `screen->w` and `screen->h`?

**A:** Only for:

- Emulation video positioning/scaling
- Backing up/restoring screen state
- Setting `DEVICE_WIDTH`/`DEVICE_HEIGHT` constants
- Any non-UI rendering operations

For all UI layout, use `ui.screen_width` and `ui.screen_height`.

### Q: What if I need the screen size in pixels?

**A:** Convert it:

```c
int screen_width_px = DP(ui.screen_width);
int screen_height_px = DP(ui.screen_height);
```

### Q: Can I do arithmetic inside DP()?

**A:** Yes! These are equivalent:

```c
// Both valid
int total_px = DP(ui.padding) + DP(ui.pill_height);
int total_px = DP(ui.padding + ui.pill_height);
```

However, the second is preferred (fewer conversions, cleaner).

### Q: What about constants like SCROLL_WIDTH?

**A:** If they represent physical UI sizes, they should be in DP:

```c
#define SCROLL_WIDTH_DP 24
int x_px = DP((ui.screen_width - SCROLL_WIDTH_DP) / 2);
```

If they're pixel-specific (rare), document clearly:

```c
#define WINDOW_RADIUS_PX 4  // Pixel-specific, not scaled
```

## Summary

**The mantra: "DP in, DP throughout, pixels out"**

1. Start with DP values (`ui.*`, constants)
2. Do all calculations in DP
3. Convert to pixels with `DP()` only when passing to SDL functions
4. Use `ui.screen_width` and `ui.screen_height` for screen dimensions
5. Document clearly when you must work in pixels (SDL_TTF, video rendering)
