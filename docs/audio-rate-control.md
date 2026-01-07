# Audio Rate Control

LessUI implements dynamic rate control for audio/video synchronization based on Hans-Kristian Arntzen's paper "Dynamic Rate Control for Retro Game Emulators" (2012).

## The Problem

Retro game consoles are highly synchronous - audio generation is locked to video frame rate. Every video frame produces a fixed number of audio samples. When emulating on modern hardware:

- The emulated system's refresh rate (e.g., NES at 60.0988 Hz) differs from the host display
- The emulated audio rate (e.g., 32040.5 Hz) differs from the host audio rate (e.g., 48000 Hz)
- If you sync to video (VSync), the audio buffer will eventually underrun or overrun
- If you sync to audio (blocking writes), you'll miss VBlanks causing video stuttering

**The fundamental challenge**: Synchronize to vsync (smooth video) while never underrunning or blocking on audio.

## Runtime-Adaptive Sync System

LessUI uses a runtime-adaptive approach that measures the actual display refresh rate and selects the appropriate sync mode automatically.

### Two Sync Modes

| Mode            | Timing Source         | Audio Handling                     | When Used                        |
| --------------- | --------------------- | ---------------------------------- | -------------------------------- |
| **Audio Clock** | Blocking audio writes | Fixed ratio (no rate control)      | Startup default, Hz mismatch >1% |
| **Vsync**       | Display vsync         | PI rate control (±0.5% adjustment) | Hz mismatch <1% from game fps    |

### Mode Selection Algorithm

```
1. Start in Audio Clock mode (safe default, works on all hardware)
2. Measure actual display Hz via vsync timing (~2 seconds warmup)
3. If measured Hz within 1% of game fps → switch to Vsync mode
4. Monitor for drift; fall back to Audio Clock if Hz becomes unstable
```

This eliminates compile-time mode selection and handles hardware variance automatically.

## Audio Clock Mode

When display Hz differs significantly from game fps (>1%), rate control cannot compensate without audible pitch changes. Instead:

- Audio writes **block** when the buffer is full
- Audio hardware clock drives emulation timing
- Frame duplication occurs naturally (less visible than frame skipping)
- No rate control needed - the blocking provides natural backpressure

**Benefits:**

- Works with any display refresh rate
- Audio buffer stays naturally stable
- No PI controller oscillation or windup

## Vsync Mode (Rate Control Active)

When display Hz closely matches game fps (<1%), vsync provides timing and rate control keeps the audio buffer stable.

### Arntzen's Core Formula

The paper's pure proportional control adjusts resampling ratio based on buffer fill:

```
error = 1 - 2×fill
adjustment = error × d
ratio = 1 - adjustment
```

**Behavior:**

- Buffer empty (fill=0): error=+1 → produce MORE samples → fill buffer
- Buffer half (fill=0.5): error=0 → maintain equilibrium
- Buffer full (fill=1): error=-1 → produce FEWER samples → drain buffer

The paper proves this converges exponentially to a stable equilibrium.

### Our Extension: Dual-Timescale PI Controller

Pure proportional control works when the host display/audio clocks match the emulated system. On cheap handheld hardware, persistent clock mismatches cause the buffer to settle away from 50%.

We extend Arntzen with an integral term on a **separate, slower timescale**:

```c
// Fast timescale (proportional): immediate response to buffer jitter
float error = 1.0f - 2.0f * fill;
float p_term = error * d;

// Slow timescale (integral): learns persistent clock offset over ~5 seconds
error_avg = α * error + (1-α) * error_avg;  // Smooth error first
integral += error_avg * ki;                  // Then integrate
integral = clamp(integral, -0.02, +0.02);    // Limit to ±2%

// Combined adjustment
float adjustment = p_term + integral;
```

**Key insight**: Original PI failed because both terms operated on the same timescale, causing them to fight. By smoothing error before integrating (~5 seconds), the integral only sees persistent trends, not per-frame noise.

### Parameters

| Parameter  | Value    | Purpose                                                |
| ---------- | -------- | ------------------------------------------------------ |
| **d**      | 1.0%     | Proportional gain. Handles frame-to-frame jitter.      |
| **ki**     | 0.00005  | Integral gain. Learns persistent clock offset.         |
| **α**      | 0.003    | Error smoothing (~333 frames / 5.5 seconds at 60fps).  |
| **clamp**  | ±2%      | Max integral correction. Handles hardware clock drift. |
| **buffer** | 5 frames | ~83ms latency. Headroom for timing variance.           |

## Implementation Details

### Sync Mode Callbacks

The audio system queries the sync manager to determine behavior:

```c
// Set by player at init
SND_setSyncCallbacks(
    SyncManager_shouldUseRateControl,  // true in Vsync mode
    SyncManager_shouldBlockAudio       // true in Audio Clock mode
);

// In SND_batchSamples()
bool should_block = snd.should_block_audio();
bool should_use_rate_control = !should_block && snd.should_use_rate_control();
```

This decouples the audio system from sync mode decisions.

### Per-Frame Integral Update

The integral must update **once per frame**, not once per audio batch. Some cores (e.g., 64-bit snes9x) use per-sample audio callbacks, calling `SND_batchSamples()` ~535 times per frame. Without this fix, effective ki = 535× intended, causing wild oscillation.

```c
// Called once per frame from main loop, before core.run()
void SND_newFrame(void) {
    // Skip in audio-clock mode (no rate control)
    if (!snd.should_use_rate_control || !snd.should_use_rate_control())
        return;

    SDL_LockAudio();

    float fill = SND_getBufferFillLevel();
    float error = 1.0f - 2.0f * fill;

    // Update smoothed error and integral (once per frame)
    error_avg = α * error + (1-α) * error_avg;
    integral += error_avg * ki;
    integral = clamp(integral, -0.02, +0.02);

    SDL_UnlockAudio();
}
```

### Thread Safety

Rate control state is shared between the main thread (integral updates) and audio thread (buffer reads). All shared state access requires `SDL_LockAudio()` to prevent torn reads on 64-bit ARM where float operations aren't atomic.

### Sample Rate Policy

Platforms must respect the core's native sample rate:

```c
int PLAT_pickSampleRate(int requested, int max) {
    return MIN(requested, max);  // Use core's rate if supported
}
```

Forcing a different rate (e.g., always 48kHz when core wants 32kHz) causes unnecessary resampling and wider buffer swings.

## Tuning Results

Tested across three platforms with different timing characteristics:

| Device     | Fill | Variance | Integral | Underruns | Notes                        |
| ---------- | ---- | -------- | -------- | --------- | ---------------------------- |
| rg35xxplus | 59%  | ±8%      | +0.15%   | 0         | Rock solid                   |
| tg5040     | 61%  | ±16%     | -0.71%   | 0         | Integral learns clock offset |
| miyoomini  | 64%  | ±14%     | +0.42%   | 0         | Fixed by sample rate policy  |

**Key findings:**

- d=0.010 (1.0%) is optimal for handheld timing variance (paper's 0.2-0.5% is for desktop)
- Integral converges in ~15-20 seconds to steady-state offset
- Each device has different clock characteristics that the integral learns

## Code References

- Sync manager: `workspace/all/player/sync_manager.c` (mode selection, Hz measurement)
- PI controller: `workspace/all/common/api.c` (`SND_calculateRateAdjust`, `SND_newFrame`)
- Sync callbacks: `workspace/all/common/api.c` (`SND_setSyncCallbacks`)
- Parameters: `workspace/all/common/api.c` (SND_RATE_CONTROL_D, SND_RATE_CONTROL_KI, etc.)
- Resampler: `workspace/all/common/audio_resampler.c`
- Sample rate policy: `workspace/<platform>/platform/platform.c` (`PLAT_pickSampleRate`)

## References

- Arntzen, H.K. (2012). ["Dynamic Rate Control for Retro Game Emulators"](https://docs.libretro.com/guides/ratecontrol.pdf)
