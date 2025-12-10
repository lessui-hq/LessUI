# Auto CPU Scaling

Dynamic CPU frequency scaling for libretro emulation based on frame timing.

**GitHub Issue**: [#44 - Add Auto CPU Scaling Option](https://github.com/nchapman/LessUI/issues/44)

## Overview

Add an "Auto" CPU speed option that dynamically scales between existing power levels (POWERSAVE/NORMAL/PERFORMANCE) based on real-time emulation performance, saving battery when possible and boosting when needed.

**Status:** ✅ Granular frequency scaling implemented. Auto mode now uses all available CPU frequencies detected from the system.

## Design Approach

### ⚠️ Audio Buffer Fill - Why It Doesn't Work for CPU Scaling

**Initial hypothesis:** Audio buffer fill directly measures CPU performance.

- Low buffer = CPU struggling
- High buffer = CPU has headroom

**Reality discovered through testing:**

Audio buffer fill is contaminated by **timing mismatches** between display refresh rate and audio sample rate, which have nothing to do with CPU performance.

| Device     | Display         | Buffer Fill | CPU Usage | Issue                     |
| ---------- | --------------- | ----------- | --------- | ------------------------- |
| tg5040     | ~60.5 Hz (fast) | 84%         | 71%       | Overfilling (good timing) |
| rg35xxplus | ~59.7 Hz (slow) | 23-40%      | 44-52%    | Draining (bad timing)     |

On rg35xxplus, low buffer fill triggered max CPU even though CPU was only 50% loaded. The system was fighting **display timing**, not CPU load.

**Why this happens:**

- NES outputs 60.0988 Hz
- rg35xxplus display runs at 59.711 Hz (-0.65% mismatch)
- Rate control compensates with pitch shift (up to ±0.5%)
- If mismatch exceeds rate control range, buffer drifts away from 50%
- Buffer fill now reflects timing quality, not CPU performance

**Conclusion:** Audio buffer fill is useful for tuning the rate control system itself, but cannot be used to detect CPU-bound conditions.

### Recommended Approach: Frame Timing

Measure how long `core.run()` takes each frame:

```c
uint32_t frame_start = SDL_GetTicks();
core.run();
uint32_t frame_time_us = (SDL_GetTicks() - frame_start) * 1000;

// At 60fps: target = 16666 us
// If frame_time consistently > 15000 us (90% of budget), CPU is struggling
```

**Why this works:**

- ✓ **Direct measurement** - measures actual emulation workload
- ✓ **Independent of audio** - not affected by timing mismatches
- ✓ **Independent of buffer size** - measures CPU capability, not I/O
- ✓ **Already available** - no new infrastructure needed

**Algorithm:**

1. Window-average frame time over 30 frames (~500ms)
2. Compare to target frame budget (16.67ms at 60fps)
3. Boost if consistently >90% of budget (CPU maxed out)
4. Reduce if consistently <70% of budget (CPU has headroom)

### Critical Discovery: Render Loop Architecture

**The fundamental issue:** VSync location determines whether rate control can work as designed.

#### Initial Architecture (Broken)

```c
while (!quit) {
    core.run()
      └─> video_refresh_callback()
            └─> GFX_blitRenderer()
            └─> GFX_flip()  ← VSYNC BLOCKS HERE (inside core.run!)
}
```

**Problem:** Core is throttled to display rate (59.71 Hz), not its natural rate (60.10 Hz).

- Produces audio at 59.71 fps instead of 60.10 fps
- Creates -0.65% audio production deficit
- Buffer drains regardless of CPU performance
- Rate control fights deficit that shouldn't exist

#### Current Architecture (Improved)

```c
while (!quit) {
    core.run()
      └─> video_refresh_callback()
            └─> GFX_blitRenderer()
            └─> frame_ready_for_flip = 1

    if (frame_ready_for_flip)
        GFX_flip()  ← VSYNC BLOCKS HERE (after core.run)
}
```

**Progress:** Core.run() measures actual CPU time, not vsync wait.

- Frame timing now accurate (42% util matches CPU usage)
- Auto CPU scaling works correctly

**Remaining issue:** Loop still blocked on vsync, so core.run() called at display rate (59.71 Hz).

- Still produces -0.65% less audio than expected
- Buffer settles at ~17-30%, not the ideal 50%

**The math:** With d=0.5% and 0.65% mismatch, equilibrium is:

```
adjustment = 1.0 - (1.0 - 2*fill) * 0.005 = 0.9935
fill = 0.15 (15%)
```

Buffer CANNOT settle at 50% when display ≠ core rate, no matter the buffer size.

#### Possible Solutions

**Option 1: Free-running core (per libretro paper)**

```c
while (!quit) {
    core.run()           // Run as fast as CPU allows
    push_audio()         // Non-blocking

    if (should_flip)     // Throttle separately
        GFX_flip()       // ~60Hz vsync
}
```

**Benefits:**

- Core runs at natural rate (60.10 Hz)
- Audio production matches expectations
- Buffer settles at 50% as designed
- Rate control only compensates for oscillator tolerances

**Challenges:**

- Need frame pacing logic to decide when to flip
- Potential for frame drops if CPU falls behind
- More complex than current architecture

**Option 2: Accept the equilibrium offset**

- Keep current architecture (simple, proven)
- Buffer settles at 15-30% instead of 50%
- Increase buffer size for safety margin (6-8 frames)
- Works well enough in practice

**Option 3: Adjust rate control baseline**

- Bias the rate adjustment formula to compensate for known mismatch
- Would make buffer settle at 50% but less generalizable
- Couples rate control to specific display timing

### Design Evolution

**Iteration 1:** Rate control stress metric
**Iteration 2:** Audio buffer fill (contaminated by vsync-in-callback)
**Iteration 3:** Frame timing with vsync-after-run (current)
**Iteration 4 (future?):** Free-running core with independent vsync

### Two-Layer Architecture

| Layer                 | Handles                    | Magnitude                     | Speed      |
| --------------------- | -------------------------- | ----------------------------- | ---------- |
| **Rate control (PI)** | Jitter + persistent drift  | ±1% (proportional) + integral | Per-frame  |
| **CPU scaling**       | Sustained performance gaps | 10-50%+                       | Per-second |

Rate control handles small timing variations. CPU scaling handles sustained performance problems that rate control can't fix.

### Benefits Over Alternatives

**vs. Background CPU monitoring (NextUI approach)**:

- Measures actual emulation performance, not system CPU %
- Already calculated every frame
- Directly tied to user-perceived quality
- Uses dedicated thread only for applying changes (not monitoring)

**vs. Inline CPU frequency changes**:

- Background thread prevents main loop blocking
- Works with all platforms (even complex overclock.elf)
- No platform refactoring needed
- system() calls happen off the critical path

## Implementation Plan

### Phase 1: Expose Stress Metric ✅

- [x] Add `SND_getRateControlStress()` function to api.c
- [x] Returns normalized stress (0.0 = relaxed, 1.0 = maxed out)
- [x] Add `SND_getUnderrunCount()` and `SND_resetUnderrunCount()` for panic path
- [x] Store `last_rate_adjust` in SND_Context for stress calculation
- [x] Track rolling average implemented in Phase 2 via window-based averaging

### Phase 2: CPU Scaling Logic in minarch ✅

- [x] Add `auto_cpu_level` state (0=POWERSAVE, 1=NORMAL, 2=PERFORMANCE)
- [x] Implement window-based averaging (~500ms = 30 frames at 60fps)
- [x] Implement consecutive window counting for hysteresis
- [x] Asymmetric timing: fast boost (2 windows), slow reduce (4 windows)
- [x] Add `updateAutoCPU()` called from main loop

### Phase 3: Emergency Escalation ✅

- [x] Track actual underrun events in audio callback (`snd.underrun_count`)
- [x] Expose `SND_getUnderrunCount()` and `SND_resetUnderrunCount()`
- [x] On underrun: boost by max 2 steps, 4s cooldown before reducing

### Phase 4: Special State Handling ✅

- [x] Disable auto-scaling during `fast_forward`
- [x] Disable auto-scaling during `show_menu`
- [x] Ignore first ~5 seconds after game start (`AUTO_CPU_STARTUP_GRACE = 300`)
- [x] Returns 0 stress if audio not initialized (safe fallback)

### Phase 5: Menu Integration ✅

- [x] Add "Auto" option to CPU Speed menu (overclock_labels[3])
- [x] Updated option count from 3 to 4
- [x] Updated description to mention Auto mode
- [x] Persists via existing config system (`minarch_cpu_speed`)
- [x] **Debug HUD**: Shows CPU level and metrics in bottom-left corner
  - Manual mode: `L1 b:48%` (level + buffer fill)
  - Auto mode: `L1 u:52% b:48%` (level + utilization + buffer fill)
  - Added bitmap font characters: L, b, u, r, :, !, F, P, S

### Phase 6: Platform Support ✅

- [x] Audit platforms - which have working `PLAT_setCPUSpeed`?
- [x] Implement CPU scaling for rg35xxplus (H700 cpufreq sysfs)

## Threading Architecture

Auto CPU scaling uses a **two-thread design** to keep the main emulation loop responsive:

### Main Thread (Emulation)

- Measures `core.run()` execution time every frame
- Stores frame times in ring buffer for percentile analysis
- Every window (~500ms), calculates 90th percentile utilization
- Decides when CPU level should change based on sustained utilization
- **Sets target level** (non-blocking, mutex-protected)
- Never blocks on expensive CPU frequency changes

### Background Thread (CPU Applier)

- Polls every 50ms checking for target changes
- When target ≠ current, applies the change
- Calls `PWR_setCPUSpeed()` which may fork `system("overclock.elf")`
- Updates current level after successful application
- Stops cleanly when exiting auto mode

### Thread Safety

```c
// Shared state protected by mutex
pthread_mutex_t auto_cpu_mutex;
int auto_cpu_target_level;   // What main thread wants
int auto_cpu_current_level;  // What's actually applied

// Main thread: non-blocking request
void auto_cpu_setTargetLevel(int level) {
    pthread_mutex_lock(&auto_cpu_mutex);
    auto_cpu_target_level = level;
    pthread_mutex_unlock(&auto_cpu_mutex);
}

// Background thread: applies changes
while (running) {
    if (target != current) {
        PWR_setCPUSpeed(...);  // May block with system()
        current = target;
    }
    usleep(50000);  // 50ms poll
}
```

**Why this works:**

- Main loop never blocks (just sets target and returns)
- Background thread absorbs any system() latency
- Worst case: 50ms delay between decision and application
- No platform code changes needed

## Algorithm Details

### Frame Timing (Current Implementation - Iteration 5)

Measures how long `core.run()` takes each frame and compares to the frame budget:

```c
// In main loop, around core.run()
uint64_t frame_start = getMicroseconds();
core.run();
uint64_t frame_time = getMicroseconds() - frame_start;

// Store in ring buffer for analysis
frame_times[frame_index++ % WINDOW_FRAMES] = frame_time;
```

**Key metrics:**

- **Frame budget**: 1,000,000 / core.fps microseconds (e.g., 16,639µs for NES at 60.0988 Hz)
- **Utilization**: frame_time / frame_budget as percentage
- **90th percentile**: Ignores outliers like loading screens, measures actual gameplay

### Window-Based Scaling

```c
// Every window (~500ms = 30 frames at 60fps)
if (frame_count >= WINDOW_FRAMES) {
    // Calculate 90th percentile frame time (ignores loading screens)
    uint64_t p90_time = percentileUint64(frame_times, WINDOW_FRAMES, 0.90f);

    // Calculate utilization as % of frame budget
    unsigned util = (p90_time * 100) / frame_budget_us;

    if (util > 85) {
        // CPU is struggling - using >85% of frame budget
        high_util_windows++;
        low_util_windows = 0;
    } else if (util < 55) {
        // CPU has plenty of headroom
        low_util_windows++;
        high_util_windows = 0;
    } else {
        // Sweet spot - reset both
        high_util_windows = 0;
        low_util_windows = 0;
    }

    // Asymmetric: fast to boost (2 windows), slow to reduce (4 windows)
    if (high_util_windows >= 2 && cpu_level < MAX_LEVEL) {
        boost_cpu();
    }
    if (low_util_windows >= 4 && cpu_level > MIN_LEVEL) {
        reduce_cpu();
    }

    frame_count = 0;
}
```

### Panic Path (Audio Underrun Detection)

Audio underruns are kept as an emergency signal - they're definitive proof of a problem:

```c
// In audio callback, when underrun detected:
if (underrun_occurred) {
    underrun_count++;
}

// In main loop:
if (SND_getUnderrunCount() > last_underrun_count) {
    // Emergency! Boost by up to MAX_STEP (not straight to max)
    new_idx = current_idx + AUTO_CPU_MAX_STEP;
    if (new_idx > max_idx) new_idx = max_idx;
    apply_frequency(new_idx);
    panic_cooldown = 8;  // ~4 seconds before allowing reduction
}
```

### Why Frame Timing Instead of Buffer Fill

| Metric                | Buffer Fill                       | Frame Timing                |
| --------------------- | --------------------------------- | --------------------------- |
| **Measures**          | Audio/display timing sync         | Actual CPU work             |
| **Problem**           | Contaminated by timing mismatches | Direct measurement          |
| **rg35xxplus 59.7Hz** | Triggers boost at 50% CPU         | Correctly shows ~50% util   |
| **PAL games**         | Different buffer dynamics         | Auto-adjusts via core.fps   |
| **Loading screens**   | Can trigger false boosts          | Filtered by 90th percentile |

## Platform CPU Speed Support

### Benchmarked Platforms

| Platform      | Architecture        | Available Frequencies (kHz)                                       | Current Config                                                                  | Status          |
| ------------- | ------------------- | ----------------------------------------------------------------- | ------------------------------------------------------------------------------- | --------------- |
| **tg5040**    | Cortex-A53 (64-bit) | 408, 600, **816**, 1008, **1200**, 1416, **1608**, 1800, **2000** | MENU=816, POWERSAVE=1200, NORMAL=1608, PERFORMANCE=2000 (74.6% ratio)           | ✅ Optimal      |
| **zero28**    | Cortex-A53 (64-bit) | 408, 600, 816, 1008, **1200**, 1416, **1608**, **1800**           | MENU=800, POWERSAVE=1200, NORMAL=1608, PERFORMANCE=1800 (74.6% ratio)           | ✅ Fixed        |
| **miyoomini** | Cortex-A7 (32-bit)  | **400**, **600**, 800, 1000, **1100**, **1600**                   | MENU=600, POWERSAVE=1100, NORMAL=1600, PERFORMANCE=1600 (68.8% ratio, N/P gap!) | ⚠️ Hardware gap |

**Bold frequencies** = currently configured bands

### Performance Comparison

| Platform  | Max Freq | Max Performance | vs. miyoomini | IPC Advantage     |
| --------- | -------- | --------------- | ------------- | ----------------- |
| miyoomini | 1600 MHz | 371k iters      | Baseline      | -                 |
| zero28    | 1800 MHz | 448k iters      | +20.8%        | +7.3% (A53 vs A7) |
| tg5040    | 2000 MHz | 496k iters      | +33.7%        | +7.3% (A53 vs A7) |

### miyoomini Hardware Gap

**Hardware limitation confirmed via 50MHz sweep**: miyoomini has a **500 MHz gap** between 1100-1600 MHz. Any request in the range 1150-1600 snaps to maximum (1600 MHz).

**Current configuration** (aligned to hardware):

- MENU: 600 MHz
- POWERSAVE: 1100 MHz
- NORMAL: 1600 MHz (no middle option due to gap)
- PERFORMANCE: 1600 MHz (same as NORMAL)

**Limitation:** NORMAL and PERFORMANCE are identical due to hardware gap. This is unavoidable without using lower frequencies (1100 MHz) which would be too slow for "NORMAL" tier on this platform.

### Other Platforms (Not Benchmarked)

| Platform    | Status     | Method        | Granular Support     |
| ----------- | ---------- | ------------- | -------------------- |
| my355       | ✅ Working | sysfs         | ✅ Runtime detection |
| rgb30       | ✅ Working | sysfs         | ✅ Runtime detection |
| magicmini   | ✅ Working | sysfs+GPU     | ✅ Runtime detection |
| trimuismart | ✅ Working | sysfs         | ✅ Runtime detection |
| rg35xx      | ✅ Working | overclock.elf | ✅ Runtime detection |
| my282       | ✅ Working | overclock.elf | ✅ Runtime detection |
| rg35xxplus  | ✅ Working | sysfs         | ✅ Runtime detection |
| m17         | ❌ Fixed   | -             | ❌ 3-level fallback  |

**Granular Support Notes:**

- All sysfs-based platforms: Read frequencies from `scaling_available_frequencies`, set via `scaling_setspeed`
- overclock.elf platforms: Read frequencies from sysfs, set via platform-specific overclock.elf
- m17: Fixed 1200 MHz clock, detection returns 0, uses 3-level fallback mode

## Audio System Tuning

### Rate Control Pitch Deviation (d)

The **d parameter** determines how much pitch adjustment the rate control algorithm can apply for jitter compensation. See [docs/audio-rate-control.md](audio-rate-control.md) for the full algorithm derivation.

**Current implementation (PI Controller):**

```c
// Rate control gains (api.c)
#define SND_RATE_CONTROL_D_DEFAULT 0.010f  // 1.0% - proportional gain
#define SND_RATE_CONTROL_KI 0.00005f       // integral gain (drift correction)
#define SND_ERROR_AVG_ALPHA 0.003f         // error smoothing (~333 frame average)
#define SND_INTEGRAL_CLAMP 0.02f           // ±2% max drift correction
```

**Why dual-timescale PI controller works:**

- Error smoothing (α=0.003) filters jitter before it reaches the integral term
- Proportional term (d=1.0%) provides immediate response to buffer level changes
- Integral term operates on slower timescale, learning persistent clock offset
- Integral clamped to ±2% handles hardware clock mismatch up to ±2%
- P and I can't fight because they operate on different timescales

### Audio Buffer Size

Our audio buffer holds **5 video frames** of audio (~83ms at 60fps):

```c
snd.buffer_video_frames = 5;
snd.frame_count = snd.buffer_video_frames * snd.sample_rate_in / snd.frame_rate;
```

With the PI controller, the buffer settles near 50-65% fill depending on device clock characteristics, providing headroom for jitter and ~42ms effective latency.

## Benchmark Methodology

The discovered frequency steps and performance data come from a custom CPU benchmark tool (`workspace/all/paks/Benchmark/`):

**Workload**: xorshift PRNG (1000 iterations per call)

- CPU-bound integer operations (XOR, shift, no memory bottleneck)
- Prevents dead code elimination via `volatile` sink
- Compiled with `-O2` for realistic optimization

**Test protocol**:

- 1500ms measurement window per frequency
- 500ms warmup to stabilize frequency
- 0.5s cooldown between tests (thermal management)
- **Initial discovery**: 50MHz increments (min→max) to find all frequency steps
- **Subsequent tests**: Use discovered exact steps for efficiency

**miyoomini discovery process**:

1. First run with hardcoded `overclock.c` values (240-1488 MHz in 10 steps)
2. Results showed unexpected snapping (many requests → 1600 MHz)
3. Second run with 50MHz sweep (400-1600 MHz, 25 steps)
4. Confirmed hardware steps: 400, 600, 800, 1000, 1100, 1600 MHz
5. **Discovered 500 MHz gap** between 1100-1600 MHz

**Key discovery**: Running the presenter in background (`&`) during benchmarks interfered with CPU frequency scaling on some platforms. All visual feedback is now blocking to ensure accurate measurements.

**Output**: CSV files in `USERDATA_PATH/logs/` with columns:

- `timestamp`: ISO 8601 timestamp
- `platform`: Platform identifier
- `freq_khz`: Requested frequency
- `actual_khz`: Kernel-reported frequency (validates hardware snapping)
- `iterations`: Work units completed (scales linearly with freq)
- `duration_ms`: Actual measurement duration
- `temp_mC`: SoC temperature in millicelsius (thermal monitoring)

## References

- [Dynamic Rate Control for Retro Game Emulators](https://docs.libretro.com/guides/ratecontrol.pdf) - Hans-Kristian Arntzen, 2012
- [docs/audio-rate-control.md](audio-rate-control.md) - Our rate control implementation
- [workspace/all/common/api.c](../workspace/all/common/api.c) - `SND_calculateRateAdjust()`, `PWR_getAvailableCPUFrequencies_sysfs()`, `PWR_setCPUFrequency_sysfs()`
- [workspace/all/common/api.h](../workspace/all/common/api.h) - `PLAT_getAvailableCPUFrequencies()`, `PLAT_setCPUFrequency()` API
- [workspace/all/minarch/minarch.c](../workspace/all/minarch/minarch.c) - Main emulation loop, `updateAutoCPU()`, `auto_cpu_detectFrequencies()`
- [workspace/all/paks/Benchmark/](../workspace/all/paks/Benchmark/) - CPU frequency benchmark tool

## Tuning Status

| Parameter               | Current             | Notes                                             |
| ----------------------- | ------------------- | ------------------------------------------------- |
| Rate control d          | 1.0%                | Proportional gain - handles frame-to-frame jitter |
| Rate control ki         | 0.00005             | Integral gain - learns persistent clock offset    |
| Error smoothing α       | 0.003 (~333 frames) | Separates P and I timescales                      |
| Integral clamp          | ±2%                 | Max drift correction (handles hardware variance)  |
| Audio buffer            | 5 frames (~83ms)    | Effective latency ~42ms at 50% fill               |
| Window size             | 30 frames (~500ms)  | Filters noise, responsive to changes              |
| Utilization high        | 85%                 | Frame time >85% of budget = boost                 |
| Utilization low         | 55%                 | Frame time <55% of budget = reduce                |
| Target util             | 70%                 | Target utilization after frequency change         |
| Max step (reduce/panic) | 2                   | Max frequency steps down (boost unlimited)        |
| Min frequency           | 400 MHz             | Floor for frequency scaling                       |
| Boost windows           | 2 (~1s)             | Fast response to performance issues               |
| Reduce windows          | 4 (~2s)             | Conservative to prevent oscillation               |
| Startup grace           | 300 frames (~5s)    | Starts at max freq, then scales                   |
| Percentile              | 90th                | Ignores outliers (loading screens)                |

### Display Rate Handling

Display refresh rate is queried from SDL at init via `SDL_GetCurrentDisplayMode()`:

| Platform   | SDL Reports | Core expects   | Base Correction   |
| ---------- | ----------- | -------------- | ----------------- |
| rg35xxplus | 60 Hz       | 60.10 Hz (NES) | 60/60.10 = 0.9983 |
| tg5040     | 60 Hz       | 60.10 Hz (NES) | 60/60.10 = 0.9983 |
| miyoomini  | 60 Hz       | 60.10 Hz (NES) | 60/60.10 = 0.9983 |

**Note:** SDL typically reports rounded integer refresh rates (60 Hz). The actual display rate may vary slightly (59.71-60.5 Hz measured via vsync timing). The PI controller's integral term learns and corrects for any mismatch over time.

**How it works:** The PI controller adjusts the resampling ratio based on buffer fill. The proportional term (d) handles jitter, while the integral term slowly learns the persistent timing offset to maintain exactly 50% buffer fill.

### Debug HUD

The debug overlay uses all 4 corners to show performance and scaling info:

```
┌─────────────────────────────────────────┐
│ 60 FPS 45%              256x224 2x      │
│                                         │
│                                         │
│ L1 u:52% b:48%                640x480   │
└─────────────────────────────────────────┘
```

| Corner | Content                        | Purpose                               |
| ------ | ------------------------------ | ------------------------------------- |
| **TL** | `60 FPS 45%`                   | Frame rate + system CPU load          |
| **TR** | `256x224 2x`                   | Core output resolution + scale factor |
| **BL** | `L1 b:48%` or `L1 u:52% b:48%` | CPU level + audio/scaling metrics     |
| **BR** | `640x480`                      | Final output resolution               |

**Bottom-left format:**

- Manual mode: `L1 b:48%` (level + buffer fill)
- Auto mode (fallback): `L1 u:52% b:48%` (level + utilization + buffer fill)
- Auto mode (granular): `1200 u:52% b:48%` (frequency in MHz + utilization + buffer fill)

**Key metrics:**

- `L0/L1/L2` = CPU level (POWERSAVE/NORMAL/PERFORMANCE) - used in manual and fallback modes
- `1200` = CPU frequency in MHz (e.g., 1200 = 1.2 GHz) - used in granular auto mode
- `u:XX%` = Frame timing utilization (90th percentile, % of frame budget)
- `b:XX%` = Audio buffer fill (should converge to ~50%)

The `u:` metric drives auto-scaling decisions:

- `>85%` = CPU struggling, triggers boost after ~1s
- `<55%` = CPU has headroom, triggers reduce after ~2s
- `55-85%` = Sweet spot, no change

## System Validation (December 2025)

### Frame Timing Confirmed as Optimal Metric

After implementing the unified RateMeter system with dual clock correction (display + audio), we revisited whether frame timing remains the best metric for CPU scaling.

**Why frame timing is correct:**

1. **Independent measurement** - Frame timing measures `core.run()` CPU work, while rate control handles audio/video sync using buffer fill. No coupling between systems.

2. **Real-world validation** - FCEUmm testing with high vs low quality audio:
   - High quality: frame timing shows 64-68% util → auto scaler correctly keeps CPU high
   - Low quality: frame timing drops → auto scaler correctly reduces CPU
   - The system responds to actual emulation workload, not arbitrary core labels

3. **No feedback loops** - Buffer fill is influenced by the PI controller rate adjustment and dynamic buffer sizing. Using it for CPU scaling would create two control systems fighting over the same signal.

**The two-layer separation is optimal:**

| Layer        | Domain               | Response Time      | Feedback Signal | What It Controls           |
| ------------ | -------------------- | ------------------ | --------------- | -------------------------- |
| Rate control | Audio/video sync     | Per-frame (~16ms)  | Buffer fill     | Resampler ratio adjustment |
| CPU scaling  | Performance headroom | Per-second (~1-2s) | Frame timing    | CPU frequency              |

### Granular Frequency Scaling (Implemented)

Auto mode now uses **all available CPU frequencies** detected from the system via `scaling_available_frequencies` sysfs interface.

**Key features:**

- Runtime frequency detection via `PLAT_getAvailableCPUFrequencies()`
- Direct frequency setting via `PLAT_setCPUFrequency()`
- Linear performance scaling for intelligent frequency selection
- Minimum frequency floor (400 MHz) filters out unusably slow frequencies
- Automatic fallback to 3-level mode if detection fails

**Algorithm improvements:**

- Performance scales linearly with frequency: `new_util = current_util × (current_freq / new_freq)`
- Target 70% utilization after frequency changes
- **Boost**: Uses linear prediction, no step limit (aggressive is safe)
- **Reduce**: Uses linear prediction, max 2 steps (conservative to avoid underruns)
- **Panic**: Boost by max 2 steps on underrun, 4s cooldown
- **Startup**: Begin at max frequency during 5s grace period

**Preset mapping for manual modes:**

- POWERSAVE: ~25% up from minimum frequency
- NORMAL: ~75% of max frequency
- PERFORMANCE: max frequency

**Example on miyoomini (6 frequencies detected: 400, 600, 800, 1000, 1100, 1600 kHz):**

```
Old: POWERSAVE → NORMAL → PERFORMANCE (3 steps)
New: 400 → 600 → 800 → 1000 → 1100 → 1600 (6 steps, granular)
```

### Frequency Band Analysis

Comprehensive analysis of benchmark data from all platforms revealed optimization opportunities.

**Fixed platform-specific issues:**

| Platform  | Issue                         | Fix Applied                               |
| --------- | ----------------------------- | ----------------------------------------- |
| zero28    | PS/N ratio 57.6% (dangerous!) | Updated to 1200/1608/1800 (74.6% ratio) ✓ |
| magicmini | PS/N ratio 57.6% (dangerous!) | Updated to 1200/1608/1800 (74.6% ratio) ✓ |

**Analysis output:** See `scripts/analyze-cpu-bands.py` and `scripts/analyze-frequency-strategies.py` for detailed frequency analysis and strategy comparison.

### Threshold Validation

The 55% LOW threshold and 85% HIGH threshold were chosen empirically but are now validated:

**Safety check for 55% threshold:**

```
At NORMAL with 55% util, reduce to POWERSAVE:
new_util = 55% / (POWERSAVE_freq / NORMAL_freq)

Safe if: new_util ≤ 85%
Requires: POWERSAVE_freq / NORMAL_freq ≥ 65%
```

All platforms now meet this requirement after fixes.

## Future Work

### Manual Band Optimization

Determine optimal POWERSAVE/NORMAL/PERFORMANCE frequencies using data-driven approach.

**Strategies evaluated:**

1. **Percentage (75%/85%/100%)** - Consistent meaning across platforms ← Recommended
2. **Performance targets** - Absolute performance levels (breaks on weak devices)
3. **Equal gaps** - Balanced tiers but ignores efficiency
4. **Max efficiency** - Best perf/watt but unsafe ratios

**Recommendation: Percentage strategy**

- POWERSAVE: 75% of max frequency
- NORMAL: 85% of max frequency
- PERFORMANCE: 100% of max frequency

This provides:

- Consistent user experience across all devices
- Safe ratios (70-80% typical)
- Works within each platform's capabilities
- Simple to understand ("three-quarter speed / nearly full / maximum")

### CPU_SPEED_MENU Reconsideration

**Current behavior:**

- MENU runs at 600-816 MHz during launcher/menu UI
- Arbitrary and platform-inconsistent
- Too slow for heavy operations (image processing, pak generation)
- Adds complexity to platform.c implementations

**Options:**

1. **Eliminate MENU** - Use NORMAL everywhere (simpler)
2. **Context-dependent** - Low for UI browsing, higher for operations
3. **Auto for menu** - Apply same scaling logic to launcher

**Recommendation:** TBD - needs testing on actual devices to measure menu performance needs
