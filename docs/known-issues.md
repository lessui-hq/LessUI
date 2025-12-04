# Known Issues

Issues discovered during development and testing of LessUI.

## Audio Rate Control

### Buffer Now Settling Near 50% ✓

**Status:** Resolved with audio clock correction
**Severity:** N/A (fixed)
**Affected:** All platforms

**Summary:**

We implemented **audio clock correction** to complement the existing display clock correction. The buffer now settles near 50% on all tested platforms.

**The Problem (before fix):**

The buffer was settling at ~65% instead of 50% because we only corrected for display/core mismatch, not audio clock drift. SDL's actual consumption rate differs from the nominal 48,000 Hz.

**The Solution:**

Added `audio_correction` factor to the rate control formula:

```c
// Before:
total_adjust = base_correction × rate_adjust

// After:
total_adjust = base_correction × audio_correction × rate_adjust

// Where:
base_correction = display_hz / core_fps        // display clock drift
audio_correction = sample_rate_out / audio_hz  // audio clock drift
rate_adjust = 1.0 ± d                          // dynamic jitter compensation
```

**Implementation:**

1. `measureAudioRate()` tracks `samples_requested` over 2-second windows
2. After 5 samples, applies median to `SND_setAudioRate()`
3. `audio_correction` factor applied in `SND_batchSamples()`

**Results (2025-12-04):**

| Platform | Audio Hz | audio_correction | Fill (before) | Fill (after) |
|----------|----------|------------------|---------------|--------------|
| rg35xxplus | ~48000 | 1.0000 | ~65% | ~55% |
| miyoomini | ~47896 | 1.0022 | ~65% | ~52% |

**Diagnostic Logging:**

```
Audio: fill=52% balance=+16 dt=2.011s
  rates: in=47649 out=48002 request=48000 consume=47855 (expect=48000)
  ratio: actual=1.007408 expected=1.007392 diff=+0.0016% (window_adj=0.992662)
  resamp: frac_pos=32440 (49.5%) step=65536/64917 (base/adj)
  rctrl: d=1.0% base=0.9927 audio=1.0022 rate=1.0006 total=0.995401
  vsync: measured=59.66Hz current=59.65Hz(±4.97) core=60.10Hz
  audio: measured=47896Hz current=48363Hz nominal=48000Hz
  cpu: p90=10900us/16638us (65%) level=2
```

---

### Miyoo Mini - High Audio Callback Jitter

**Status:** Known, hardware limitation
**Severity:** Low (functional, just oscillates more)
**Affected:** miyoomini

**Symptoms:**

Buffer fill oscillates widely (38-75%) even though average is ~52%:
```
fill=72% → 39% → 48% → 58% → 51% → 61% → 38% → 48% → 57%
```

**Root Cause:**

miyoomini's audio callback timing is extremely jittery:
- Normal request: ~47855 samples/window
- Burst request: 48364-48469 samples/window (+1% spike)
- Spread: ~600-800 Hz (vs ~200 Hz on rg35xxplus)

When SDL bursts, it drains the buffer faster than rate control can respond.

**Evidence:**
```
[22:04:09] fill=72% balance=+297
[22:04:11] fill=39% balance=-1079 dt=2.028s  ← dropped 33% in one window!
           rates: in=47255 request=48469     ← SDL requested a burst
```

**Why it doesn't underrun:**

The ~52% average fill provides enough headroom. Even swings to 38% don't hit empty.

**Possible mitigations (not implemented):**
- Increase buffer size from 4 to 5-6 frames (+16ms latency)
- Increase d parameter for faster response
- Platform-specific tuning

**Current decision:** Accept as hardware limitation. Audio is stable, just oscillates more than other platforms.

---

### Startup Underruns During Buffer Priming

**Status:** Known, minor
**Severity:** Low
**Affected:** All platforms

**Symptoms:**
- 60-90 audio underruns in the first 1-2 seconds of gameplay
- Buffer not yet primed when audio callback starts
- Causes brief audio glitches at startup

**Evidence:**
```
[WARN] Audio: 78 underrun(s) in last second
```

**Current decision:** Accept as minor issue. Brief startup glitch is acceptable trade-off for 4-frame (~67ms) low latency.

---

## Auto CPU Scaling

### PANIC Triggers on Menu Transitions

**Status:** Known, acceptable
**Severity:** Low
**Affected:** All platforms

**Symptoms:**
- Auto CPU PANIC path triggers when entering auto mode from menu
- Happens during state transitions (menu close → gameplay)
- Causes brief spike to PERFORMANCE, then reduces back down

**Evidence:**
```
[DEBUG] Auto CPU: grace period complete, monitoring active
[WARN] Auto CPU: PANIC - underrun detected, boosting to PERFORMANCE
[INFO] Auto CPU: applying PERFORMANCE (level 2)
[DEBUG] Auto CPU: p90=6672us/16638us (40%) level=2
[INFO] Auto CPU: REDUCE requested, level 1 (util=40%, 4 windows)
```

**Root cause:**
- 60-frame grace period helps but doesn't eliminate transient underruns
- Menu transition causes irregular frame timing
- Auto CPU responds correctly by boosting, then reducing

**Current decision:** Acceptable edge case. System recovers quickly and settles at correct level.

---

## Platform-Specific Issues

### Miyoo Mini - High CPU Usage Mystery

**Status:** Unresolved
**Severity:** Medium
**Affected:** miyoomini only

**Symptoms from earlier logs:**
- Frame timing util: 64-68% (emulation work)
- System CPU usage: 76-99% (total)
- Gap of 15-30% unaccounted for
- Prevents auto CPU from reducing below PERFORMANCE

**Hypothesis:**
- Audio callback overhead?
- SDL/MI_SYS overhead?
- Background processes?
- Inefficient vsync implementation?

**Investigation needed:**
- Profile where the extra CPU is going
- Compare with other A7 platforms (trimuismart)
- Check if MI_SYS calls are blocking/expensive

---

## Resolved Issues

### Resampler Accuracy ✓

**Status:** Verified correct
**Resolution:** No fix needed

The resampler was suspected of systematic bias, but logging proved it's accurate:
- `diff` between actual and expected ratio: ±0.002% (essentially zero)
- `frac_pos` varies randomly (no drift)
- Window-averaged comparisons match within 0.002%

### Display Rate Measurement ✓

**Status:** Working correctly
**Resolution:** Implemented adaptive d + stability detection

- 30-sample median with 1.0 Hz stability threshold
- Adaptive d: 2% during measurement, 1% after
- Correctly measures display rates on all tested platforms

---

## Document Updates Needed

- [ ] Update main README.md with audio rate control summary
- [ ] Update docs/audio-rate-control.md with audio_correction details
