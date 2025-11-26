#!/usr/bin/env python3
"""
Analyze row fitting across all retro handheld devices.

Simulates the UI_initLayout algorithm to see how well different MIN/MAX_PILL
ranges work across all supported devices.
"""

import math
import re

# Device configurations: (name, width, height, diagonal_inches)
DEVICES = [
    # Cortex-A7 (ARMv7)
    ("Miyoo Mini", 640, 480, 2.8),
    ("Miyoo Mini Plus", 640, 480, 2.8),
    ("Miyoo A30", 640, 480, 2.8),

    # Cortex-A35 (RK3326)
    ("RG-351P/M", 480, 320, 3.5),
    ("RG-351V/MP", 640, 480, 3.5),
    ("RGB10/10S/20", 480, 320, 3.5),
    ("RGB20S", 640, 480, 3.5),
    ("RGB10 Max/Max2", 854, 480, 5.0),
    ("GameForce Chi", 640, 480, 3.5),
    ("Odroid Go Advance", 320, 480, 3.5),
    ("Odroid Go Super", 854, 480, 5.0),
    ("GKD Pixel 2", 640, 480, 2.4),

    # Cortex-A53 (H700)
    ("RG-28XX", 640, 480, 2.8),
    ("RG-34XX/SP", 720, 480, 3.4),
    ("RG-35XX H/Plus/SP", 640, 480, 3.5),
    ("RG-40XX H/V", 640, 480, 4.0),
    ("RG Cube XX", 720, 720, 3.95),
    ("GKD Bubble", 640, 480, 3.5),

    # Cortex-A53 (A133 Plus)
    ("Trimui Brick", 1024, 768, 3.2),
    ("Trimui Smart Pro", 1280, 720, 4.95),
    ("MagicX Mini Zero 28", 640, 480, 2.8),

    # Cortex-A55 (RK3566)
    ("Miyoo Flip", 640, 480, 3.5),
    ("RG353M/V/P", 640, 480, 3.5),
    ("RG-503", 960, 544, 4.95),
    ("RG ARC-D/S", 640, 480, 4.0),
    ("RGB30", 720, 720, 4.0),
    ("PowKiddy X55", 1280, 720, 5.5),
    ("PowKiddy RK2023", 640, 480, 3.5),
    ("GKD Mini Plus", 640, 480, 3.5),

    # Cortex-A76 (RK3588)
    ("RG406H/V", 960, 540, 4.0),
    ("RG556", 1920, 1152, 5.48),

    # Snapdragon
    ("Retroid Pocket 5", 1920, 1080, 5.5),
    ("Retroid Pocket Mini", 960, 640, 3.7),
]

def simulate_layout(width, height, diagonal, min_pill, max_pill, baseline_ppi=144):
    """Simulate the UI_initLayout algorithm."""
    # Calculate PPI and dp_scale
    diagonal_px = math.sqrt(width**2 + height**2)
    ppi = diagonal_px / diagonal
    dp_scale = ppi / baseline_ppi

    # Available space (screen - top/bottom padding)
    screen_height_dp = int(height / dp_scale + 0.5)
    padding = 10
    available_dp = screen_height_dp - (padding * 2)

    # Try different row counts (content + 1 footer)
    # Start from maximum possible rows (when pill = MIN_PILL) and work down
    # Prefer even pixels but accept odd as fallback
    max_possible_rows = (available_dp // min_pill) - 1  # -1 for footer
    if max_possible_rows < 1:
        max_possible_rows = 1

    best_pill = 0
    best_rows = 0
    best_is_even = False

    for content_rows in range(max_possible_rows, 0, -1):
        total_rows = content_rows + 1  # +1 for footer
        pill = available_dp // total_rows

        # Early exit: pills only get larger as rows decrease
        if pill > max_pill:
            break

        pill_px = int(pill * dp_scale + 0.5)
        is_even = (pill_px % 2 == 0)

        if pill >= min_pill:
            if is_even:
                # Perfect: in range AND even
                best_pill = pill
                best_rows = content_rows
                best_is_even = True
                break
            elif best_rows == 0:
                # Acceptable but odd - keep as backup
                best_pill = pill
                best_rows = content_rows

    # Emergency fallback
    if best_rows == 0:
        best_pill = min_pill
        best_rows = 1

    # Calculate actual usage
    used_dp = (best_rows + 1) * best_pill
    wasted_dp = available_dp - used_dp
    wasted_px = int(wasted_dp * dp_scale + 0.5)
    fill_pct = (used_dp / available_dp) * 100 if available_dp > 0 else 0
    pill_px = int(best_pill * dp_scale + 0.5)
    font_px = int(16 * dp_scale + 0.5)  # FONT_LARGE = 16dp

    return {
        'ppi': ppi,
        'dp_scale': dp_scale,
        'screen_dp': screen_height_dp,
        'available_dp': available_dp,
        'rows': best_rows,
        'pill_dp': best_pill,
        'pill_px': pill_px,
        'font_px': font_px,
        'font_ratio': (font_px / pill_px) if pill_px > 0 else 0,
        'used_dp': used_dp,
        'wasted_dp': wasted_dp,
        'wasted_px': wasted_px,
        'fill_pct': fill_pct,
    }

def analyze_range(min_pill, max_pill):
    """Analyze how well a pill range works across all devices."""
    print(f"\n{'='*100}")
    print(f"ANALYZING RANGE: {min_pill}-{max_pill}dp")
    print(f"{'='*100}\n")

    results = []
    for name, width, height, diagonal in DEVICES:
        result = simulate_layout(width, height, diagonal, min_pill, max_pill)
        results.append((name, width, height, result))

    # Print detailed results
    print(f"{'Device':<25} {'Screen':<12} {'PPI':>5} {'Scale':>5} {'Rows':>4} {'Pill':>8} {'Font':>5} {'F/P':>5} {'Waste':>6} {'Fill':>5}")
    print(f"{'-'*25} {'-'*12} {'-'*5} {'-'*5} {'-'*4} {'-'*8} {'-'*5} {'-'*5} {'-'*6} {'-'*5}")

    for name, width, height, r in results:
        print(f"{name:<25} {width:>4}x{height:<4} {r['ppi']:>5.0f} {r['dp_scale']:>5.2f} "
              f"{r['rows']:>4} {r['pill_dp']:>3}dp/{r['pill_px']:>2}px "
              f"{r['font_px']:>3}px {r['font_ratio']*100:>4.0f}% "
              f"{r['wasted_px']:>4}px {r['fill_pct']:>4.1f}%")

    # Summary statistics
    print(f"\n{'Summary Statistics:':<25}")
    print(f"{'-'*50}")

    row_counts = [r['rows'] for _, _, _, r in results]
    fill_pcts = [r['fill_pct'] for _, _, _, r in results]
    wasted_pxs = [r['wasted_px'] for _, _, _, r in results]
    font_ratios = [r['font_ratio'] for _, _, _, r in results]

    print(f"{'Row counts:':<25} {min(row_counts)}-{max(row_counts)} (avg {sum(row_counts)/len(row_counts):.1f})")
    print(f"{'Fill percentage:':<25} {min(fill_pcts):.1f}%-{max(fill_pcts):.1f}% (avg {sum(fill_pcts)/len(fill_pcts):.1f}%)")
    print(f"{'Wasted pixels:':<25} {min(wasted_pxs)}-{max(wasted_pxs)}px (avg {sum(wasted_pxs)/len(wasted_pxs):.1f}px)")
    print(f"{'Font/Pill ratio:':<25} {min(font_ratios)*100:.0f}%-{max(font_ratios)*100:.0f}% (avg {sum(font_ratios)/len(font_ratios)*100:.0f}%)")

    # Identify problematic devices
    poor_fill = [(name, r['fill_pct']) for name, _, _, r in results if r['fill_pct'] < 95]
    if poor_fill:
        print(f"\n{'Devices with <95% fill:':<25}")
        for name, pct in poor_fill:
            print(f"  {name:<25} {pct:.1f}%")

    high_waste = [(name, r['wasted_px']) for name, _, _, r in results if r['wasted_px'] > 20]
    if high_waste:
        print(f"\n{'Devices wasting >20px:':<25}")
        for name, px in high_waste:
            print(f"  {name:<25} {px}px")

if __name__ == '__main__':
    # Current range
    analyze_range(28, 38)

    # Alternatives for comparison
    analyze_range(28, 32)  # Original narrow range
    analyze_range(30, 40)  # Shifted up
