#!/usr/bin/env python3
"""Analyze CPU benchmark data to find optimal frequency bands for auto scaling.

This script analyzes actual benchmark data from system reports to determine
optimal CPU frequency configurations for each platform. The goal is to ensure
auto CPU scaling works reliably without oscillation.

Data sources:
- Benchmark data extracted from system reports on SD card
- Current configurations from workspace/*/platform/platform.c

Key constraints:
- POWERSAVE/NORMAL ratio must be >65% (safe for 55% util threshold)
- Should maximize efficiency (performance per watt)
- Should provide meaningful performance tiers
"""

import sys

# Benchmark data from system reports (freq in kHz -> iterations)
# Data extracted from /Volumes/LESSUI_DEV/system_report_*.md files
BENCHMARKS = {
    "rg35xxplus": {
        480: 79400,
        720: 119000,
        936: 154900,
        1008: 166900,
        1104: 182700,
        1200: 198600,
        1320: 218600,
        1416: 234500,
        1512: 250500,
    },
    "zero28": {
        408: 67100,
        600: 99300,
        816: 135100,
        1008: 167100,
        1200: 199000,
        1416: 234900,
        1608: 266600,
        1800: 298600,
    },
    "tg5040": {
        408: 67300,
        600: 99200,
        816: 134800,
        1008: 167000,
        1200: 199000,
        1416: 234800,
        1608: 266600,
        1800: 298300,
        2000: 330500,
    },
    "miyoomini": {
        400: 66400,
        600: 99700,
        800: 132900,
        1000: 166200,
        1100: 182700,
        1600: 232600,
    },
}

# Current configurations from workspace/*/platform/platform.c
CURRENT_CONFIGS = {
    "rg35xxplus": {
        "POWERSAVE": 1008,
        "NORMAL": 1320,
        "PERFORMANCE": 1512,
    },
    "zero28": {
        "POWERSAVE": 816,
        "NORMAL": 1416,
        "PERFORMANCE": 1800,
    },
    "tg5040": {
        "POWERSAVE": 1200,
        "NORMAL": 1608,
        "PERFORMANCE": 2000,
    },
    "miyoomini": {
        "POWERSAVE": 1104,
        "NORMAL": 1296,
        "PERFORMANCE": 1488,
    },
    "my355": {
        "POWERSAVE": 1104,
        "NORMAL": 1608,
        "PERFORMANCE": 1992,
    },
    "rgb30": {
        "POWERSAVE": 1104,
        "NORMAL": 1608,
        "PERFORMANCE": 1992,
    },
    "magicmini": {
        "POWERSAVE": 816,
        "NORMAL": 1416,
        "PERFORMANCE": 2016,
    },
    "trimuismart": {
        "POWERSAVE": 1104,
        "NORMAL": 1344,
        "PERFORMANCE": 1536,
    },
    "rg35xx": {
        "POWERSAVE": 1104,
        "NORMAL": 1296,
        "PERFORMANCE": 1488,
    },
    "my282": {
        "POWERSAVE": 1056,
        "NORMAL": 1344,
        "PERFORMANCE": 1512,
    },
}


def analyze_platform(name, freqs):
    """Analyze a single platform's benchmark data."""
    print(f"\n{'='*80}")
    print(f"PLATFORM: {name}")
    print(f"{'='*80}")

    # Filter out None values
    valid_freqs = {f: p for f, p in freqs.items() if p is not None}
    if not valid_freqs:
        print("No benchmark data available")
        return None

    freq_list = sorted(valid_freqs.keys())
    min_freq = freq_list[0]
    max_freq = freq_list[-1]

    # Normalize performance (relative to lowest freq)
    baseline_perf = valid_freqs[min_freq]
    normalized = {f: p / baseline_perf for f, p in valid_freqs.items()}

    print(f"\nAvailable frequencies: {freq_list}")
    print(f"Range: {min_freq} - {max_freq} kHz")

    # Calculate efficiency (performance per MHz)
    print(
        f"\n{'Freq (MHz)':<12} {'Perf Index':<12} {'Efficiency':<12} {'Perf/Watt*':<12}"
    )
    print("-" * 50)
    for freq in freq_list:
        perf = normalized[freq]
        efficiency = perf / (freq / 1000)  # perf per MHz
        # Very rough power estimate: P ≈ V² × f, assume V ≈ freq^0.3
        power_estimate = (freq**1.3) / (min_freq**1.3)
        perf_per_watt = perf / power_estimate
        print(
            f"{freq:<12} {perf:>11.2f}x {efficiency:>11.4f} {perf_per_watt:>11.3f}"
        )

    # Find optimal bands using several strategies
    print(
        f"\n{'Strategy':<30} {'POWERSAVE':<12} {'NORMAL':<12} {'PERFORMANCE':<12} {'PS/N Ratio':<12} {'Safe @55%?'}"
    )
    print("-" * 100)

    strategies = []

    # Strategy 1: Equal performance gaps
    if len(freq_list) >= 3:
        perf_gap = (normalized[max_freq] - normalized[min_freq]) / 3
        target_ps = normalized[min_freq] + perf_gap
        target_n = normalized[min_freq] + 2 * perf_gap

        ps_freq = min(freq_list, key=lambda f: abs(normalized[f] - target_ps))
        n_freq = min(freq_list, key=lambda f: abs(normalized[f] - target_n))
        p_freq = max_freq

        ratio = ps_freq / n_freq
        safe = "✓" if 55 / ratio <= 85 else "✗"
        strategies.append(("Equal perf gaps", ps_freq, n_freq, p_freq, ratio, safe))

    # Strategy 2: Equal frequency gaps
    if len(freq_list) >= 3:
        freq_gap = (max_freq - min_freq) / 3
        ps_freq = min(freq_list, key=lambda f: abs(f - (min_freq + freq_gap)))
        n_freq = min(freq_list, key=lambda f: abs(f - (min_freq + 2 * freq_gap)))
        p_freq = max_freq

        ratio = ps_freq / n_freq
        safe = "✓" if 55 / ratio <= 85 else "✗"
        strategies.append(("Equal freq gaps", ps_freq, n_freq, p_freq, ratio, safe))

    # Strategy 3: Maximize efficiency (best perf/watt for each band)
    if len(freq_list) >= 5:
        perf_per_watt = {}
        for freq in freq_list:
            perf = normalized[freq]
            power = (freq**1.3) / (min_freq**1.3)
            perf_per_watt[freq] = perf / power

        # Sort by efficiency
        by_efficiency = sorted(perf_per_watt.items(), key=lambda x: x[1], reverse=True)

        # Pick top from different ranges
        candidates = [f for f, _ in by_efficiency]
        ps_candidates = [f for f in candidates if f <= max_freq * 0.6]
        n_candidates = [f for f in candidates if max_freq * 0.5 < f < max_freq * 0.9]
        p_candidates = [f for f in candidates if f >= max_freq * 0.8]

        if ps_candidates and n_candidates and p_candidates:
            ps_freq = ps_candidates[0]
            n_freq = n_candidates[0]
            p_freq = p_candidates[0]

            ratio = ps_freq / n_freq
            safe = "✓" if 55 / ratio <= 85 else "✗"
            strategies.append(("Max efficiency", ps_freq, n_freq, p_freq, ratio, safe))

    # Strategy 4: Safe scaling (ensure 70%+ ratios)
    if len(freq_list) >= 3:
        p_freq = max_freq
        # Find NORMAL that gives 80-90% of PERF
        n_candidates = [f for f in freq_list if 0.8 <= f / p_freq <= 0.9]
        if n_candidates:
            n_freq = max(n_candidates)  # Pick highest in range
            # Find POWERSAVE that gives 70-80% of NORMAL
            ps_candidates = [f for f in freq_list if 0.70 <= f / n_freq <= 0.80]
            if ps_candidates:
                ps_freq = max(ps_candidates)

                ratio = ps_freq / n_freq
                safe = "✓" if 55 / ratio <= 85 else "✗"
                strategies.append(
                    ("Safe scaling (70-90%)", ps_freq, n_freq, p_freq, ratio, safe)
                )

    # Strategy 5: Current config
    if name in CURRENT_CONFIGS:
        cfg = CURRENT_CONFIGS[name]
        ratio = cfg["POWERSAVE"] / cfg["NORMAL"]
        safe = "✓" if 55 / ratio <= 85 else "✗"
        strategies.append(
            (
                "CURRENT CONFIG",
                cfg["POWERSAVE"],
                cfg["NORMAL"],
                cfg["PERFORMANCE"],
                ratio,
                safe,
            )
        )

    for strategy, ps, n, p, ratio, safe in strategies:
        print(
            f"{strategy:<30} {ps:<12} {n:<12} {p:<12} {ratio:>11.1%} {safe:>10}"
        )

    # Show what happens at 55% util for each strategy
    print(f"\nProjected utilization after NORMAL→POWERSAVE reduction at 55%:")
    print(
        f"{'Strategy':<30} {'POWERSAVE util':<20} {'Margin to 85%':<15} {'Status':<10}"
    )
    print("-" * 75)
    for strategy, ps, n, p, ratio, safe in strategies:
        if ratio > 0:
            new_util = 55 / ratio
            margin = 85 - new_util
            status = "SAFE" if margin > 0 else "DANGER"
            print(
                f"{strategy:<30} {new_util:>6.1f}%             {margin:>14.1f}%  {status:<10}"
            )

    return strategies


# Main analysis
def main():
    print("CPU FREQUENCY BAND OPTIMIZATION ANALYSIS")
    print("=" * 80)
    print("\nBased on actual benchmark data from system reports")
    print("Goal: Find optimal frequency bands for auto CPU scaling")
    print("\nConstraints:")
    print("- POWERSAVE/NORMAL ratio must be >65% (safe for 55% threshold)")
    print("- Should maximize efficiency (performance per watt)")
    print("- Should provide meaningful performance tiers")

    all_results = {}
    for platform, freqs in sorted(BENCHMARKS.items()):
        result = analyze_platform(platform, freqs)
        if result:
            all_results[platform] = result

    # Summary recommendations
    print(f"\n{'='*80}")
    print("SUMMARY: RECOMMENDED CONFIGURATIONS")
    print(f"{'='*80}")
    print(
        f"\n{'Platform':<15} {'Strategy':<25} {'POWERSAVE':<12} {'NORMAL':<12} {'PERFORMANCE':<12} {'Status':<10}"
    )
    print("-" * 95)

    for platform, strategies in sorted(all_results.items()):
        # Find best strategy: prefer "Safe scaling", fallback to current if safe
        safe_strat = next(
            (s for s in strategies if s[0] == "Safe scaling (70-90%)"), None
        )
        current_strat = next(
            (s for s in strategies if s[0] == "CURRENT CONFIG"), None
        )

        if safe_strat:
            best = safe_strat
            status = "✓ UPDATE" if best != current_strat else "✓ OK"
        elif current_strat and current_strat[5] == "✓":
            best = current_strat
            status = "✓ OK"
        else:
            best = strategies[0]
            status = "⚠ REVIEW"

        strategy, ps, n, p, ratio, safe = best
        print(
            f"{platform:<15} {strategy:<25} {ps:<12} {n:<12} {p:<12} {status:<10}"
        )

    print(f"\n{'='*80}")
    print("PLATFORMS NEEDING UPDATES")
    print(f"{'='*80}")

    needs_update = []
    for platform, strategies in sorted(all_results.items()):
        safe_strat = next(
            (s for s in strategies if s[0] == "Safe scaling (70-90%)"), None
        )
        current_strat = next(
            (s for s in strategies if s[0] == "CURRENT CONFIG"), None
        )

        if safe_strat and current_strat and safe_strat != current_strat:
            needs_update.append((platform, current_strat, safe_strat))

    if needs_update:
        for platform, current, recommended in needs_update:
            print(f"\n{platform}:")
            print(f"  Current:     PS={current[1]} N={current[2]} P={current[3]}")
            print(
                f"  Recommended: PS={recommended[1]} N={recommended[2]} P={recommended[3]}"
            )
            print(f"  Reason: Improves ratio from {current[4]:.1%} to {recommended[4]:.1%}")
    else:
        print("\nAll platforms are already optimally configured!")


if __name__ == "__main__":
    main()
