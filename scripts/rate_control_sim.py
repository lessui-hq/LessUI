#!/usr/bin/env python3
"""
Rate Control Simulator - Matches C implementation exactly
"""

import argparse

def simulate(
    display_fps,
    core_fps,
    core_audio_rate=32040,
    host_audio_rate=48000,
    buffer_size=4096,
    d_param=0.005,
    clamp=0.05,
    frames=3000,
    initial_fill=0.5,
):
    """
    Simulate exactly what our C code does:

    1. r = core_audio / core_fps (input samples per frame)
    2. base_ratio = host_audio / core_audio (resampling ratio)
    3. rate_adjust = 1 + (1 - 2*fill) * d (Arntzen formula)
    4. display_correction = display_fps / core_fps
    5. corrected_adjust = rate_adjust * display_correction
    6. samples_produced = r * base_ratio / corrected_adjust (inverted)
    7. samples_consumed = host_audio / display_fps
    """

    # Input samples per frame
    r = core_audio_rate / core_fps

    # Base resampling ratio
    base_ratio = host_audio_rate / core_audio_rate

    # Display correction (display / core)
    display_correction = display_fps / core_fps

    # Consumption per frame (soundcard at display rate)
    consumed_per_frame = host_audio_rate / display_fps

    buffer_level = buffer_size * initial_fill
    history = []

    for frame in range(frames):
        fill = buffer_level / buffer_size
        fill = max(0.0, min(1.0, fill))

        # Rate control (Arntzen formula)
        rate_adjust = 1.0 - (1.0 - 2.0 * fill) * d_param

        # Apply display correction
        corrected_adjust = rate_adjust * display_correction

        # Clamp (safety limit)
        corrected_adjust = max(1.0 - clamp, min(1.0 + clamp, corrected_adjust))

        # Production per frame (resampler inverts ratio_adjust)
        # output = input * base_ratio / ratio_adjust
        produced_per_frame = r * base_ratio / corrected_adjust

        # Update buffer
        buffer_level += produced_per_frame - consumed_per_frame
        buffer_level = max(0.0, min(buffer_size, buffer_level))

        history.append(fill * 100)

    return {
        'history': history,
        'final_fill': history[-1],
        'min_fill': min(history),
        'max_fill': max(history),
        'r': r,
        'base_ratio': base_ratio,
        'display_correction': display_correction,
        'base_output': r * base_ratio,
        'consumed': consumed_per_frame,
    }


def draw_graph(history, width=60, height=12):
    """Draw ASCII graph of buffer level over time."""
    step = max(1, len(history) // width)
    sampled = [history[i] for i in range(0, len(history), step)][:width]

    for row in range(height, -1, -1):
        threshold = row * 100 / height
        line = ""
        for val in sampled:
            if abs(row - height // 2) == 0:
                line += "-" if val < threshold else "#"
            elif val >= threshold:
                line += "#"
            else:
                line += " "

        if row == height:
            print(f"100% |{line}|")
        elif row == height // 2:
            print(f" 50% |{line}| <- target")
        elif row == 0:
            print(f"  0% |{line}|")
        else:
            print(f"     |{line}|")


def main():
    parser = argparse.ArgumentParser(description='Rate Control Simulator (C implementation)')
    parser.add_argument('--display', type=float, default=59.71,
                        help='Display refresh rate in Hz (default: 59.71)')
    parser.add_argument('--core', type=float, default=60.10,
                        help='Core frame rate in Hz (default: 60.10)')
    parser.add_argument('--d', type=float, default=0.005,
                        help='Rate control d parameter (default: 0.005)')
    parser.add_argument('--clamp', type=float, default=0.05,
                        help='Safety clamp (default: 0.05)')
    parser.add_argument('--buffer', type=int, default=4096,
                        help='Buffer size (default: 4096)')
    parser.add_argument('--frames', type=int, default=3000,
                        help='Frames to simulate (default: 3000)')

    args = parser.parse_args()

    mismatch = (args.display - args.core) / args.core * 100

    print("=" * 70)
    print("Rate Control Simulator (C Implementation)")
    print("=" * 70)
    print(f"Display:     {args.display:.2f} Hz")
    print(f"Core:        {args.core:.2f} Hz")
    print(f"Mismatch:    {mismatch:+.2f}%")
    print(f"d parameter: {args.d} ({args.d * 100:.1f}%)")
    print(f"Clamp:       {args.clamp} ({args.clamp * 100:.1f}%)")
    print()

    result = simulate(
        display_fps=args.display,
        core_fps=args.core,
        d_param=args.d,
        clamp=args.clamp,
        buffer_size=args.buffer,
        frames=args.frames,
    )

    print(f"r (input samples/frame):      {result['r']:.2f}")
    print(f"base_ratio (host/core audio): {result['base_ratio']:.4f}")
    print(f"display_correction (display/core): {result['display_correction']:.4f}")
    print(f"base output (r * base_ratio): {result['base_output']:.2f} samples/frame")
    print(f"consumption (host/display):   {result['consumed']:.2f} samples/frame")
    print()

    # Check equilibrium
    needed_adjust = result['base_output'] / result['consumed']
    print(f"For equilibrium, need corrected_adjust = {needed_adjust:.4f}")
    print(f"  (base_output / consumption = {result['base_output']:.2f} / {result['consumed']:.2f})")
    print()
    print(f"Results after {args.frames} frames:")
    print(f"  Final buffer: {result['final_fill']:.1f}%")
    print(f"  Min buffer:   {result['min_fill']:.1f}%")
    print(f"  Max buffer:   {result['max_fill']:.1f}%")
    print()

    draw_graph(result['history'])


if __name__ == "__main__":
    main()
