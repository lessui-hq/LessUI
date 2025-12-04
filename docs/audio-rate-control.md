# Audio Rate Control

LessUI implements dynamic rate control for audio/video synchronization based on Hans-Kristian Arntzen's paper "Dynamic Rate Control for Retro Game Emulators" (2012).

## The Problem

Retro game consoles are highly synchronous - audio generation is locked to video frame rate. Every video frame produces a fixed number of audio samples. When emulating on modern hardware:

- The emulated system's refresh rate (e.g., NES at 60.0988 Hz) differs from the host display
- The emulated audio rate (e.g., 32040.5 Hz) differs from the host audio rate (e.g., 48000 Hz)
- If you sync to video (VSync), the audio buffer will eventually underrun or overrun
- If you sync to audio (blocking writes), you'll miss VBlanks causing video stuttering

**The fundamental challenge**: Synchronize to vsync (smooth video) while never underrunning or blocking on audio.

## The Algorithm

### Definitions

| Symbol | Meaning | Example |
|--------|---------|---------|
| **fᵥ** | Emulated system frame rate | 60.0988 Hz (NES) |
| **fₐ** | Emulated system audio rate | 32040.5 Hz |
| **r** | Samples per frame (emulated) | fₐ/fᵥ = 533.5 samples |
| **mᵥ** | Host display refresh rate | 59.71 Hz (actual) |
| **mₐ** | Host audio sample rate | 48000 Hz |
| **R** | Samples per frame (host, actual) | mₐ/mᵥ = 804.1 samples |
| **R'** | Samples per frame (host, estimated) | m'ₐ/m'ᵥ ≈ 800 samples |
| **Aᵦ** | Current buffer level (samples) | |
| **Aᴮ** | Buffer capacity (samples) | |
| **d** | Maximum allowed pitch deviation | 0.005-0.015 |

### Core Formula

The resampler converts from the emulated rate to the host rate. The **base resampling ratio** is:

```
base_ratio = R' / r = (host_audio / host_display) / (core_audio / core_fps)
```

The **dynamic rate control formula** adjusts this ratio based on buffer fill:

```
ΔAᵦ = [1 + ((Aᴮ - 2Aᵦ) / Aᴮ) × d] × R' - R
```

Rewriting in terms of fill level (fill = Aᵦ/Aᴮ):

```
adjustment = 1 + (1 - 2×fill) × d
```

**Behavior:**
- Buffer empty (fill=0): adjustment = 1+d → produce MORE samples → fill buffer
- Buffer half (fill=0.5): adjustment = 1.0 → maintain equilibrium
- Buffer full (fill=1): adjustment = 1-d → produce FEWER samples → drain buffer

### Our Implementation (Inverted for Resampler Convention)

Our resampler uses ratio_adjust to control step size. Larger steps = fewer output samples. So we invert the formula:

```c
// When fill=0: adjustment = 1 - d  (smaller steps, more outputs, fills buffer)
// When fill=0.5: adjustment = 1.0  (no change)
// When fill=1: adjustment = 1 + d  (larger steps, fewer outputs, drains buffer)
return 1.0f - (1.0f - 2.0f * fill) * d;
```

This creates a self-correcting feedback loop that naturally converges to 50% buffer fill.

## Stability Analysis

### Convergence Proof

The paper proves the system is stable using continuous-time analysis. The differential equation:

```
δAᵦ/δf + (2dR'/Aᴮ) × Aᵦ = R'(1+d) - R
```

Has solution:

```
Aᵦ = Aᴮ × (R'(1+d) - R) / (2dR') + C₀ × exp(-2dR'/Aᴮ × f)
```

As f → ∞, the exponential term vanishes and the buffer converges to:

```
Aᵦ,ᵪ = Aᴮ × (R'(1+d) - R) / (2dR')
```

### Equilibrium Points

**If R' = R (perfect estimate):**
```
Aᵦ,ᵪ = Aᴮ/2
```
Buffer settles at 50% - maximum headroom for jitter in both directions.

**If R' ≠ R (estimation error):**
The buffer settles at a different level, but **stays stable** as long as R falls within the range [R'(1-d), R'(1+d)].

### The Critical Constraint

**d must be chosen so that:**
```
R'(1-d) ≤ R ≤ R'(1+d)
```

Rearranging:
```
|R - R'| / R' ≤ d
```

**In plain English: d must be at least as large as the relative mismatch between expected and actual samples-per-frame.**

If the display runs at 59.71 Hz but the core expects 60.10 Hz, that's a -0.65% mismatch. With d = 0.5%, the algorithm cannot fully compensate, and the buffer will drift toward empty.

## Handheld Device Considerations

### Why We Need Larger d

The paper recommends d = 0.2-0.5% for **desktop systems** because:
- Desktop monitors: 59.94-60.00 Hz (very stable, <0.5% variance)
- Good oscillator quality
- Only needs to handle timing jitter

**Handheld devices are different:**
- Cheap LCD panels: 57-62 Hz (measured variance up to 4%)
- Display/core mismatches of 0.65-2% are common
- Need larger d to handle structural mismatch, not just jitter

### Measured Display Timings

| Platform | Measured Hz | Core expects | Mismatch |
|----------|-------------|--------------|----------|
| rg35xxplus | 59.71 Hz | 60.10 Hz (NES) | -0.65% |
| tg5040 | ~60.5 Hz | 60.10 Hz (NES) | +0.67% |

### Solution: Adaptive d + Base Correction

We use a two-phase approach that separates static mismatch from dynamic jitter:

**Phase 1: Measurement (first ~10-50 frames)**
- Use large d=2% to handle unknown display/core mismatch
- Measure actual display rate via vsync timing
- Wait for stable readings (10 consecutive frames with spread < 0.5 Hz)

**Phase 2: Refined (after measurement)**
- Apply base_correction for static display/core offset
- Switch to small d=0.5% for jitter only

```c
// Adaptive d parameter
float d = (display_hz_measured) ? 0.005 : 0.02;  // 0.5% or 2%

// Base correction (static offset, measured once)
float base_correction = display_hz / core_hz;  // e.g., 59.71/60.10 = 0.9935

// Dynamic adjustment (jitter compensation)
float rate_adjust = 1 - (1 - 2*fill) * d;  // 1.0 ± d

// Combined
float total_adjust = base_correction * rate_adjust;
```

This keeps d optimal for each phase while handling real-world display variance.

## Pitch Audibility

With d=0.5% and base_correction handling display mismatch separately:

1. **Base correction**: Static offset (e.g., -0.65%), always applied, inaudible
2. **Rate control**: Dynamic ±0.5% for jitter, averages ~0.06% in practice
3. **Combined**: Total deviation well under 1% (human perception threshold)
4. **Stable at 50%**: Maximum headroom for jitter in both directions

## Implementation Details

### Components

1. **Linear Interpolation Resampler** (`audio_resampler.c`)
   - Fixed-point 16.16 math for efficiency on ARM
   - Smoothly blends between adjacent samples
   - Zero memory allocation (uses ring buffer directly)

2. **Dynamic Rate Control** (`api.c`)
   - Calculates adjustment factor each audio batch
   - Single formula, no averaging or filtering needed

3. **Safety Clamp** (`audio_resampler.h`)
   - Limits ratio_adjust to ±5% absolute maximum
   - Catches bugs/misconfiguration, separate from d parameter

### Key Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Rate control d (adaptive) | 2% → 0.5% | Large during measurement, small after |
| Base correction | Measured display_hz / core fps | Handles display/core mismatch statically |
| Measurement | Sliding window, 10 frames | Waits for spread < 0.5 Hz (stable) |
| Target fill | 50% | Maximum jitter headroom |
| Safety clamp | ±5% | Catches bugs without limiting normal operation |
| Buffer size | 4 frames | ~67ms latency, balanced headroom |

### Code Flow

```
SND_init()
  └─> display_hz = 0 (not measured yet)

Main loop (frames 1-N):
  ├─> core.run()
  │     └─> audio_callback(samples)
  │           └─> SND_batchSamples()
  │                 ├─> d = (display_hz > 0) ? 0.005 : 0.02  (adaptive!)
  │                 ├─> base_correction = display_hz > 0 ? display_hz/core : 1.0
  │                 ├─> rate_adjust = 1 - (1 - 2*fill) * d
  │                 ├─> total_adjust = base_correction * rate_adjust
  │                 └─> AudioResampler_resample(..., total_adjust)
  │
  └─> GFX_flip()
        └─> measureDisplayRate()
              └─> (after 10 stable frames) → SND_setDisplayRate(59.71)
                    └─> display_hz now set, d switches to 0.5%
```

## Design Evolution

| Iteration | Approach | Issue |
|-----------|----------|-------|
| 1 | Rate control d=0.5% only | Insufficient for display/core mismatch |
| 2 | Rate control d=1.5% only | Worked on rg35xxplus, oscillated on tg5040 |
| 3 | Rate control d=2% only | Worse oscillation, overcorrects jitter |
| 4 | SDL display query + d=0.5% | SDL rounds to integer (60 vs 59.71), inaccurate |
| 5 | Fixed 30-frame measurement | Captured startup noise, bad readings |
| 6 | **Adaptive d + stability detection** | Handles bootstrap, filters noise, converges to 50% |

**Final approach**:
- Start with d=2% to handle unknown mismatch during measurement
- Measure display rate via vsync with stability detection (10 frames, spread < 0.5 Hz)
- Once measured, apply base_correction and switch to d=0.5%
- This separates static offset from dynamic jitter as the paper intended

## Comparison with Other Implementations

| Aspect | MinUI | NextUI | LessUI |
|--------|-------|--------|--------|
| Resampling | Nearest-neighbor | libsamplerate | Linear interpolation |
| Rate control | None (blocking) | Cubic + averaging | Linear (paper formula) |
| Display correction | No | No | Yes (vsync measurement) |
| Adaptive d | No | No | Yes (2% → 0.5%) |
| Memory | Zero | malloc/free per batch | Zero |
| Dependencies | None | libsamplerate | None |
| Max pitch shift | 0% | ±1% | ±0.5% (after measurement) |
| Handles mismatch | No | Partially | Yes (via base correction) |

## Why This Approach

1. **Mathematically proven** - Paper proves exponential convergence to stable equilibrium
2. **Separation of concerns** - Base correction for mismatch, adaptive d for jitter
3. **Bootstrap-friendly** - Large d handles unknown mismatch during measurement
4. **Filters startup noise** - Stability detection waits for clean readings
5. **Zero allocations** - Critical for memory-constrained handhelds
6. **No dependencies** - Easy cross-compilation for 15+ platforms
7. **Stable on all devices** - Converges to 50% regardless of display timing
8. **Simple code** - Clear inline calculation, easy to understand

## References

- Arntzen, H.K. (2012). ["Dynamic Rate Control for Retro Game Emulators"](https://docs.libretro.com/guides/ratecontrol.pdf)
- Implementation: `workspace/all/common/audio_resampler.c`
- Integration: `workspace/all/common/api.c` (`SND_calculateRateAdjust`)
- CPU Scaling: `docs/auto-cpu-scaling.md` (uses frame timing, not buffer fill)
