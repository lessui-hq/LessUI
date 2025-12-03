# Auto CPU Scaling

Dynamic CPU frequency scaling for libretro emulation based on audio rate control stress.

**GitHub Issue**: [#44 - Add Auto CPU Scaling Option](https://github.com/nchapman/LessUI/issues/44)

## Overview

Add an "Auto" CPU speed option that dynamically scales between existing power levels (POWERSAVE/NORMAL/PERFORMANCE) based on real-time emulation performance, saving battery when possible and boosting when needed.

## Design Approach

### Why Audio Rate Control Stress?

We already implement [Hans-Kristian Arntzen's Dynamic Rate Control](https://docs.libretro.com/guides/ratecontrol.pdf) for audio/video sync. The rate control adjustment factor directly measures system stress:

```c
adjustment = 1 + (1 - 2*fill) * d   // d = 0.005 (max ±0.5%)
```

| Buffer Fill | Adjustment | Stress | Meaning |
|-------------|------------|--------|---------|
| 50% | 1.000 | 0% | Equilibrium |
| 25% | 1.0025 | 50% | Working harder |
| 10% | 1.004 | 80% | Struggling |
| 0% | 1.005 | 100% | Maxed out |

**Key insight**: If rate control is consistently near its ±0.5% limit, it can't compensate anymore - that's when CPU scaling should intervene.

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

**vs. Raw buffer occupancy**:
- Buffer-size independent
- Works regardless of buffer_video_frames setting
- Normalized 0-1 metric

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
- [x] **Debug HUD**: Shows "XxYY%" stacked above FPS line when auto mode active
  - X = current level (0/1/2 = POWERSAVE/NORMAL/PERFORMANCE)
  - YY% = window-averaged stress (0-100%)
  - Uses only bitmap font characters (0-9, x, %)
- [x] **CSV Logging**: Logs every CPU change to `USERDATA_PATH/logs/auto_cpu.csv`
  - Columns: timestamp, path, old_level, new_level, stress, cpu_usage, reason
  - Reason: "boost", "reduce", or "panic"
  - Enables data-driven tuning decisions

### Phase 6: Platform Support
- [ ] Audit platforms - which have working `PLAT_setCPUSpeed`?
- [ ] Implement CPU scaling for rg35xxplus (currently empty stub)
- [ ] Add `PLAT_supportsCPUScaling()` or similar for runtime detection

## Threading Architecture

Auto CPU scaling uses a **two-thread design** to keep the main emulation loop responsive:

### Main Thread (Emulation)
- Monitors rate control stress every frame
- Accumulates stress over windows (~500ms)
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

### Stress Calculation

```c
float SND_getRateControlStress(void) {
    // Last rate adjustment from SND_calculateRateAdjust()
    float adjust = last_rate_adjust;

    // Normalize: 1.0 = equilibrium, 1.005 = max boost needed
    // stress = 0.0 (happy) to 1.0 (maxed out)
    float stress = (adjust - 1.0f) / d;
    return fmaxf(0.0f, fminf(1.0f, stress));
}
```

### Window-Based Scaling

```c
// Every frame: accumulate stress
stress_sum += SND_getRateControlStress();
frame_count++;

// Every window (~500ms = 30 frames at 60fps)
if (frame_count >= WINDOW_FRAMES) {
    float avg_stress = stress_sum / frame_count;

    if (avg_stress > 0.7f) {
        high_stress_windows++;
        low_stress_windows = 0;
    } else if (avg_stress < 0.25f) {
        low_stress_windows++;
        high_stress_windows = 0;
    } else {
        high_stress_windows = 0;
        low_stress_windows = 0;
    }

    // Asymmetric: fast to boost, slow to reduce
    if (high_stress_windows >= 2 && cpu_level < MAX_LEVEL) {
        cpu_level++;
        apply_cpu_level();
        high_stress_windows = 0;
    }
    if (low_stress_windows >= 4 && cpu_level > MIN_LEVEL) {
        cpu_level--;
        apply_cpu_level();
        low_stress_windows = 0;
    }

    stress_sum = 0;
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
    high_stress_windows = 0;
    low_stress_windows = 0;
}
```

## Platform CPU Speed Support

| Platform | Status | Method | Frequencies |
|----------|--------|--------|-------------|
| tg5040 | ✅ Working | sysfs | 800/1200/1608/2000 MHz |
| my355 | ✅ Working | sysfs | 800/1104/1608/1992 MHz |
| rgb30 | ✅ Working | sysfs | 800/1104/1608/1992 MHz |
| zero28 | ✅ Working | sysfs | 800/816/1416/1800 MHz |
| magicmini | ✅ Working | sysfs+GPU | 800/816/1416/2016 MHz |
| miyoomini | ✅ Working | overclock.elf | 504/1104/1296/1488 MHz |
| trimuismart | ✅ Working | sysfs | 504/1104/1344/1536 MHz |
| rg35xx | ✅ Working | overclock.elf | 504/1104/1296/1488 MHz |
| my282 | ✅ Working | overclock.elf | 576-1512 MHz + core count |
| rg35xxplus | ❌ **Not implemented** | - | H700 supports cpufreq |
| m17 | ❌ Fixed | - | Fixed at 1200 MHz |

## Audio Buffer Context

Our audio buffer holds **8 video frames** of audio (~133ms at 60fps, ~160ms at 50fps):

```c
snd.buffer_video_frames = 8;
snd.frame_count = snd.buffer_video_frames * snd.sample_rate_in / snd.frame_rate;
```

**Buffer size history:**
- Started at 5 frames (~83ms) - caused spurious underruns at low CPU usage
- CSV logging revealed underruns happening at 30-40% CPU usage (not CPU-bound)
- Increased to 8 frames - eliminated false underruns, stable scaling

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

## References

- [Dynamic Rate Control for Retro Game Emulators](https://docs.libretro.com/guides/ratecontrol.pdf) - Hans-Kristian Arntzen, 2012
- [docs/audio-rate-control.md](audio-rate-control.md) - Our rate control implementation
- [workspace/all/common/api.c](../workspace/all/common/api.c) - `SND_calculateRateAdjust()`
- [workspace/all/minarch/minarch.c](../workspace/all/minarch/minarch.c) - Main emulation loop

## Tuning Status

| Parameter | Current | Status | Notes |
|-----------|---------|--------|-------|
| Audio buffer | 8 frames (133ms) | ✅ Fixed | Was 5 frames, caused false underruns |
| Window size | 30 frames (500ms) | ✅ Good | 6 buffer cycles, filters noise well |
| High threshold | 70% | ✅ Adjusted | Was 60%, raised for more headroom |
| Low threshold | 25% | ✅ Adjusted | Was 20%, raised to catch 19-24% stress games |
| Boost windows | 2 (1s) | ✅ Good | Fast response to performance issues |
| Reduce windows | 4 (2s) | ✅ Good | Conservative to prevent oscillation |
| Startup grace | 60 frames (1s) | ✅ Good | Avoids false positives during buffer fill |

### Current Values (Iteration 3)

```c
// api.c
snd.buffer_video_frames = 8;           // ~133ms at 60fps (was 5)

// minarch.c
#define AUTO_CPU_WINDOW_FRAMES 30      // ~500ms at 60fps
#define AUTO_CPU_HIGH_THRESHOLD 0.7f   // Stress above 70% = needs more CPU
#define AUTO_CPU_LOW_THRESHOLD 0.25f   // Stress below 25% = can save power
#define AUTO_CPU_BOOST_WINDOWS 2       // Boost after 1s of high stress
#define AUTO_CPU_REDUCE_WINDOWS 4      // Reduce after 2s of low stress
#define AUTO_CPU_STARTUP_GRACE 60      // Ignore first ~1s after game start
```

Tetris now reduces smoothly to POWERSAVE without oscillation. CSV logging confirmed the fix.
