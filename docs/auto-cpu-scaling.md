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
- Rate control (d=2%) fights deficit that shouldn't exist

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

**The math:** With d=1% and 0.65% mismatch, equilibrium is:
```
adjustment = 1.0 - (1.0 - 2*fill) * 0.01 = 0.9935
fill = 0.175 (17.5%)
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
    // Emergency! Immediate boost to PERFORMANCE
    cpu_level = MAX_LEVEL;
    apply_cpu_level();
    high_util_windows = 0;
    low_util_windows = 0;
}
```

### Why Frame Timing Instead of Buffer Fill

| Metric | Buffer Fill | Frame Timing |
|--------|-------------|--------------|
| **Measures** | Audio/display timing sync | Actual CPU work |
| **Problem** | Contaminated by timing mismatches | Direct measurement |
| **rg35xxplus 59.7Hz** | Triggers boost at 50% CPU | Correctly shows ~50% util |
| **PAL games** | Different buffer dynamics | Auto-adjusts via core.fps |
| **Loading screens** | Can trigger false boosts | Filtered by 90th percentile |

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

## Audio System Tuning

### Rate Control Pitch Deviation (d) - Critical Understanding

The **d parameter is a single value** that determines BOTH:
1. **Maximum mismatch capacity** - How much display/core rate difference the system can handle
2. **Response aggressiveness** - How quickly the system adjusts to buffer changes

These are not separate concerns - they're the same dial.

**Arntzen's paper recommends d = 0.2-0.5% for desktop systems** because:
- Desktop monitors: 59.94-60.00 Hz (very stable, <0.5% variance)
- Good oscillator quality
- Only needs to handle timing jitter

**Handheld devices are different**:
- Cheap LCD panels: 57-62 Hz (measured variance up to 4%)
- Display/core mismatches of 0.65-4% are common
- Need larger d to handle structural mismatch, not just jitter

**Current implementation:**

```c
// Rate control sensitivity (api.c)
#define SND_RATE_CONTROL_D 0.005f  // ±0.5% pitch adjustment for jitter

// Resampler safety clamp (audio_resampler.h) - CRITICAL: Must be separate!
#define SND_RESAMPLER_MAX_DEVIATION 0.05f  // ±5% absolute maximum
```

**Why we were confused (critical mistake):**

Originally had both using the same constant:
- `SND_RATE_CONTROL_D = 0.01f` (1%)
- Resampler clamped to `±SND_RATE_CONTROL_D` (same 1%)

When we tried display correction (e.g., 0.9935 for -0.65% mismatch), the combined value `rate_adjust * display_correction` could exceed 1%, hitting the resampler clamp. We kept changing d but the clamp prevented it from working.

**The fix:** Separate the constants:
- Rate control d = 0.5% (Arntzen algorithm parameter)
- Resampler clamp = 5% (safety limit, must be larger to allow corrections)

Now display correction can work within the 5% safety envelope.

**Key insight from testing:**

When display/core mismatch (e.g., 59.71 vs 60.10 = 0.65%) exceeds d (0.5%), the buffer cannot converge to 50% without additional correction.

**Solution approaches explored:**

1. **Display rate correction** (complex, working) - Measure display rate, apply `correction = display_fps / core_fps`
2. **Increase d** (simple, not tested) - Set d=1-2% to naturally handle mismatch (requires clamp ≥ d + display_correction)

Current implementation uses approach #1 (display correction) but approach #2 may be simpler.

### Audio Buffer Size

Our audio buffer holds **6 video frames** of audio (~100ms at 60fps):

```c
snd.buffer_video_frames = 6;
snd.frame_count = snd.buffer_video_frames * snd.sample_rate_in / snd.frame_rate;
```

Buffer fill equilibrium depends on display/core rate mismatch when vsync throttles the loop. Actual latency is ~33-50ms as buffer doesn't stay at 50% on devices with timing mismatches.

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

| Parameter | Current | Notes |
|-----------|---------|-------|
| Rate control d | 0.5% (with display correction) | Paper recommends 0.2-0.5% for desktop |
| Resampler safety clamp | 5% | Prevents extreme pitch shifts from bugs |
| Audio buffer | 6 frames (~100ms) | Effective latency ~50ms at equilibrium |
| Window size | 30 frames (~500ms) | Filters noise, responsive to changes |
| Utilization high | 85% | Frame time >85% of budget = boost |
| Utilization low | 55% | Frame time <55% of budget = reduce |
| Boost windows | 2 (~1s) | Fast response to performance issues |
| Reduce windows | 4 (~2s) | Conservative to prevent oscillation |
| Startup grace | 60 frames (~1s) | Avoids false positives during warmup |
| Percentile | 90th | Ignores outliers (loading screens) |

### Measured Display Timings (2025-12-04)

| Platform | Measured Hz | Core expects | Mismatch | Solution |
|----------|-------------|--------------|----------|----------|
| rg35xxplus | 59.71 Hz | 60.10 Hz (NES) | -0.65% | Display correction (0.9935x) |

**The fundamental problem:** Main loop runs at display rate (vsync-throttled) but audio system expects core rate, creating structural production deficit.

**Current solution:** Display rate correction factor applied to resampler ratio_adjust:
- Measures stable display rate via vsync timing (requires 3 consistent readings within 0.5 Hz)
- Applies `correction = display_fps / core_fps` (e.g., 59.71/60.10 = 0.9935)
- Combined with rate control: `corrected_adjust = rate_adjust * display_correction`
- Buffer converges to 50% as designed

**Alternative approach (simpler, not yet tested):**
- Remove display correction entirely
- Increase `SND_RATE_CONTROL_D` from 0.5% to 1-2%
- Rate control naturally handles mismatch if d ≥ mismatch magnitude
- Simpler code, same result for devices with consistent display rates

### Debug HUD

When Auto CPU mode is enabled, the debug overlay shows:
```
0x 62/27
```
- `0x` = Current CPU level (0=POWERSAVE, 1=NORMAL, 2=PERFORMANCE)
- `62` = Frame timing utilization (90th percentile, % of frame budget)
- `27` = Audio buffer fill (% full)

Example: On rg35xxplus running Punch-Out at POWERSAVE, 62% utilization means core.run() uses 62% of the 16.6ms frame budget. The 27% buffer fill is expected due to the -0.65% display/core rate mismatch.

### CSV Logging

CPU scaling events are logged to `USERDATA_PATH/logs/auto_cpu.csv`:
```csv
timestamp,path,old_level,new_level,util_p90,fill,reason
1234567,game.nes,1,2,87,25,boost
```
