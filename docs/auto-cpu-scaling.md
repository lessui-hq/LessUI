# Auto CPU Scaling

Dynamic CPU frequency scaling for libretro emulation based on frame timing.

**GitHub Issue**: [#44 - Add Auto CPU Scaling Option](https://github.com/nchapman/LessUI/issues/44)

## Overview

Add an "Auto" CPU speed option that dynamically scales between existing power levels (POWERSAVE/NORMAL/PERFORMANCE) based on real-time emulation performance, saving battery when possible and boosting when needed.

## Design Approach

### ⚠️ Audio Buffer Fill - Why It Doesn't Work for CPU Scaling

**Initial hypothesis:** Audio buffer fill directly measures CPU performance.
- Low buffer = CPU struggling
- High buffer = CPU has headroom

**Reality discovered through testing:**

Audio buffer fill is contaminated by **timing mismatches** between display refresh rate and audio sample rate, which have nothing to do with CPU performance.

| Device | Display | Buffer Fill | CPU Usage | Issue |
|--------|---------|-------------|-----------|-------|
| tg5040 | ~60.5 Hz (fast) | 84% | 71% | Overfilling (good timing) |
| rg35xxplus | ~59.7 Hz (slow) | 23-40% | 44-52% | Draining (bad timing) |

On rg35xxplus, low buffer fill triggered max CPU even though CPU was only 50% loaded. The system was fighting **display timing**, not CPU load.

**Why this happens:**
- NES outputs 60.0988 Hz
- rg35xxplus display runs at 59.711 Hz (-0.65% mismatch)
- Rate control compensates with pitch shift (up to ±2%)
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

### Design Evolution

**Iteration 1:** Rate control stress metric
- Too abstract, symmetric around 50% (both high and low = stressed)

**Iteration 2:** Audio buffer fill
- More intuitive, asymmetric (only low fill = problem)
- But contaminated by display/audio timing mismatches
- Works for tuning rate control, fails for CPU scaling

**Iteration 3 (recommended):** Frame timing
- Direct CPU performance measurement
- Unaffected by timing mismatches
- Actually tells us if CPU can keep up with emulation

### Two-Layer Architecture

| Layer | Handles | Magnitude | Speed |
|-------|---------|-----------|-------|
| **Rate control** | Clock drift, jitter, oscillator tolerances | ±0.5% | Per-frame |
| **CPU scaling** | Sustained performance gaps | 10-50%+ | Per-second |

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
- [x] On underrun: immediate boost to PERFORMANCE, reset counters

### Phase 4: Special State Handling ✅
- [x] Disable auto-scaling during `fast_forward`
- [x] Disable auto-scaling during `show_menu`
- [x] Ignore first ~1 second after game start (`AUTO_CPU_STARTUP_GRACE = 60`)
- [x] Returns 0 stress if audio not initialized (safe fallback)

### Phase 5: Menu Integration ✅
- [x] Add "Auto" option to CPU Speed menu (overclock_labels[3])
- [x] Updated option count from 3 to 4
- [x] Updated description to mention Auto mode
- [x] Persists via existing config system (`minarch_cpu_speed`)
- [x] **Debug HUD**: Shows "LxFF%" stacked above FPS line when auto mode active
  - L = current level (0/1/2 = POWERSAVE/NORMAL/PERFORMANCE)
  - FF% = window-averaged buffer fill (0-100%)
  - Uses only bitmap font characters (0-9, x, %)
- [x] **CSV Logging**: Logs every CPU change to `USERDATA_PATH/logs/auto_cpu.csv`
  - Columns: timestamp, path, old_level, new_level, fill, cpu_usage, reason
  - Reason: "boost", "reduce", or "panic"
  - Enables data-driven tuning decisions

### Phase 6: Platform Support
- [ ] Audit platforms - which have working `PLAT_setCPUSpeed`?
- [ ] Implement CPU scaling for rg35xxplus (currently empty stub)
- [ ] Add `PLAT_supportsCPUScaling()` or similar for runtime detection

## Threading Architecture

Auto CPU scaling uses a **two-thread design** to keep the main emulation loop responsive:

### Main Thread (Emulation)
- Monitors audio buffer fill every frame
- Accumulates fill % over windows (~500ms)
- Decides when CPU level should change
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

### Buffer Fill Monitoring

The audio buffer fill level directly measures system performance:
- **High fill (>60%)**: CPU is keeping up, buffer has plenty of headroom
- **Low fill (<25%)**: CPU is struggling, buffer is draining toward underrun
- **Sweet spot (25-60%)**: Healthy operation, no action needed

```c
unsigned SND_getBufferOccupancy(void) {
    // Returns buffer fill level as 0-100%
    return (unsigned)(SND_getBufferFillLevel() * 100.0f);
}
```

### Window-Based Scaling

```c
// Every frame: accumulate buffer fill
fill_sum += SND_getBufferOccupancy();
frame_count++;

// Every window (~500ms = 30 frames at 60fps)
if (frame_count >= WINDOW_FRAMES) {
    unsigned avg_fill = fill_sum / frame_count;

    if (avg_fill < 25) {
        // Buffer low - CPU struggling
        low_fill_windows++;
        high_fill_windows = 0;
    } else if (avg_fill > 60) {
        // Buffer healthy - CPU has headroom
        high_fill_windows++;
        low_fill_windows = 0;
    } else {
        low_fill_windows = 0;
        high_fill_windows = 0;
    }

    // Asymmetric: fast to boost, slow to reduce
    if (low_fill_windows >= 2 && cpu_level < MAX_LEVEL) {
        cpu_level++;
        apply_cpu_level();
        low_fill_windows = 0;
    }
    if (high_fill_windows >= 4 && cpu_level > MIN_LEVEL) {
        cpu_level--;
        apply_cpu_level();
        high_fill_windows = 0;
    }

    fill_sum = 0;
    frame_count = 0;
}
```

### Panic Path

```c
// In audio callback, when underrun detected:
if (underrun_occurred) {
    underrun_count++;
}

// In main loop:
if (SND_getUnderrunCount() > last_underrun_count) {
    // Emergency! Immediate boost
    cpu_level = MAX_LEVEL;
    apply_cpu_level();
    low_fill_windows = 0;
    high_fill_windows = 0;
}
```

## Platform CPU Speed Support

### Benchmarked Platforms

| Platform | Architecture | Available Frequencies (kHz) | Current Config | Status |
|----------|--------------|------------------------------|----------------|--------|
| **tg5040** | Cortex-A53 (64-bit) | 408, 600, **816**, 1008, **1200**, 1416, **1608**, 1800, **2000** | MENU=800→816, POWERSAVE=1200✓, NORMAL=1608✓, PERFORMANCE=2000✓ | ✅ Optimal |
| **zero28** | Cortex-A53 (64-bit) | 408, 600, **816**, 1008, **1200**, 1416, **1608**, **1800** | MENU=800→816, POWERSAVE=816✓, NORMAL=1416✓, PERFORMANCE=1800✓ | ✅ Optimal |
| **miyoomini** | Cortex-A7 (32-bit) | **400**, **600**, 800, 1000, **1100**, **1600** | MENU=504→600, POWERSAVE=1104→1100, NORMAL=1296→**1600!**, PERFORMANCE=1488→1600 | ⚠️ Needs tuning |

**Bold frequencies** = currently configured bands

### Performance Comparison

| Platform | Max Freq | Max Performance | vs. miyoomini | IPC Advantage |
|----------|----------|-----------------|---------------|---------------|
| miyoomini | 1600 MHz | 371k iters | Baseline | - |
| zero28 | 1800 MHz | 448k iters | +20.8% | +7.3% (A53 vs A7) |
| tg5040 | 2000 MHz | 496k iters | +33.7% | +7.3% (A53 vs A7) |

### Critical Issue: miyoomini Configuration

**Hardware limitation confirmed via 50MHz sweep**: miyoomini has a **500 MHz gap** between 1100-1600 MHz. Any request in the range 1150-1600 snaps to maximum (1600 MHz).

**Current problem**:
- NORMAL (1296 kHz) overshoots to 1600 MHz (+23%, should be mid-tier)
- PERFORMANCE (1488 kHz) reaches 1600 MHz (correct max)
- **Both bands are identical** - no differentiation between NORMAL and PERFORMANCE

**Recommended fix**:
```c
CPU_SPEED_MENU        = 600000   // was 504000 → 600 (align to hardware)
CPU_SPEED_POWERSAVE   = 1000000  // was 1104000 → 1100 (use available step, -10% power)
CPU_SPEED_NORMAL      = 1100000  // was 1296000 → 1600! (fix overshoot, -31% power!)
CPU_SPEED_PERFORMANCE = 1600000  // was 1488000 → 1600 (align to hardware)
```

**Impact of fix**:
- Restores proper differentiation between bands (1000/1100/1600 vs current 1100/1600/1600)
- NORMAL power consumption reduced by ~31% (1100 vs 1600 MHz)
- Battery life improvement for moderate gaming workloads

### Other Platforms (Not Benchmarked)

| Platform | Status | Method | Configured Frequencies |
|----------|--------|--------|------------------------|
| my355 | ✅ Working | sysfs | 800/1104/1608/1992 MHz |
| rgb30 | ✅ Working | sysfs | 800/1104/1608/1992 MHz |
| magicmini | ✅ Working | sysfs+GPU | 800/816/1416/2016 MHz |
| trimuismart | ✅ Working | sysfs | 504/1104/1344/1536 MHz |
| rg35xx | ✅ Working | overclock.elf | 504/1104/1296/1488 MHz |
| my282 | ✅ Working | overclock.elf | 576-1512 MHz + core count |
| rg35xxplus | ❌ **Not implemented** | - | H700 supports cpufreq |
| m17 | ❌ Fixed | - | Fixed at 1200 MHz |

## Audio System Tuning (Independent of CPU Scaling)

While audio buffer fill doesn't work for CPU scaling, the audio system itself still needs proper tuning for glitch-free playback.

### Rate Control Pitch Deviation (d)

We use **d = 2%** (defined in `audio_resampler.h` as `SND_RATE_CONTROL_D`):

```c
#define SND_RATE_CONTROL_D 0.02f  // ±2% pitch shift allowed
```

**Why 2% instead of the paper's 0.5%:**

Handheld devices have significant timing mismatches that desktop systems don't:
- Cheap display panels: 59.5-60.5 Hz variance (±0.8%)
- Audio DAC oscillator tolerances: ±0.3%
- Combined worst case: ~1.5-2% total mismatch

**The tradeoff:** We sacrifice pitch accuracy for stable latency.
- Well-calibrated devices (tg5040 @ ~60.5Hz): uses ~0.5% correction, sounds perfect
- Badly-calibrated devices (rg35xxplus @ 59.7Hz): uses ~1.5% correction, minor pitch shift
- Alternative: 200ms+ buffer with perfect pitch but bad input lag

**Single source of truth:** Both `SND_calculateRateAdjust()` (api.c) and `AudioResampler_resample()` (audio_resampler.c) reference the same constant. Initially these were out of sync (0.5% vs 1% hardcoded cap), which prevented full compensation.

### Audio Buffer Size

Our audio buffer holds **8 video frames** of audio (~133ms at 60fps, ~160ms at 50fps):

```c
snd.buffer_video_frames = 8;
snd.frame_count = snd.buffer_video_frames * snd.sample_rate_in / snd.frame_rate;
```

**Buffer size history:**
- Started at 5 frames (~83ms) - caused spurious underruns
- Increased to 8 frames - eliminated underruns
- With d=2%, might be able to reduce back to 5 frames (needs testing)

**Effective latency:** Buffer converges to 50% fill, so ~66ms actual latency (half of 133ms).

## Testing Results

### Miyoo Mini Testing (2025-12-02)

**Test 1: NES - Mike Tyson's Punch-Out!!**
- Initial underrun at startup (entering auto from menu)
- Panic boost to PERFORMANCE (1488 MHz) immediately
- Sustained stress: 60-78% throughout gameplay
- **Result**: Correctly stayed at PERFORMANCE - game legitimately needs it
- **Issue**: Startup underrun when transitioning from MENU speed
- **Fix**: Applied NORMAL synchronously before starting thread

**Test 2: Game Boy - Tetris**
- Initial panic boost to PERFORMANCE (startup underrun)
- Stress quickly dropped to 61-65% range
- Then settled at **19-24% sustained** (very relaxed)
- Multiple LOW stress windows but never 4 consecutive
- **Result**: Stuck at PERFORMANCE, unable to reduce
- **Issue**: 20% threshold too low, stress hovers just above it (21-24%)

### Tuning Observations

**Iteration 1 thresholds (too conservative):**
```c
AUTO_CPU_HIGH_THRESHOLD = 0.60   // 60%
AUTO_CPU_LOW_THRESHOLD  = 0.20   // 20%
AUTO_CPU_REDUCE_WINDOWS = 4      // 2 seconds
```

**Problem**: Tetris stress pattern 19-24%
- Occasionally <20% → counts as LOW
- Occasionally >20% → resets to NORMAL, counter resets
- Never achieves 4 consecutive LOW windows

**Iteration 2:**
- Raised LOW_THRESHOLD to 25% - still oscillating

**Iteration 3 (current):**
```c
AUTO_CPU_HIGH_THRESHOLD = 0.70   // Raised to 70% (was 60%)
AUTO_CPU_LOW_THRESHOLD  = 0.25   // Raised to 25% (was 20%)
snd.buffer_video_frames = 8      // Increased from 5 (was causing false underruns)
```

**Key insight from CSV logging:**
- Tetris was panicking (underrun) at 30-40% CPU usage
- CPU wasn't the bottleneck - audio buffer was too small
- Increasing buffer from 5→8 frames eliminated spurious underruns
- Now games reduce smoothly: 2→1→0 without panic oscillation

### Known Issues

1. ~~**Startup underrun**~~ - Fixed: apply initial NORMAL synchronously
2. ~~**Low threshold too conservative**~~ - Fixed: raised to 25%
3. ~~**Debug HUD segfault**~~ - Fixed: use only bitmap font characters, added bounds checking
4. ~~**False underruns at low CPU usage**~~ - Fixed: increased audio buffer from 5→8 frames

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
- [workspace/all/common/api.c](../workspace/all/common/api.c) - `SND_calculateRateAdjust()`
- [workspace/all/minarch/minarch.c](../workspace/all/minarch/minarch.c) - Main emulation loop
- [workspace/all/paks/Benchmark/](../workspace/all/paks/Benchmark/) - CPU frequency benchmark tool

## Tuning Status

| Parameter | Current | Status | Notes |
|-----------|---------|--------|-------|
| Audio buffer | 8 frames (133ms) | ✅ Fixed | Was 5 frames, caused false underruns |
| Window size | 30 frames (500ms) | ✅ Good | 6 buffer cycles, filters noise well |
| Fill low threshold | 25% | ✅ v4 | Buffer below this = boost CPU |
| Fill high threshold | 60% | ✅ v4 | Buffer above this = can reduce CPU |
| Boost windows | 2 (1s) | ✅ Good | Fast response to performance issues |
| Reduce windows | 4 (2s) | ✅ Good | Conservative to prevent oscillation |
| Startup grace | 60 frames (1s) | ✅ Good | Avoids false positives during buffer fill |

### Current Values (Iteration 4 - Buffer-Based)

```c
// api.c
snd.buffer_video_frames = 8;           // ~133ms at 60fps (was 5)

// minarch.c
#define AUTO_CPU_WINDOW_FRAMES 30      // ~500ms at 60fps
#define AUTO_CPU_FILL_LOW 25           // Buffer below 25% = needs more CPU
#define AUTO_CPU_FILL_HIGH 60          // Buffer above 60% = can save power
#define AUTO_CPU_BOOST_WINDOWS 2       // Boost after 1s of low buffer
#define AUTO_CPU_REDUCE_WINDOWS 4      // Reduce after 2s of high buffer
#define AUTO_CPU_STARTUP_GRACE 60      // Ignore first ~1s after game start
```

Iteration 4 switched from rate control stress to direct buffer fill monitoring. This is simpler
and more intuitive: low buffer = CPU struggling, high buffer = CPU has headroom.
