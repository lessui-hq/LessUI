# TODO: Outstanding Issues

## Audio/Video Sync on tg5050 (Trimui Smart Pro)

**Status:** Root cause identified, solution designed, implementation pending
**Priority:** High (severely degraded user experience)
**Platform:** tg5050 (tg5040 works fine with cubic boost)

### Problem Summary

tg5050 experiences severe audio buffer issues (2649 overruns + 103 underruns in single session) due to extreme display refresh rate mismatch that current vsync-driven timing cannot handle.

### Root Cause: Dynamic Refresh Rate Mismatch

**Discovered via vsync measurement (implemented 2026-01-06):**

SDL reports: 60Hz (integer, via `SDL_GetCurrentDisplayMode()`)
**Actual measured behavior:**
- Starts at: 65-68Hz (14% faster than games!)
- Drifts to: 62-63Hz over 30 seconds
- Continues oscillating: ±1Hz throughout session

**Example from GB.log:**
```
[03:45:35] Measured: 68.510Hz (14.18% above SDL's 60Hz)
[03:45:38] Drift to: 63.860Hz (dropped 6.79%)
[03:45:43] Drift to: 62.991Hz (dropped 1.36%)
[03:46:23] Drift to: 61.526Hz (dropped 1.86%)
[03:46:28] Drift to: 62.873Hz (rose 2.19%)
```

The screen refresh rate is **both wrong and unstable**.

### Why Current Approaches Fail

**Frame pacing (Bresenham) fails:**
- At 14% mismatch (68Hz screen vs 60fps game), would skip 13% of frames
- Human perception threshold for stutter: 2-3%
- 13% frame skipping = very visible stuttering
- Dynamic drift (62→68Hz) requires constant re-adjustment
- Result: 2649 buffer overruns despite paced mode

**Audio rate control fails:**
- PI controller tuned for 68Hz overshoots when screen drops to 62Hz
- Integral term chases a moving target
- Frame pacing + audio rate control fight each other (one skips frames, other adjusts pitch)
- Result: Oscillates between overrun (100% full) and underrun (0% empty)

**Fundamental issue:** Vsync-driven timing assumes:
1. Display Hz ≈ game Hz (within 1-2%) ❌ tg5050: 14%
2. Display Hz is stable ❌ tg5050: drifts 6Hz
3. Audio pitch adjustment can compensate ❌ 14% is too much

### Expert Analysis (Subject Matter Expert Agent, 2026-01-06)

> **"The approach is fundamentally problematic for this hardware."**
>
> At 14% mismatch with dynamic drift, vsync-driven timing cannot work. The correct architecture is **audio-clock master timing** with vsync used only for tear-free presentation, not timing.

**Key insight:**
- **Frame pacing (dropping frames):** Game *jumps* forward periodically → visible stutter
- **Audio-clock (duplicating frames):** Some frames *held slightly longer* → barely perceptible

At 68Hz screen / 60fps game:
- Frame pacing: Skip every 8th frame → visible stutter
- Audio-clock: Show some frames twice → much less noticeable

### Solution: Auto Sync Mode Selection

Implement hybrid sync strategy based on measured Hz mismatch:

```c
if (|measured_hz - game_fps| < 1%)  → VSYNC_DIRECT  // Current default
if (|measured_hz - game_fps| < 5%)  → VSYNC_PACED   // Current fallback
if (|measured_hz - game_fps| > 5%)  → AUDIO_MASTER  // NEW for tg5050
```

**Audio-clock master mode:**
- Audio hardware is master clock (runs core at exact 60fps)
- Vsync only prevents tearing (not for timing)
- Frame duplication instead of frame skipping
- No audio rate control needed (buffer naturally stable)
- Already implemented: `workspace/all/player/player_loop_audioclock.inc`

### Implementation Status

**✅ Completed (2026-01-06):**
1. Vsync measurement system
   - Measures actual Hz by timing `SDL_RenderPresent()` calls
   - Uses exponential moving average (α=0.01) for stability
   - Rejects outliers outside 50-120Hz range
   - Stable after 120 samples (~2 seconds)

2. Continuous drift tracking
   - Re-checks every 300 frames (~5 seconds)
   - Logs Hz changes >0.1%
   - Resets Bresenham accumulator on Hz change (prevents glitches)

3. Tighter tolerance
   - Changed `FRAME_PACER_TOLERANCE` from 2% to 1%
   - Based on RetroArch paper: audio-only rate control works up to ~0.5%

4. Removed `FORCE_FRAME_PACING`
   - Was testing hack, didn't solve root cause
   - Removed from tg5040/tg5050 platform.h

**📋 TODO:**
1. Add sync mode selection logic in frame pacer
   - Detect when measured Hz > 5% from game fps
   - Switch to audio-clock mode automatically
   - Keep vsync measurement running for dynamic detection

2. Disable audio rate control in audio-clock mode
   - Buffer naturally stable when audio drives timing
   - Rate control fights audio-clock (causes oscillation)

3. Test on tg5050
   - Verify audio-clock eliminates buffer warnings
   - Confirm frame duplication is imperceptible
   - Measure actual improvement vs current state

4. Consider per-platform override
   - Add `SYNC_MODE_AUDIO_CLOCK` define for platforms that need it
   - tg5050 might always need audio-clock regardless of game

### Files Modified (2026-01-06)

**Vsync measurement implementation:**
- `workspace/all/player/frame_pacer.h` - Added measurement state, functions, constants
- `workspace/all/player/frame_pacer.c` - Implemented measurement + drift tracking
- `workspace/all/player/player_loop_vsync.inc` - Calls `FramePacer_recordVsync()`
- `tests/unit/all/player/test_frame_pacer.c` - Added 6 tests for measurement

**Cleanup:**
- `workspace/tg5040/platform/platform.h` - Removed FORCE_FRAME_PACING
- `workspace/tg5050/platform/platform.h` - Removed FORCE_FRAME_PACING
- `workspace/all/common/cpu.c` - Fixed unused variable lint error

### Related Context

**tg5040 status:** ✅ Working well with cubic boost
- Mean fill: 40.5% (healthy)
- Buffer warnings: 704 (acceptable)
- System stable

**Why tg5040 works but tg5050 doesn't:**
- Both have refresh rate mismatch, but tg5050's is more extreme
- tg5050's dynamic drift is more pronounced
- tg5040 likely closer to 60Hz or more stable

### References

**RetroArch Dynamic Rate Control (Arntzen, 2012):**
- Recommends d = 0.2-0.5% for audio pitch adjustment
- States frame pacing "only useful when game frame rate is **close** to monitor frame rate"
- At >0.5% mismatch: "other methods should be employed"

**Expert recommendation (SME agent a77771d):**
- Primary: Audio-clock master for |mismatch| > 5%
- Fallback: Hybrid sync mode selection
- Never: Frame pacing at >5% mismatch (causes perceptible stutter)

### Test Logs Evidence

**Before vsync measurement (old behavior):**
- System thinks 60Hz, uses direct vsync
- Result: 8000+ buffer overrun warnings

**With vsync measurement + frame pacing (tested 2026-01-06):**
- Detects 68Hz, switches to paced mode
- Tracks drift: 68→63Hz over time
- Result: Still 2649 overruns + 103 underruns (frame pacing can't handle 14%)

**Expected with audio-clock mode:**
- Audio drives timing at exact game fps
- Vsync just presents frames (duplicating when needed)
- Result: Zero buffer warnings (audio naturally stable)

### Open Questions

1. Does tg5040 also have dynamic refresh rate? (Not tested yet)
2. Should we keep vsync mode as default and auto-switch, or force audio-clock for some platforms?
3. Do other devices in the lineup have similar issues?
4. Does audio-clock mode affect input latency measurably?

### Next Steps

1. **Implement auto sync mode selection** - Priority: High
   - Use measured Hz to pick VSYNC vs AUDIO_CLOCK
   - Test threshold value (currently thinking 5%)

2. **Test on tg5050** - Priority: High
   - Verify it solves the buffer issue
   - Measure subjective quality (frame duplication vs stutter)

3. **Measure other devices** - Priority: Medium
   - Run vsync measurement on all supported platforms
   - Identify which need audio-clock mode

4. **Document behavior** - Priority: Low
   - Add platform notes about sync modes
   - Explain why some devices use audio-clock
