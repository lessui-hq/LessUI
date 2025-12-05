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

| Platform | Measured Hz | Swing | Core expects | Mismatch |
|----------|-------------|-------|--------------|----------|
| rg35xxplus | 59.77 Hz | 0.89 Hz | 60.10 Hz (NES) | -0.55% |
| miyoomini | 59.66 Hz | 0.41 Hz | 60.10 Hz (NES) | -0.73% |

### Solution: Adaptive d + Dual Correction

**Why not just use d=2% always?** Because it causes oscillation once corrections are applied.

The problem: A large static d handles mismatch well, but becomes too aggressive once the mismatch is corrected. The solution is to **separate static mismatch from dynamic jitter** using dual correction + adaptive d.

We use a two-phase approach:

**Phase 1: Measurement (first ~0.5-20 seconds)**
- Use large d=2% to handle unknown display/audio mismatch
- Measure display rate via vsync timing (per-frame samples)
- Measure audio rate via callback timing (2-second windows)
- Wait for stable readings (30 display samples with spread < 1.0 Hz, 10 audio samples with spread < 500 Hz)

**Phase 2: Refined (after meters stable)**
- Apply base_correction for static display/core offset
- Apply audio_correction for audio clock drift
- Switch to small d=1% for jitter only

```c
// Adaptive d parameter (2% before meters stable, 1% after)
int meters_stable = RateMeter_isStable(&display_meter) && RateMeter_isStable(&audio_meter);
float d = meters_stable ? 0.01 : 0.02;  // 1% or 2%

// Base correction (display clock drift)
float base_correction = display_hz / core_hz;  // e.g., 59.77/60.10 = 0.9945

// Audio correction (audio clock drift)
float audio_correction = sample_rate_out / audio_hz;  // e.g., 48000/47969 = 1.0007

// Dynamic adjustment (jitter compensation)
float rate_adjust = 1 - (1 - 2*fill) * d;  // 1.0 ± d

// Combined (three corrections multiplied)
float total_adjust = base_correction * audio_correction * rate_adjust;
```

This keeps d optimal for each phase while handling real-world display variance and audio clock drift.

## Pitch Audibility

With d=1% (after stable) and dual correction handling both display and audio clock drift separately:

1. **Base correction**: Static offset for display drift (e.g., -0.55%), always applied, inaudible
2. **Audio correction**: Static offset for audio clock drift (e.g., +0.07%), always applied, inaudible
3. **Rate control**: Dynamic ±1% for jitter, averages ~0.1% in practice
4. **Combined**: Total deviation well under 1% (human perception threshold)
5. **Stable at 50%**: Maximum headroom for jitter in both directions

## Implementation Details

### Components

1. **RateMeter** (`rate_meter.c`) - Unified measurement module
   - Ring buffer of Hz samples with running median
   - Min/max tracking for swing detection (used for buffer sizing)
   - Stability detection based on configurable spread threshold
   - Same algorithm for both display and audio rates

2. **Linear Interpolation Resampler** (`audio_resampler.c`)
   - Fixed-point 16.16 math for efficiency on ARM
   - Smoothly blends between adjacent samples
   - Zero memory allocation (uses ring buffer directly)

3. **Dynamic Rate Control** (`api.c`)
   - Calculates adjustment factor each audio batch
   - Single formula using meter values directly
   - Values continuously improve as meters gather more samples

4. **Dynamic Buffer Sizing** (`api.c`)
   - Base buffer of 4 frames (~67ms)
   - Automatically increases based on measured swing
   - Higher timing variance = more headroom needed
   - Caps at 8 frames maximum

### Key Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Rate control d (adaptive) | 2% → 1% | Large before meters stable, small after |
| Display meter window | 30 samples | ~0.5 sec at 60fps |
| Display stability | spread < 1.0 Hz | Relaxed for handheld device jitter |
| Display interval | Every frame | Per-frame vsync measurements |
| Audio meter window | 10 samples | ~20 sec total (10 × 2s windows) |
| Audio stability | spread < 500 Hz | SDL callback bursts cause 200-800 Hz variance |
| Audio interval | 2.0 seconds | Averages out SDL callback burst patterns |
| Base buffer | 4 frames | ~67ms latency at 60fps |
| Max buffer | 8 frames | Hard limit (~133ms) |
| Target fill | 50% | Maximum jitter headroom |
| Safety clamp | ±5% | Catches bugs without limiting normal operation |

### Code Flow

```
SND_init()
  └─> Initialize display_meter and audio_meter

Main loop (frames 1-N):
  ├─> core.run()
  │     └─> audio_callback(samples)
  │           └─> SND_batchSamples()
  │                 ├─> base_correction = display_meter.median / core_fps
  │                 ├─> audio_correction = sample_rate_out / audio_meter.median
  │                 ├─> d = meters_stable ? 0.01 : 0.02
  │                 ├─> rate_adjust = 1 - (1 - 2*fill) * d
  │                 └─> total_adjust = base × audio × rate
  │
  ├─> measureDisplayRate()
  │     └─> SND_addDisplaySample(hz)
  │           └─> (when stable) → SND_updateBufferForSwing()
  │
  └─> measureAudioRate()
        └─> SND_addAudioSample(hz)
```

## Design Evolution

| Iteration | Approach | Issue |
|-----------|----------|-------|
| 1 | Rate control d=0.5% only | Insufficient for display/core mismatch (-0.73% on miyoomini) |
| 2 | Rate control d=1.5% only | Worked on rg35xxplus, oscillated on tg5040 |
| 3 | Rate control d=2% only | **Worse oscillation** - too aggressive after correction applied |
| 4 | SDL display query + d=0.5% | SDL rounds to integer (60 vs 59.71), inaccurate |
| 5 | Fixed 30-frame measurement | Captured startup noise, bad readings |
| 6 | Adaptive d + dual correction | **Key insight**: Separate static mismatch from dynamic jitter |
| 7 | Stability detection + median | Filters noise, waits for good data before switching d |
| 8 | **Unified RateMeter + dynamic buffer** | Same algorithm for both, continuous refinement, adaptive buffer sizing |

**The key lesson from iteration 3**: You can't just "raise d to handle handheld variance." A large static d overcorrects jitter once base/audio corrections eliminate the static mismatch. The solution requires **adaptive d** (large during bootstrap, small after correction) + **dual correction** (separate display and audio clock drift from jitter).

**Current approach**:
- Unified RateMeter module handles both display and audio measurement
- Meters track running median, min/max swing, and stability
- Values improve when better data arrives (smaller spread)
- Display swing triggers dynamic buffer resizing (4-8 frames)
- Large d (2%) until meters stable, then small d (1%) for jitter only

### Measurement Interval Strategy

Display and audio use different measurement intervals to match their signal characteristics:

**Display (per-frame):**
- Vsync timing is consistent frame-to-frame
- 30 samples × 16.7ms = ~0.5 sec total
- Median filters outliers from occasional frame drops
- Achieves stability: spread < 1 Hz (typical: 0.4-0.9 Hz)

**Audio (2-second windows):**
- SDL callbacks have burst patterns (40-53 kHz per 100ms)
- But average is stable over longer windows (47-48 kHz per 2s)
- 10 samples × 2 sec = ~20 sec total measurement
- Median filters occasional burst spikes
- Achieves stability on clean devices: spread < 500 Hz (typical: 150-300 Hz)
- Falls back gracefully on noisy devices (miyoomini: ~800 Hz spread)

## Comparison with Other Implementations

| Aspect | MinUI | NextUI | LessUI |
|--------|-------|--------|--------|
| Resampling | Nearest-neighbor | libsamplerate | Linear interpolation |
| Rate control | None (blocking) | Cubic + averaging | Linear (paper formula) |
| Display correction | No | No | Yes (RateMeter) |
| Audio correction | No | No | Yes (RateMeter) |
| Adaptive d | No | No | Yes (2% → 1%) |
| Dynamic buffer | No | No | Yes (swing-based) |
| Memory | Zero | malloc/free per batch | Zero |
| Dependencies | None | libsamplerate | None |
| Max pitch shift | 0% | ±1% | ±2% → ±1% (adaptive) |
| Handles mismatch | No | Partially | Yes (via dual correction) |

## Augmentations for Handheld Devices

LessUI extends the original Arntzen algorithm with the following enhancements for cheap handheld hardware:

### 1. **Dual Clock Correction** (original paper had none)
- **Base correction**: Measured display_hz / core_fps (handles display drift)
- **Audio correction**: Measured sample_rate_out / audio_hz (handles audio clock drift)
- **Why needed**: Cheap oscillators on handhelds cause both display and audio clock drift
- **Original paper assumed**: Accurate clocks, only needed dynamic d for jitter

### 2. **Adaptive d Parameter** (original paper used static d)
- **Implementation**: 2% before meters stable, 1% after
- **Why complexity is necessary**: You might think "just use d=2% always" but this causes bad oscillation
- **The problem with static d=2%**:
  - Works great during measurement phase (handles -0.73% mismatch + jitter)
  - But once base_correction and audio_correction are applied, mismatch is eliminated
  - Now d=2% is TOO AGGRESSIVE for jitter-only response
  - Buffer oscillates instead of settling (e.g., tested on tg5040: 30-70% swings)
- **The solution**: Separate static correction (base/audio) from dynamic adjustment (d)
  - Large d (2%) during bootstrap handles unknown mismatch
  - Small d (1%) after measurement handles only jitter (corrections handle mismatch)
  - Buffer converges smoothly to 50% on all devices
- **Original paper used**: Static 0.2-0.5% (sufficient for stable desktop monitors with no clock drift)

### 3. **Unified RateMeter System** (original paper didn't measure rates)
- **Implementation**: Same algorithm measures both display and audio rates continuously
- **Features**: Median filtering, min/max tracking, stability detection, continuous refinement
- **Why needed**: Handheld displays vary 57-62 Hz, need continuous measurement not SDL queries

### 4. **Different Measurement Intervals** (original paper didn't specify)
- **Display**: Per-frame samples (30 × 16.7ms = 0.5s window)
- **Audio**: 2-second windows (10 × 2s = 20s total)
- **Why needed**: SDL callback bursts (40-53 kHz per 100ms) need averaging over 2s windows

### 5. **Dynamic Buffer Sizing** (original paper used static buffer)
- **Implementation**: Base 4 frames, grows to 8 frames based on measured display swing
- **Formula**: Extra frames = (swing_hz / display_hz) × base_frames × 1.5 safety
- **Why needed**: High timing variance requires more headroom (miyoomini: 0.4 Hz swing, rg35xxplus: 0.9 Hz)

### 6. **Relaxed Stability Thresholds** (original paper didn't address noisy signals)
- **Display**: 1.0 Hz spread (vs desktop monitors: <0.1 Hz)
- **Audio**: 500 Hz spread (accommodates SDL callback burst patterns)
- **Why needed**: Cheap hardware has noisier timing, need thresholds that filter outliers but still converge

### 7. **Graceful Fallback** (original paper assumed good data)
- **Implementation**: If audio meter never stabilizes (spread > 500 Hz), returns 0 (no correction)
- **Example**: Miyoomini audio meter sees 764 Hz spread, falls back to display correction only
- **Why needed**: Some platforms have such noisy audio timing that correction would hurt, not help

## Why This Approach

1. **Mathematically proven foundation** - Paper proves exponential convergence to stable equilibrium
2. **Extended for real hardware** - Augmentations handle cheap handheld device realities
3. **Unified algorithm** - Same RateMeter code handles both display and audio
4. **Continuous refinement** - Values improve as meters gather more samples
5. **Dynamic adaptation** - Buffer sizing and d parameter adjust to device characteristics
6. **Bootstrap-friendly** - Large d handles unknown mismatch during measurement
7. **Filters noise** - Median-based calculation ignores outliers
8. **Zero allocations** - Critical for memory-constrained handhelds
9. **No dependencies** - Easy cross-compilation for 15+ platforms
10. **Stable on all devices** - Converges to 50% regardless of display/audio timing quality
11. **Simple code** - Modular, testable, easy to understand

## References

- Arntzen, H.K. (2012). ["Dynamic Rate Control for Retro Game Emulators"](https://docs.libretro.com/guides/ratecontrol.pdf)
- Rate measurement: `workspace/all/common/rate_meter.c`
- Resampling: `workspace/all/common/audio_resampler.c`
- Integration: `workspace/all/common/api.c` (`SND_batchSamples`, `SND_addDisplaySample`, `SND_addAudioSample`)
- CPU Scaling: `docs/auto-cpu-scaling.md` (uses frame timing, not buffer fill)
