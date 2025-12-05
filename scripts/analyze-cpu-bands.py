#!/usr/bin/env python3
"""Analyze CPU benchmark data to find optimal frequency bands.

This script analyzes actual benchmark data from system reports to determine
optimal CPU frequency configurations for each platform.

Data sources:
- Benchmark data extracted from system reports on SD card
- Current configurations from workspace/*/platform/platform.c

The recommended 4-level system uses percentage of max frequency:
- IDLE: 20% (launcher, tools, settings)
- POWERSAVE: 55% (light gaming - GB, NES)
- NORMAL: 80% (most gaming - SNES, GBA)
- PERFORMANCE: 100% (demanding - PS1, N64)

Note: Safety ratios between levels are NOT required because:
- Auto CPU scaling uses granular frequencies with conservative step limits
- Manual presets are fixed user choices with no automatic transitions
"""

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
    "my355": {
        408: 67400,
        600: 99300,
        816: 134800,
        1104: 194000,
        1416: 238400,
        1608: 277000,
        1800: 301100,
        1992: 315700,
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
        f"\n{'Strategy':<30} {'IDLE':<10} {'POWERSAVE':<12} {'NORMAL':<12} {'PERFORMANCE':<12}"
    )
    print("-" * 80)

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

    # Strategy 5: 4-level system (IDLE/POWERSAVE/NORMAL/PERFORMANCE)
    # Uses percentage of MAX frequency (not range)
    # IDLE: 20% of max (for launcher/tools, no audio concerns)
    # POWERSAVE: 55% of max (light gaming)
    # NORMAL: 80% of max (most gaming)
    # PERFORMANCE: 100% (max)
    if len(freq_list) >= 3:
        idle_target = max_freq * 0.20
        ps_target = max_freq * 0.55
        n_target = max_freq * 0.80
        p_freq = max_freq

        idle_freq = min(freq_list, key=lambda f: abs(f - idle_target))
        ps_freq = min(freq_list, key=lambda f: abs(f - ps_target))
        n_freq = min(freq_list, key=lambda f: abs(f - n_target))

        ratio = ps_freq / n_freq
        safe = "✓" if 55 / ratio <= 85 else "✗"
        strategies.append(
            ("4-level (20/55/80/100)", ps_freq, n_freq, p_freq, ratio, safe, idle_freq)
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

    for strat in strategies:
        if len(strat) >= 7:  # 4-level strategy with IDLE
            strategy, ps, n, p, idle = strat[0], strat[1], strat[2], strat[3], strat[6]
            print(f"{strategy:<30} {idle:<10} {ps:<12} {n:<12} {p:<12}")
        else:  # 3-level strategy without IDLE
            strategy, ps, n, p = strat[0], strat[1], strat[2], strat[3]
            print(f"{strategy:<30} {'-':<10} {ps:<12} {n:<12} {p:<12}")

    return strategies


# Main analysis
def main():
    print("CPU FREQUENCY BAND ANALYSIS")
    print("=" * 80)
    print("\nBased on actual benchmark data from system reports")
    print("\nRecommended 4-level system (% of max frequency):")
    print("- IDLE: 20% (launcher, tools)")
    print("- POWERSAVE: 55% (light gaming)")
    print("- NORMAL: 80% (most gaming)")
    print("- PERFORMANCE: 100% (demanding games)")

    all_results = {}
    for platform, freqs in sorted(BENCHMARKS.items()):
        result = analyze_platform(platform, freqs)
        if result:
            all_results[platform] = result

    # Summary: Show recommended 4-level configs for all platforms
    print(f"\n{'='*80}")
    print("RECOMMENDED 4-LEVEL CONFIGURATIONS (20/55/80/100% of max)")
    print(f"{'='*80}")
    print(
        f"\n{'Platform':<15} {'IDLE':<10} {'POWERSAVE':<12} {'NORMAL':<12} {'PERFORMANCE':<12}"
    )
    print("-" * 65)

    for platform, strategies in sorted(all_results.items()):
        # Find the 4-level strategy
        four_level = next(
            (s for s in strategies if s[0] == "4-level (20/55/80/100)"), None
        )

        if four_level:
            idle = four_level[6]
            ps, n, p = four_level[1], four_level[2], four_level[3]
            print(f"{platform:<15} {idle:<10} {ps:<12} {n:<12} {p:<12}")


if __name__ == "__main__":
    main()
