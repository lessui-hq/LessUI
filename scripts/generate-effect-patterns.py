#!/usr/bin/env python3
"""
Generate effect pattern PNGs for CRT/scanline effects.

All patterns use shadow-based approach (black pixels, varying alpha)
to darken the image without color washing.

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

# Lines: horizontal scanline shadow
# Use opaque black (255) in pattern, control visibility via global opacity (like GRID)
# This allows per-scale opacity tuning to keep fine patterns visible
LINES_SHADOW = 255  # α for scanline shadow (255=opaque black, controlled via opacity)

# Grid: 2×2 pixel grid (unchanged from original)
GRID_ALPHA = 255    # α for black pixels

# CRT: 6-column vertical stripe pattern (aperture-style)
# Use opaque black for all shadow pixels, control via global opacity
CRT_SHADOW = 255    # α for shadow columns (opaque black, controlled via opacity)

# =============================================================================
# Pattern Generation
# =============================================================================

def generate_lines_scale(scale):
    """
    Generate 1×N scanline pattern for specific pixel scale.

    For scale N, creates N-pixel tall pattern with:
    - Row 0: Shadow (LINES_DARK alpha)
    - Rows 1 to N-1: Transparent (no darkening)

    This ensures one scanline shadow per scaled game pixel.
    """
    img = Image.new('RGBA', (1, scale))
    img.putpixel((0, 0), (0, 0, 0, LINES_SHADOW))   # shadow on first row
    for y in range(1, scale):
        img.putpixel((0, y), (0, 0, 0, 0))          # transparent for remaining rows
    return img

def generate_grid_scale(scale):
    """
    Generate 2×N grid pattern for specific pixel scale.

    Base 2×2 pattern (3 opaque corners, 1 transparent) is scaled
    up to 2×(2*scale) to match pixel scaling.
    """
    size = 2 * scale
    img = Image.new('RGBA', (size, size))

    # Scale up the base 2×2 pattern using nearest-neighbor
    for y in range(size):
        for x in range(size):
            # Map to base 2×2 pattern coordinates
            base_x = x // scale
            base_y = y // scale
            # Bottom-right corner is transparent, rest opaque
            if base_x == 1 and base_y == 1:
                img.putpixel((x, y), (0, 0, 0, 0))
            else:
                img.putpixel((x, y), (0, 0, 0, GRID_ALPHA))
    return img

def generate_crt_scale(scale):
    """
    Generate 6×N CRT pattern for specific pixel scale.

    Creates vertical shadow columns (opaque black), controlled via global opacity.
    Alternating columns for phosphor grid appearance.
    """
    img = Image.new('RGBA', (6, scale))
    # Alternating shadow/transparent columns (matches phosphor grid)
    # Columns 0,2,4: shadow, Columns 1,3,5: transparent
    for y in range(scale):
        for x in range(6):
            if x % 2 == 0:
                img.putpixel((x, y), (0, 0, 0, CRT_SHADOW))  # Shadow column
            else:
                img.putpixel((x, y), (0, 0, 0, 0))  # Transparent column
    return img

def show_pattern(name, img):
    """Display pattern info."""
    print(f"\n{name}.png ({img.width}×{img.height}):")
    for y in range(img.height):
        row = []
        for x in range(img.width):
            a = img.getpixel((x, y))[3]
            if a == 0:
                row.append("  · ")
            elif a < 50:
                row.append(f"░{a:3d}")
            elif a < 150:
                row.append(f"▒{a:3d}")
            else:
                row.append(f"▓{a:3d}")
        print(f"  {''.join(row)}")

def main():
    print("Generating effect patterns...")
    print(f"Output: {OUT_DIR}/")

    # Generate patterns for common pixel scales (2-8)
    # Scale 1 is never used (1:1 scaling has no interpolation artifacts needing effects)
    scales = [2, 3, 4, 5, 6, 7, 8]

    patterns = []
    for scale in scales:
        patterns.append((f"line-{scale}", generate_lines_scale(scale)))
        patterns.append((f"grid-{scale}", generate_grid_scale(scale)))
        patterns.append((f"crt-{scale}", generate_crt_scale(scale)))

    for name, img in patterns:
        path = os.path.join(OUT_DIR, f"{name}.png")
        img.save(path, optimize=True)
        show_pattern(name, img)
        print(f"  Saved: {path} ({os.path.getsize(path)} bytes)")

    print("\n" + "=" * 50)
    print(f"Generated {len(patterns)} pattern files for scales: {scales}")
    print("\nPattern structure:")
    print(f"  LINE-N: 1×N pixels (shadow on row 0, transparent on rows 1 to N-1)")
    print(f"  GRID-N: {2*scale}×{2*scale} pixels (base 2×2 pattern scaled up)")
    print(f"  CRT-N:  6×N pixels (6-column phosphor pattern)")
    print("\nTuning parameters (edit script to adjust):")
    print(f"  LINES: shadow={LINES_SHADOW} (opaque black, control via opacity)")
    print(f"  GRID:  alpha={GRID_ALPHA} (opaque black, control via opacity)")
    print(f"  CRT:   shadow={CRT_SHADOW} (opaque black, control via opacity)")
    print("\nUsage: Load pattern-N.png and tile at 1:1 (no scaling)")
    print("  All patterns use opaque shadows - adjust global opacity per scale")
    print("=" * 50)

if __name__ == "__main__":
    main()
