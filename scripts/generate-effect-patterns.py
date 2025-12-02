#!/usr/bin/env python3
"""
Generate effect pattern PNGs for CRT/LCD effects.

All patterns use shadow-based approach (black pixels, varying alpha)
to darken the image without color washing.

Pattern Types:
- Lines: Horizontal scanline shadow (existing)
- Grid: 2×2 pixel grid (existing)
- Grille: Aperture grille / Trinitron-style vertical stripes
- Slot: Slot mask (consumer TV-style)
- Dot: Dot/shadow mask (triad phosphor pattern)
- DMG: Game Boy DMG LCD (chunky pixel borders)
- GBC: Game Boy Color LCD (finer grid)
- LCD: RGB stripe LCD (GBA SP-style)

Usage:
    python3 scripts/generate-effect-patterns.py
"""

from PIL import Image
import os

# Output directory
OUT_DIR = "skeleton/SYSTEM/res"

# =============================================================================
# TUNING PARAMETERS - adjust these to taste
# =============================================================================

# All patterns use opaque black (255) and control visibility via global opacity.
# This allows per-scale opacity tuning in the C code.

# Lines: horizontal scanline shadow
LINES_SHADOW = 255

# Grid: 2×2 pixel grid
GRID_ALPHA = 255

# Grille: Aperture grille (Trinitron-style)
# Vertical shadow bars between phosphor stripes
# Phosphor = transparent (light passes), Bar = shadow (darkens)
GRILLE_BAR = 255       # Opaque shadow on vertical bars

# Slot Mask: Consumer TV-style
# Metal mask with slot openings
# Slot = transparent (light passes), Metal = shadow (darkens)
SLOT_METAL = 255       # Opaque shadow on mask metal

# Dot Mask: Triad-like phosphor dots
# Phosphor dots with metal mask between
# Dot = transparent (light passes), Metal = shadow (darkens)
DOT_METAL = 255        # Opaque shadow on mask metal

# DMG LCD: Original Game Boy (gray brick)
# Large, chunky pixel grid with thick borders
# Cell = transparent (light passes), Border = shadow (darkens)
DMG_BORDER = 255       # Opaque border shadow

# GBC LCD: Game Boy Color
# Finer grid, lighter borders
# Cell = transparent (light passes), Border = shadow (darkens)
GBC_BORDER = 255       # Opaque border shadow

# LCD Stripe: RGB stripe LCD (GBA SP-style)
# Subtle vertical shadow stripes between subpixels
# Subpixel = transparent (light passes), Gap = shadow (darkens)
LCD_GAP = 255          # Opaque gap shadow

# =============================================================================
# Pattern Generation
# =============================================================================

def generate_lines_scale(scale):
    """
    Generate 1×N scanline pattern for specific pixel scale.

    For scale N, creates N-pixel tall pattern with:
    - Row 0: Shadow (LINES_SHADOW alpha)
    - Rows 1 to N-1: Transparent (no darkening)

    This ensures one scanline shadow per scaled game pixel.
    """
    img = Image.new('RGBA', (1, scale))
    img.putpixel((0, 0), (0, 0, 0, LINES_SHADOW))
    for y in range(1, scale):
        img.putpixel((0, y), (0, 0, 0, 0))
    return img

def generate_grid_scale(scale):
    """
    Generate 2×N grid pattern for specific pixel scale.

    Base 2×2 pattern (3 opaque corners, 1 transparent) is scaled
    up to 2×(2*scale) to match pixel scaling.
    """
    size = 2 * scale
    img = Image.new('RGBA', (size, size))

    for y in range(size):
        for x in range(size):
            base_x = x // scale
            base_y = y // scale
            # Bottom-right corner is transparent, rest opaque
            if base_x == 1 and base_y == 1:
                img.putpixel((x, y), (0, 0, 0, 0))
            else:
                img.putpixel((x, y), (0, 0, 0, GRID_ALPHA))
    return img

def generate_grille_scale(scale):
    """
    Generate aperture grille pattern (Trinitron-style).

    Tile Size: 2×1 (alternating phosphor/bar columns)

    Pattern:
    | Pixel | Description        | Alpha      |
    |-------|--------------------|------------|
    | 0     | Phosphor stripe    | 0 (clear)  |
    | 1     | Vertical bar       | 255 (dark) |

    Scale determines height (tiles vertically at 1:1).
    """
    img = Image.new('RGBA', (2, scale))
    for y in range(scale):
        img.putpixel((0, y), (0, 0, 0, 0))            # Phosphor (transparent)
        img.putpixel((1, y), (0, 0, 0, GRILLE_BAR))   # Bar (shadow)
    return img

def generate_slot_scale(scale):
    """
    Generate slot mask pattern (consumer TV-style).

    Tile Size: 4×3 (scaled by pixel scale)

    Pattern Layout:
    Row 0: metal metal metal metal   (shadow bar)
    Row 1: metal slot  slot  metal   (vertical bars + clear slot)
    Row 2: metal metal metal metal   (shadow bar)

    Metal = shadow (alpha=255), Slot = clear (alpha=0)
    """
    base_w, base_h = 4, 3
    img = Image.new('RGBA', (base_w * scale, base_h * scale))

    for y in range(base_h * scale):
        for x in range(base_w * scale):
            base_x = x // scale
            base_y = y // scale

            # Row 0 and 2: all metal (shadow)
            if base_y == 0 or base_y == 2:
                img.putpixel((x, y), (0, 0, 0, SLOT_METAL))
            # Row 1: metal on edges (shadow), slot interior clear
            elif base_y == 1:
                if base_x == 0 or base_x == 3:
                    img.putpixel((x, y), (0, 0, 0, SLOT_METAL))
                else:
                    img.putpixel((x, y), (0, 0, 0, 0))  # Slot (transparent)
    return img

def generate_dot_scale(scale):
    """
    Generate dot/shadow mask pattern (triad-like).

    Tile Size: 3×3 (scaled by pixel scale)

    Pattern:
    [ metal, dot,   metal ]
    [ dot,   metal, dot   ]
    [ metal, dot,   metal ]

    Dot = clear (alpha=0), Metal = shadow (alpha=255)
    Creates a diamond/staggered pattern of clear holes.
    """
    base_size = 3
    img = Image.new('RGBA', (base_size * scale, base_size * scale))

    # Base 3x3 pattern: 1 = dot hole (clear), 0 = metal (shadow)
    base_pattern = [
        [0, 1, 0],
        [1, 0, 1],
        [0, 1, 0],
    ]

    for y in range(base_size * scale):
        for x in range(base_size * scale):
            base_x = x // scale
            base_y = y // scale
            if base_pattern[base_y][base_x] == 1:
                img.putpixel((x, y), (0, 0, 0, 0))          # Dot hole (transparent)
            else:
                img.putpixel((x, y), (0, 0, 0, DOT_METAL))  # Metal (shadow)
    return img

def generate_dmg_scale(scale):
    """
    Generate Game Boy DMG LCD pattern.

    Tile Size: 3×3 (scaled by pixel scale)

    Pattern:
    [ border, border, border ]
    [ border, cell,   border ]
    [ border, border, border ]

    Cell = clear (alpha=0), Border = shadow (alpha=255)
    Large, visible LCD pixels with thick dark borders.
    """
    base_size = 3
    img = Image.new('RGBA', (base_size * scale, base_size * scale))

    for y in range(base_size * scale):
        for x in range(base_size * scale):
            base_x = x // scale
            base_y = y // scale
            # Only center pixel is cell (clear), rest is border (shadow)
            if base_x == 1 and base_y == 1:
                img.putpixel((x, y), (0, 0, 0, 0))            # Cell (transparent)
            else:
                img.putpixel((x, y), (0, 0, 0, DMG_BORDER))   # Border (shadow)
    return img

def generate_gbc_scale(scale):
    """
    Generate Game Boy Color LCD pattern.

    Tile Size: 2×2 (scaled by pixel scale)

    Pattern:
    [ border, border ]
    [ border, cell   ]

    Cell = clear (alpha=0), Border = shadow (alpha=255)
    Finer grid than DMG.
    """
    base_size = 2
    img = Image.new('RGBA', (base_size * scale, base_size * scale))

    for y in range(base_size * scale):
        for x in range(base_size * scale):
            base_x = x // scale
            base_y = y // scale
            # Bottom-right is cell (clear), rest is border (shadow)
            if base_x == 1 and base_y == 1:
                img.putpixel((x, y), (0, 0, 0, 0))            # Cell (transparent)
            else:
                img.putpixel((x, y), (0, 0, 0, GBC_BORDER))   # Border (shadow)
    return img

def generate_lcd_scale(scale):
    """
    Generate RGB stripe LCD pattern (GBA SP-style).

    Tile Size: 3×1 (vertical stripes, height = scale)

    Pattern:
    | Pixel | Description     | Alpha      |
    |-------|-----------------|------------|
    | 0     | Subpixel        | 0 (clear)  |
    | 1     | Gap             | 255 (dark) |
    | 2     | Subpixel        | 0 (clear)  |

    Subpixel = clear (alpha=0), Gap = shadow (alpha=255)
    Subtle vertical shadow stripes for modern handheld LCD look.
    """
    img = Image.new('RGBA', (3, scale))
    for y in range(scale):
        img.putpixel((0, y), (0, 0, 0, 0))         # Subpixel (transparent)
        img.putpixel((1, y), (0, 0, 0, LCD_GAP))   # Gap (shadow)
        img.putpixel((2, y), (0, 0, 0, 0))         # Subpixel (transparent)
    return img

def show_pattern(name, img):
    """Display pattern info."""
    print(f"\n{name}.png ({img.width}×{img.height}):")
    # Only show first 8 rows to keep output manageable
    max_rows = min(img.height, 8)
    for y in range(max_rows):
        row = []
        for x in range(img.width):
            a = img.getpixel((x, y))[3]
            if a == 0:
                row.append("  · ")
            elif a < 50:
                row.append(f"░{a:3d}")
            elif a < 100:
                row.append(f"▒{a:3d}")
            else:
                row.append(f"▓{a:3d}")
        print(f"  {''.join(row)}")
    if img.height > max_rows:
        print(f"  ... ({img.height - max_rows} more rows)")

def main():
    print("Generating effect patterns...")
    print(f"Output: {OUT_DIR}/")

    # Generate patterns for common pixel scales (2-8)
    scales = [2, 3, 4, 5, 6, 7, 8]

    generators = [
        ("line", generate_lines_scale),
        ("grid", generate_grid_scale),
        ("grille", generate_grille_scale),
        ("slot", generate_slot_scale),
        ("lcd", generate_lcd_scale),
    ]

    patterns = []
    for scale in scales:
        for name, gen_func in generators:
            patterns.append((f"{name}-{scale}", gen_func(scale)))

    for name, img in patterns:
        path = os.path.join(OUT_DIR, f"{name}.png")
        img.save(path, optimize=True)
        show_pattern(name, img)
        print(f"  Saved: {path} ({os.path.getsize(path)} bytes)")

    print("\n" + "=" * 60)
    print(f"Generated {len(patterns)} pattern files for scales: {scales}")
    print("\nPattern types:")
    for name, _ in generators:
        print(f"  {name.upper()}")
    print("\nAll patterns use shadow-only approach:")
    print("  - Clear pixels (alpha=0) where light passes through")
    print("  - Shadow pixels (alpha=255) where darkening occurs")
    print("  - Global opacity controlled via EFFECT_getOpacity()")
    print("=" * 60)

if __name__ == "__main__":
    main()
