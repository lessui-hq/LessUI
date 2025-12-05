#!/usr/bin/env python3
"""Compare different CPU frequency band selection strategies.

This script analyzes various approaches to selecting POWERSAVE/NORMAL/PERFORMANCE
frequencies and shows the tradeoffs for each strategy.
"""

import sys

# Benchmark data from system reports (freq in kHz -> iterations)
BENCHMARKS = {
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

# Current configurations
CURRENT = {
    "zero28": {"PS": 1200, "N": 1608, "P": 1800},
    "rg35xxplus": {"PS": 1008, "N": 1320, "P": 1512},
    "tg5040": {"PS": 1200, "N": 1608, "P": 2000},
    "miyoomini": {"PS": 1100, "N": 1600, "P": 1600},
}


def find_closest(freq_list, target):
    """Find closest available frequency to target."""
    return min(freq_list, key=lambda f: abs(f - target))


def calculate_strategy(name, freqs, strategy_name, selector_fn):
    """Calculate a configuration strategy and return stats."""
    freq_list = sorted(freqs.keys())
    min_freq = min(freq_list)
    max_freq = max(freq_list)

    # Get frequencies from strategy
    ps_freq, n_freq, p_freq = selector_fn(freq_list, freqs)

    # Calculate performance indices
    baseline = freqs[min_freq]
    ps_perf = freqs[ps_freq] / baseline
    n_perf = freqs[n_freq] / baseline
    p_perf = freqs[p_freq] / baseline

    # Calculate ratios for auto-scaling safety
    ps_n_ratio = ps_freq / n_freq
    n_p_ratio = n_freq / p_freq

    # Check if safe for 55% threshold
    safe = "✓" if 55 / ps_n_ratio <= 85 else "✗"

    # Calculate utilization after reduction
    util_after_reduce = 55 / ps_n_ratio if ps_n_ratio > 0 else 999
    margin = 85 - util_after_reduce

    return {
        "name": strategy_name,
        "ps": ps_freq,
        "n": n_freq,
        "p": p_freq,
        "ps_perf": ps_perf,
        "n_perf": n_perf,
        "p_perf": p_perf,
        "ps_n_ratio": ps_n_ratio,
        "n_p_ratio": n_p_ratio,
        "safe": safe,
        "util_after": util_after_reduce,
        "margin": margin,
    }


def strategy_current(freq_list, freqs):
    """Current configuration."""
    # This will be filled in from CURRENT dict
    return None, None, None


def strategy_percentage(freq_list, freqs):
    """75% / 85% / 100% of max frequency."""
    max_freq = max(freq_list)
    ps = find_closest(freq_list, max_freq * 0.75)
    n = find_closest(freq_list, max_freq * 0.85)
    p = max_freq
    return ps, n, p


def strategy_perf_target(freq_list, freqs):
    """Target consistent performance levels (2.5x / 3.5x / 4.5x)."""
    baseline = freqs[min(freq_list)]
    perf = {f: freqs[f] / baseline for f in freq_list}

    # Find closest to target performance
    ps = min(freq_list, key=lambda f: abs(perf[f] - 2.5))
    n = min(freq_list, key=lambda f: abs(perf[f] - 3.5))
    p = max(freq_list)  # Always max for performance
    return ps, n, p


def strategy_equal_gaps(freq_list, freqs):
    """Equal performance gaps between bands."""
    baseline = freqs[min(freq_list)]
    max_freq = max(freq_list)
    perf = {f: freqs[f] / baseline for f in freq_list}

    max_perf = perf[max_freq]
    min_perf = perf[min(freq_list)]
    gap = (max_perf - min_perf) / 3

    ps = min(freq_list, key=lambda f: abs(perf[f] - (min_perf + gap)))
    n = min(freq_list, key=lambda f: abs(perf[f] - (min_perf + 2 * gap)))
    p = max_freq
    return ps, n, p


def strategy_efficiency(freq_list, freqs):
    """Pick most efficient frequencies for each tier."""
    baseline = freqs[min(freq_list)]
    max_freq = max(freq_list)

    # Calculate efficiency (perf per watt estimate)
    efficiency = {}
    for f in freq_list:
        perf = freqs[f] / baseline
        # Rough power estimate: P ≈ f^1.3
        power = (f ** 1.3) / (min(freq_list) ** 1.3)
        efficiency[f] = perf / power

    # Sort by efficiency
    by_eff = sorted(efficiency.items(), key=lambda x: x[1], reverse=True)
    candidates = [f for f, _ in by_eff]

    # Pick most efficient from each range
    ps_candidates = [f for f in candidates if f <= max_freq * 0.6]
    n_candidates = [f for f in candidates if max_freq * 0.5 < f < max_freq * 0.9]
    p_candidates = [f for f in candidates if f >= max_freq * 0.8]

    ps = ps_candidates[0] if ps_candidates else freq_list[len(freq_list)//3]
    n = n_candidates[0] if n_candidates else freq_list[2*len(freq_list)//3]
    p = p_candidates[0] if p_candidates else max_freq

    return ps, n, p


def strategy_lower_powersave(freq_list, freqs):
    """Try to get POWERSAVE as low as possible while staying safe."""
    max_freq = max(freq_list)
    p = max_freq

    # NORMAL at 85% of max
    n = find_closest(freq_list, max_freq * 0.85)

    # POWERSAVE: go as low as possible while keeping 65%+ ratio
    target_ratio = 0.65  # minimum safe ratio
    min_ps = int(n * target_ratio)

    # Find lowest freq that meets minimum
    ps_candidates = [f for f in freq_list if f >= min_ps]
    ps = min(ps_candidates) if ps_candidates else freq_list[0]

    return ps, n, p


def analyze_all_strategies(platform, freqs):
    """Run all strategies and return results."""
    strategies = [
        ("Current Config", strategy_current),
        ("Percentage (75/85/100%)", strategy_percentage),
        ("Performance Target", strategy_perf_target),
        ("Equal Perf Gaps", strategy_equal_gaps),
        ("Max Efficiency", strategy_efficiency),
        ("Lower POWERSAVE", strategy_lower_powersave),
    ]

    results = []
    for name, fn in strategies:
        if name == "Current Config":
            if platform in CURRENT:
                cfg = CURRENT[platform]
                result = calculate_strategy(
                    platform, freqs, name,
                    lambda fl, fr: (cfg["PS"], cfg["N"], cfg["P"])
                )
                results.append(result)
        else:
            result = calculate_strategy(platform, freqs, name, fn)
            results.append(result)

    return results


def print_platform_analysis(platform, freqs):
    """Print detailed analysis for one platform."""
    freq_list = sorted(freqs.keys())
    baseline = freqs[min(freq_list)]

    print(f"\n{'='*100}")
    print(f"PLATFORM: {platform}")
    print(f"{'='*100}")
    print(f"Available: {len(freq_list)} frequencies from {min(freq_list)} to {max(freq_list)} kHz")

    results = analyze_all_strategies(platform, freqs)

    # Print comparison table
    print(f"\n{'Strategy':<25} {'PS':<8} {'N':<8} {'P':<8} {'PS:N':<8} {'Safe?':<7} {'Util@55%':<10} {'Margin':<8}")
    print("-" * 100)

    for r in results:
        print(f"{r['name']:<25} {r['ps']:<8} {r['n']:<8} {r['p']:<8} "
              f"{r['ps_n_ratio']:>6.1%}  {r['safe']:<7} "
              f"{r['util_after']:>6.1f}%    {r['margin']:>6.1f}%")

    # Print performance indices
    print(f"\n{'Strategy':<25} {'PS Perf':<10} {'N Perf':<10} {'P Perf':<10} {'Gap PS→N':<12} {'Gap N→P':<12}")
    print("-" * 100)

    for r in results:
        gap_ps_n = r['n_perf'] - r['ps_perf']
        gap_n_p = r['p_perf'] - r['n_perf']
        print(f"{r['name']:<25} {r['ps_perf']:>6.2f}x    {r['n_perf']:>6.2f}x    "
              f"{r['p_perf']:>6.2f}x    {gap_ps_n:>8.2f}x    {gap_n_p:>8.2f}x")


def main():
    print("CPU FREQUENCY STRATEGY COMPARISON")
    print("=" * 100)
    print("\nComparing different approaches to selecting POWERSAVE / NORMAL / PERFORMANCE frequencies")
    print("\nKey criteria:")
    print("  - Safe: PS/N ratio ≥ 65% (ensures 55% util doesn't exceed 85% after reduction)")
    print("  - Efficient: Good performance per watt")
    print("  - Meaningful: Clear performance tiers for users")

    for platform in sorted(BENCHMARKS.keys()):
        print_platform_analysis(platform, BENCHMARKS[platform])

    # Summary recommendation
    print(f"\n{'='*100}")
    print("RECOMMENDATION SUMMARY")
    print(f"{'='*100}")

    print(f"\n{'Platform':<15} {'Recommended Strategy':<30} {'PS':<8} {'N':<8} {'P':<8} {'Why':<30}")
    print("-" * 100)

    for platform in sorted(BENCHMARKS.keys()):
        results = analyze_all_strategies(platform, BENCHMARKS[platform])

        # Pick best: prioritize safety, then balance
        safe_results = [r for r in results if r['safe'] == '✓']
        if safe_results:
            # Among safe options, pick one with good balance
            best = max(safe_results, key=lambda r: r['margin'])
            reason = f"Safe, {best['margin']:.0f}% margin"
        else:
            best = results[0]
            reason = "Current (unsafe!)"

        print(f"{platform:<15} {best['name']:<30} {best['ps']:<8} {best['n']:<8} "
              f"{best['p']:<8} {reason:<30}")


if __name__ == "__main__":
    main()
