# Known Issues

Issues discovered during development and testing of LessUI.

## Audio Rate Control

### Buffer Settling Above 50% (Active Investigation)

**Status:** Under investigation
**Severity:** Medium
**Affected:** rg35xxplus (confirmed), possibly all platforms

**Symptoms:**
- Buffer settles at 60-69% instead of target 50%
- Rate control applying positive adjustments (draining)
- Suggests over-production of audio samples

**Evidence from rg35xxplus log (09:18:33):**
```
[INFO] Audio: display rate set to 59.71 Hz (base_correction=0.9934, -0.66% mismatch)
[DEBUG] Audio: fill=49% base=0.9934 adjust=-0.01% total=-0.67% (d=0.5%)
[DEBUG] Auto CPU: fill=59% level=0
[DEBUG] Audio: fill=40% base=0.9934 adjust=-0.10% total=-0.75% (d=0.5%)
[DEBUG] Auto CPU: fill=66% level=0
[DEBUG] Audio: fill=46% base=0.9934 adjust=-0.04% total=-0.69% (d=0.5%)
[DEBUG] Auto CPU: fill=69% level=0
```

Buffer oscillating 40-69%, averaging ~60%, not converging to 50%.

**Expected behavior:**
- fill=50%, adjust=0%, total=-0.66%
- Stable equilibrium

**Actual behavior:**
- fill=60-69%, adjust=+0.3 to +0.5%
- Oscillating, overfilling

**Hypothesis:**
1. base_correction direction might be inverted
2. Resampler interpretation of total_adjust might be wrong
3. Need to verify: larger total_adjust → more outputs or fewer outputs?

**Investigation needed:**
- Trace through resampler with actual values
- Verify base_correction=0.9934 produces FEWER samples (as intended)
- Check if we're multiplying when we should divide (or vice versa)

---

### Startup Underruns During Buffer Priming

**Status:** Known, minor
**Severity:** Low
**Affected:** All platforms

**Symptoms:**
- 60+ audio underruns in the first second of gameplay
- Buffer not yet primed when audio callback starts
- Causes brief audio glitches at startup

**Evidence:**
```
[WARN] Audio: 60 underrun(s) in last second
```

**Workarounds considered:**
- Increase buffer from 4 to 5-6 frames (reduces underruns, increases latency)
- Pre-fill buffer before starting audio (complex)
- Accept as unavoidable with low-latency buffer

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
[DEBUG] Auto CPU: p90=6672us/16638us (40%) cpu=46% fill=53% level=2 (high=0 low=4)
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

## Document Updates Needed

- [ ] Update main README.md with audio rate control summary
- [ ] Document adaptive d parameter in code comments
- [ ] Add troubleshooting section to docs/audio-rate-control.md
