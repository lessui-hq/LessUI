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

# Lines: 3-row horizontal scanline pattern
LINES_LIGHT = 20    # α for subtle shadow row
LINES_DARK = 130    # α for strong scanline row
LINES_GAP = 0       # α for gap row (0 = transparent)

# Grid: 2×2 pixel grid (unchanged from original)
GRID_ALPHA = 255    # α for black pixels

# CRT: 6-column vertical stripe pattern (aperture-style)
CRT_PHOSPHOR = 30   # α for phosphor columns (light shadow)
CRT_GAP = 100       # α for gap columns (darker shadow)

# =============================================================================
# Pattern Generation
# =============================================================================

def generate_lines():
    """1×3 horizontal scanline pattern with shadow gradient."""
    img = Image.new('RGBA', (1, 3))
    img.putpixel((0, 0), (0, 0, 0, LINES_LIGHT))  # light shadow
    img.putpixel((0, 1), (0, 0, 0, LINES_DARK))   # strong scanline
    img.putpixel((0, 2), (0, 0, 0, LINES_GAP))    # gap
    return img

def generate_grid():
    """2×2 pixel grid pattern."""
    img = Image.new('RGBA', (2, 2))
    img.putpixel((0, 0), (0, 0, 0, GRID_ALPHA))
    img.putpixel((1, 0), (0, 0, 0, GRID_ALPHA))
    img.putpixel((0, 1), (0, 0, 0, GRID_ALPHA))
    img.putpixel((1, 1), (0, 0, 0, 0))  # transparent corner
    return img

def generate_crt():
    """6×1 vertical stripe pattern (aperture-style)."""
    img = Image.new('RGBA', (6, 1))
    # Alternating phosphor/gap columns
    alphas = [CRT_PHOSPHOR, CRT_GAP, CRT_PHOSPHOR, CRT_GAP, CRT_PHOSPHOR, CRT_GAP]
    for x in range(6):
        img.putpixel((x, 0), (0, 0, 0, alphas[x]))
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

    patterns = [
        ("line", generate_lines()),
        ("grid", generate_grid()),
        ("crt", generate_crt()),
    ]

    for name, img in patterns:
        path = os.path.join(OUT_DIR, f"{name}.png")
        img.save(path)
        show_pattern(name, img)
        print(f"  Saved: {path} ({os.path.getsize(path)} bytes)")

    print("\n" + "=" * 50)
    print("Tuning parameters (edit script to adjust):")
    print(f"  LINES: light={LINES_LIGHT}, dark={LINES_DARK}, gap={LINES_GAP}")
    print(f"  GRID:  alpha={GRID_ALPHA}")
    print(f"  CRT:   phosphor={CRT_PHOSPHOR}, gap={CRT_GAP}")
    print("=" * 50)

if __name__ == "__main__":
    main()
