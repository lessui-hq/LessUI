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

# Lines: horizontal scanline shadow with dimensional falloff
LINES_LEADIN = 128      # Light lead-in before scanline
LINES_SHADOW = 255      # Primary scanline (dark) - full opacity like MinUI
LINES_TRAIL = 64        # Very light trail after scanline

# Grid: 2×2 pixel grid
GRID_ALPHA = 255

# Grille: Aperture grille (Trinitron-style)
# Vertical shadow bars between phosphor stripes
# Phosphor = transparent (light passes), Bar = shadow (darkens)
GRILLE_BAR = 255       # Opaque shadow on vertical bars
GRILLE_GLOW = 60       # Phosphor edge glow near bars

# Slot Mask: Consumer TV-style
# Metal mask with slot openings
# Slot = transparent (light passes), Metal = shadow (darkens)
SLOT_METAL = 255       # Opaque shadow on mask metal
SLOT_GLOW = 60         # Phosphor edge glow inside slots

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
    Generate scanline pattern for specific pixel scale.

    Tile size: scale × scale (aligned to pixel grid)

    Pattern based on perfect CRT overlay:
    - At scale 2: Simple 1-row scanline (row 0: alpha 255)
    - At scale 3+: 3-row dimensional scanline
      - Row 0: Light lead-in (alpha 30)
      - Row 1: Main dark scanline (alpha 80)
      - Row 2: Very light trail (alpha 10)
      - Rows 3+: Transparent

    The 3-row pattern creates CRT phosphor glow with dimensional falloff.
    """
    img = Image.new('RGBA', (scale, scale))
    for y in range(scale):
        for x in range(scale):
            if scale == 2:
                # Scale 2: simple scanline
                if y == 0:
                    img.putpixel((x, y), (0, 0, 0, 255))
                else:
                    img.putpixel((x, y), (0, 0, 0, 0))
            else:
                # Scale 3+: dimensional scanline with lead-in and trail
                if y == 0:
                    img.putpixel((x, y), (0, 0, 0, LINES_LEADIN))
                elif y == 1:
                    img.putpixel((x, y), (0, 0, 0, LINES_SHADOW))
                elif y == 2:
                    img.putpixel((x, y), (0, 0, 0, LINES_TRAIL))
                else:
                    img.putpixel((x, y), (0, 0, 0, 0))
    return img

def generate_grid_scale(scale):
    """
    Generate NxN LCD pixel grid pattern for specific pixel scale.

    Creates a scale×scale tile where:
    - Row 0: all black (horizontal border between pixels)
    - Column 0: all black (vertical border between pixels)
    - Rest: transparent (the LCD pixel interior where light passes)

    This mimics the appearance of LCD subpixel borders on handheld screens.
    When tiled, creates a grid of scale×scale "pixels" with 1px dark borders.
    """
    img = Image.new('RGBA', (scale, scale))

    for y in range(scale):
        for x in range(scale):
            # Row 0 or column 0 = border (black opaque)
            # Everything else = pixel interior (transparent)
            if y == 0 or x == 0:
                img.putpixel((x, y), (0, 0, 0, GRID_ALPHA))
            else:
                img.putpixel((x, y), (255, 255, 255, 0))
    return img

def generate_grille_scale(scale):
    """
    Generate aperture grille pattern (Trinitron-style).

    Tile size: scale × scale (aligned to pixel grid)

    Alternating phosphor stripes and vertical bars (50% each).
    - First half: phosphor (transparent) with edge glow near bar
    - Second half: vertical bar (shadow)

    At scale 2: Simple stripe (no room for glow)
    At scale 3+: Add subtle glow on rightmost phosphor column
    """
    img = Image.new('RGBA', (scale, scale))
    mid = scale // 2
    for y in range(scale):
        for x in range(scale):
            if x < mid:
                # Phosphor stripe - add glow on right edge (next to bar) for scale 3+
                if x == mid - 1 and scale >= 3:
                    img.putpixel((x, y), (0, 0, 0, GRILLE_GLOW))
                else:
                    img.putpixel((x, y), (0, 0, 0, 0))
            else:
                img.putpixel((x, y), (0, 0, 0, GRILLE_BAR))   # Bar (shadow)
    return img

def generate_slot_scale(scale):
    """
    Generate slot mask pattern (consumer TV-style) with staggered slots.

    Tile must align to pixel grid: scale × (scale * 2)
    Single column of slots, staggered between top and bottom rows.

    At scale 2: Simple slots (no room for glow)
    At scale 3+: Add subtle phosphor glow on edges adjacent to metal borders

    At scale 4 (4×8 tile):
      ████    Row 0: top border
      █▒··    Row 1: border, glow, slot opening
      █···    Rows 2-3: border + slot opening
      ████    Row 4: middle border
      ··▒█    Row 5: slot opening, glow, border (staggered)
      ···█    Rows 6-7: slot opening + border

    Phosphor glow creates softer edges on the slot openings.
    """
    tile_w = scale
    tile_h = scale * 2
    mid_y = scale  # Divider between top and bottom rows

    img = Image.new('RGBA', (tile_w, tile_h))

    for y in range(tile_h):
        for x in range(tile_w):
            # Top borders (row 0 and row mid_y)
            if y == 0 or y == mid_y:
                img.putpixel((x, y), (0, 0, 0, SLOT_METAL))
                continue

            # Top half (rows 1 to mid_y-1): border on LEFT, slot on right
            if 1 <= y < mid_y:
                if x == 0:
                    img.putpixel((x, y), (0, 0, 0, SLOT_METAL))  # Left border
                # Add glow on first row after border (vertical beam falloff)
                elif y == 1 and scale >= 3:
                    img.putpixel((x, y), (0, 0, 0, SLOT_GLOW))
                else:
                    img.putpixel((x, y), (255, 255, 255, 0))      # Slot opening

            # Bottom half (rows mid_y+1 to end): slot on left, border on RIGHT (staggered)
            else:
                if x == tile_w - 1:
                    img.putpixel((x, y), (0, 0, 0, SLOT_METAL))  # Right border
                # Add glow on first row after border (vertical beam falloff)
                elif y == mid_y + 1 and scale >= 3:
                    img.putpixel((x, y), (0, 0, 0, SLOT_GLOW))
                else:
                    img.putpixel((x, y), (255, 255, 255, 0))      # Slot opening

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

    Tile size: scale × scale (aligned to pixel grid)

    Thin vertical shadow stripe in the middle (~33% of width).
    Similar to grille but thinner shadow lines (more light passes through).

    At scale 2: Can't do 33% width with 1px, so use reduced opacity
                to simulate the lighter/softer look (170 vs 255 = ~66%)
    At scale 3+: ~33% shadow width, full opacity
    """
    img = Image.new('RGBA', (scale, scale))

    # Calculate shadow stripe position (centered, ~1/3 width)
    shadow_width = max(1, scale // 3)
    mid = scale // 2
    shadow_start = mid - shadow_width // 2
    shadow_end = shadow_start + shadow_width

    # At scale 2, we can't make the stripe thinner, so reduce opacity instead
    # to simulate ~33% coverage vs grille's 50% (170/255 ≈ 66%)
    alpha = 170 if scale == 2 else LCD_GAP

    for y in range(scale):
        for x in range(scale):
            if shadow_start <= x < shadow_end:
                img.putpixel((x, y), (0, 0, 0, alpha))   # Gap (shadow)
            else:
                img.putpixel((x, y), (0, 0, 0, 0))       # Subpixel (transparent)
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
