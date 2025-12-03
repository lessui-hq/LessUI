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
- [x] **Debug HUD**: Shows "CPU:X S:XX%" when auto mode active
  - CPU:0/1/2 = current level (POWERSAVE/NORMAL/PERFORMANCE)
  - S:XX% = real-time rate control stress (0-100%)

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

    if (avg_stress > 0.6f) {
        high_stress_windows++;
        low_stress_windows = 0;
    } else if (avg_stress < 0.2f) {
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

Our audio buffer holds **5 video frames** of audio (~83ms at 60fps, ~100ms at 50fps):

```c
snd.buffer_video_frames = 5;
snd.frame_count = snd.buffer_video_frames * snd.sample_rate_in / snd.frame_rate;
```

This is tight, which is why we use rate control stress (buffer-independent) rather than raw buffer occupancy.

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

**Current thresholds:**
```c
AUTO_CPU_HIGH_THRESHOLD = 0.60   // 60%
AUTO_CPU_LOW_THRESHOLD  = 0.20   // 20%
AUTO_CPU_REDUCE_WINDOWS = 4      // 2 seconds
```

**Problem**: Tetris stress pattern 19-24%
- Occasionally <20% → counts as LOW
- Occasionally >20% → resets to NORMAL, counter resets
- Never achieves 4 consecutive LOW windows
- Could run at POWERSAVE (1104 MHz) but stays at PERFORMANCE (1488 MHz)

**Proposed fix:**
```c
AUTO_CPU_LOW_THRESHOLD  = 0.25   // Raise to 25% (was 20%)
AUTO_CPU_REDUCE_WINDOWS = 3      // Reduce to 3 windows = 1.5s (was 4 = 2s)
```

**Reasoning:**
- 25% threshold catches 19-24% stress range consistently
- Still maintains 35% deadband from high threshold (25% to 60%)
- Faster reduction (1.5s) for better battery savings on easy games
- More aggressive power savings without sacrificing responsiveness

**Risk**: May cause oscillation if a game needs exactly NORMAL level. Need more testing.

### Known Issues

1. **Startup underrun** - Fixed by applying initial NORMAL synchronously
2. **Low threshold too conservative** - Prevents reduction on easy games
3. **Debug HUD segfault** - Needs investigation (boundary issue in blitBitmapText)

## References

- [Dynamic Rate Control for Retro Game Emulators](https://docs.libretro.com/guides/ratecontrol.pdf) - Hans-Kristian Arntzen, 2012
- [docs/audio-rate-control.md](audio-rate-control.md) - Our rate control implementation
- [workspace/all/common/api.c](../workspace/all/common/api.c) - `SND_calculateRateAdjust()`
- [workspace/all/minarch/minarch.c](../workspace/all/minarch/minarch.c) - Main emulation loop

## Tuning Status

| Parameter | Current | Status | Notes |
|-----------|---------|--------|-------|
| Window size | 30 frames (500ms) | ✅ Good | 6 buffer cycles, filters noise well |
| High threshold | 60% | ✅ Good | Catches struggling games before crisis |
| Low threshold | 20% | ⚠️ **Too low** | Tetris hovers 19-24%, can't reduce. Try 25% |
| Boost windows | 2 (1s) | ✅ Good | Fast response to performance issues |
| Reduce windows | 4 (2s) | ⚠️ **Too slow?** | Combined with low threshold, prevents reduction. Try 3 |
| Startup grace | 60 frames (1s) | ✅ Good | Avoids false positives during buffer fill |

### Recommended Next Iteration

```c
#define AUTO_CPU_LOW_THRESHOLD 0.25f   // Raise from 0.20 to 0.25
#define AUTO_CPU_REDUCE_WINDOWS 3      // Reduce from 4 to 3 (1.5s)
```

This should allow Tetris-class games to reduce to POWERSAVE while maintaining stability.
