# Audio Rate Control

LessUI implements dynamic rate control for audio/video synchronization based on Hans-Kristian Arntzen's paper "Dynamic Rate Control for Retro Game Emulators" (2012).

## The Problem

Retro game consoles have audio and video rates that are tightly coupled - every video frame produces a fixed number of audio samples. When emulating on modern hardware:

- The emulated system's refresh rate (e.g., SNES at 60.0988 Hz) differs from the host display
- The emulated audio rate (e.g., 32040.5 Hz) differs from the host audio rate (e.g., 48000 Hz)
- If you sync to video (VSync), the audio buffer will eventually underrun or overrun
- If you sync to audio (blocking writes), you'll miss VBlanks causing video stuttering

## The Solution

Dynamically adjust the audio resampling ratio based on buffer fill level:

```
adjustment = 1 - (1 - 2 * fill) * d
```

Where:
- `fill` = current buffer level (0.0 to 1.0)
- `d` = maximum allowed pitch deviation (0.005 = 0.5%)

This creates a self-correcting feedback loop:
- Buffer < 50% full: produce more output samples (fills buffer)
- Buffer = 50% full: no adjustment (equilibrium)
- Buffer > 50% full: produce fewer output samples (drains buffer)

The system naturally converges to 50% buffer fill, providing maximum headroom for timing jitter in both directions.

## Implementation

### Components

1. **Linear Interpolation Resampler** (`audio_resampler.c`)
   - Fixed-point 16.16 math for efficiency on ARM
   - Smoothly blends between adjacent samples
   - Zero memory allocation (uses ring buffer directly)

2. **Dynamic Rate Control** (`api.c`)
   - Calculates adjustment factor each audio batch
   - Clamps to ±1% to keep pitch shift inaudible

### Key Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Max deviation (d) | 0.5% | Below human perception (~1%) |
| Target fill | 50% | Maximum jitter headroom |
| Clamp range | ±1% | Safety margin for edge cases |

## Comparison with Other Implementations

| Aspect | MinUI | NextUI | LessUI |
|--------|-------|--------|--------|
| Resampling | Nearest-neighbor | libsamplerate | Linear interpolation |
| Rate control | None (blocking) | Cubic + averaging | Linear (paper formula) |
| Memory | Zero | malloc/free per batch | Zero |
| Dependencies | None | libsamplerate | None |
| Max pitch shift | 0% | ±1% | ±0.5% |

## Why This Approach

1. **Zero allocations** - Critical for memory-constrained handhelds
2. **No dependencies** - Easy cross-compilation for 15+ platforms
3. **Mathematically proven** - Paper proves exponential convergence
4. **Immediate response** - No averaging delay
5. **Inaudible** - 0.5% pitch shift is imperceptible
6. **CPU efficient** - Fixed-point math, no sinc convolutions

## References

- Arntzen, H.K. (2012). ["Dynamic Rate Control for Retro Game Emulators"](https://docs.libretro.com/guides/ratecontrol.pdf)
- Implementation: `workspace/all/common/audio_resampler.c`
- Integration: `workspace/all/common/api.c` (`SND_calculateRateAdjust`)
