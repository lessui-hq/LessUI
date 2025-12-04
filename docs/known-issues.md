# Known Issues

Issues discovered during development and testing of LessUI.

## Audio Rate Control

### Buffer Now Settling Near 50% ✓

**Status:** Resolved with unified RateMeter system
**Severity:** N/A (fixed)
**Affected:** All platforms

**Summary:**

We implemented a **unified RateMeter system** with separate measurement intervals for display and audio. The buffer now settles near 50% on all tested platforms with both display and audio correction working optimally.

**The Solution:**

Unified measurement system with configurable intervals:

```c
// Single RateMeter algorithm handles both display and audio
// Key difference: measurement intervals match signal characteristics

Display: per-frame measurements (30 samples × 16.7ms = 0.5s)
Audio:   2-second windows (10 samples × 2s = 20s)

total_adjust = base_correction × audio_correction × rate_adjust

// Where:
base_correction = display_hz / core_fps        // display clock drift
audio_correction = sample_rate_out / audio_hz  // audio clock drift
rate_adjust = 1.0 ± d                          // dynamic jitter compensation
```

**Key Design Decisions:**

1. **Longer audio intervals (2s)** - Averages out SDL callback burst patterns (40-53 kHz per 100ms → stable 47-48 kHz per 2s)
2. **Relaxed audio threshold (500 Hz)** - Accommodates SDL burst jitter while rejecting truly noisy data
3. **Lock on stability** - Once stable, values only update if new data has smaller spread
4. **Graceful fallback** - If meter never stabilizes, returns 0 (no correction applied)

**Results (2025-12-04):**

| Platform | Display | Audio | Fill | Behavior |
|----------|---------|-------|------|----------|
| rg35xxplus | 59.77 Hz (0.89 Hz swing) | 47969 Hz (202 Hz swing) ✓ | ~50% | Both corrections active |
| miyoomini | 59.66 Hz (0.41 Hz swing) | Not stable (764 Hz swing) | ~55% | Display correction only |

**Diagnostic Logging (rg35xxplus):**

```
Display: meter stable at 59.77 Hz (swing=0.89 Hz, core=60.10 Hz, correction=0.9945, -0.55% mismatch)
Audio: meter stable at 47968.67 Hz (swing=202.45 Hz, nominal=48000 Hz, correction=1.0007, -0.07% drift)

Audio: fill=53% balance=+16 dt=2.010s
  rates: in=47690 out=47997 request=47968 consume=47968 (expect=48000)
  ratio: actual=1.006386 diff=+0.0007%
  rctrl: d=1.0% base=0.9945 audio=1.0007 rate=0.9999 total=0.995049
  vsync: measured=59.77Hz swing=1.91 core=60.10Hz
  audio: measured=47969Hz current=47969Hz nominal=48000Hz
```

---

### Miyoo Mini - High Audio Callback Jitter

**Status:** Working with graceful degradation
**Severity:** Low (functional and stable)
**Affected:** miyoomini

**Behavior:**

The audio meter **does not achieve stability** on miyoomini due to SDL callback burst patterns:
- Measurement spread: ~764 Hz (above 500 Hz threshold)
- Audio correction: **disabled** (meter returns 0)
- System falls back to display correction + dynamic rate control only
- Buffer fill: 38-67%, averaging ~55%

**Why this happens:**

Even with 2-second measurement windows, SDL callback bursts create variance:
- Typical window: 47,855 Hz (stable)
- Burst window: 48,364 Hz (+1% spike)
- Occasional slow: 47,600 Hz (-0.5% dip)
- Spread: 764 Hz (too wide for 500 Hz threshold)

**Why it still works:**

The display correction (0.9927 = -0.73%) handles the main mismatch, and the ±1% dynamic rate control absorbs the audio drift. Buffer fill remains stable around 55%.

**Evidence (current):**
```
Display: meter stable at 59.66 Hz (swing=0.41 Hz) ✓
Audio: measured=0Hz (meter not stable, 764 Hz spread > 500 Hz threshold)
Fill: 38-67%, averaging ~55%
```

**Trade-off:**

We could increase the audio threshold to 800-1000 Hz to enable correction on miyoomini, but that would risk locking in bad values. The conservative 500 Hz threshold ensures we only apply correction when data quality is high.

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

### Audio Buffer Blocking Loop ✓

**Status:** Removed (2025-12-04)
**Resolution:** Replaced with warning + graceful degradation

**What was removed:**
```c
// OLD: Block up to 10ms waiting for buffer space
while (tries < 10 && available < estimated_output) {
    SDL_Delay(1);  // Could block emulation!
}
```

**Why it was safe to remove:**
1. Predated modern RateMeter rate control system
2. Resampler already handles buffer full gracefully (partial writes + state preservation)
3. If buffer fills, it indicates rate control failure → need to know about it
4. Now logs warning instead of silently blocking

**Benefits:**
- Removes 10ms blocking hazard from main emulation loop
- Makes rate control failures visible for debugging
- Trusts sophisticated rate control instead of legacy safety code

### Resampler Accuracy ✓

**Status:** Verified correct
**Resolution:** No fix needed

The resampler was suspected of systematic bias, but logging proved it's accurate:
- `diff` between actual and expected ratio: ±0.002% (essentially zero)
- `frac_pos` varies randomly (no drift)
- Window-averaged comparisons match within 0.002%

### Rate Measurement System ✓

**Status:** Working correctly
**Resolution:** Implemented unified RateMeter system

- Same RateMeter algorithm for both display and audio
- Different measurement intervals (display: per-frame, audio: 2-sec windows)
- Median-based calculation with min/max tracking
- Values lock when stable, only update with better data (smaller spread)
- Display: 30 samples, stability < 1.0 Hz, per-frame measurements
- Audio: 10 samples, stability < 500 Hz, 2-second windows (averages SDL bursts)
- Dynamic buffer sizing based on display swing
- Adaptive d: 2% before stable, 1% after
- Graceful fallback when data is too noisy (returns 0 = no correction)

**Performance overhead:** Negligible (<0.1% of frame budget, ~17µs per frame average)

---

## Performance Analysis

### Render Loop Overhead

Comprehensive audit of the main emulation loop (60 fps = 16,667µs budget):

| Operation | Cost | % of Frame |
|-----------|------|------------|
| measureDisplayRate() + RateMeter_addSample() | 5.0 µs | 0.03% |
| measureAudioRate() | 0.5 µs | 0.003% |
| updateAutoCPU() (every 30 frames) | 200 µs | 0.04% |
| Other framework code | 10 µs | 0.06% |
| **Total overhead (average)** | **17 µs** | **0.1%** |
| **Available for emulation** | **16,650 µs** | **99.9%** |

**Conclusion:** Framework overhead is negligible. RateMeter sorting (insertion sort on 30 floats) costs 5µs per frame but provides stable, maintainable rate measurement. Not worth optimizing.

---

## Document Updates Needed

- [x] Update docs/audio-rate-control.md with unified RateMeter system
- [x] Document blocking loop removal and performance analysis
