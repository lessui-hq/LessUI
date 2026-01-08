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

| Mode            | Timing Source         | Audio Handling                    | When Used                        |
| --------------- | --------------------- | --------------------------------- | -------------------------------- |
| **Audio Clock** | Blocking audio writes | Fixed ratio (no rate control)     | Startup default, Hz mismatch >1% |
| **Vsync**       | Display vsync         | P rate control (±1.2% max adjust) | Hz mismatch <1% from game fps    |

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
- No controller oscillation or windup

## Vsync Mode (Rate Control Active)

When display Hz closely matches game fps (<1%), vsync provides timing and rate control keeps the audio buffer stable.

### Arntzen's Proportional Control

The paper's proportional control adjusts resampling ratio based on buffer fill:

```c
error = 1 - 2 * fill;      // +1 when empty, 0 at half, -1 when full
adjustment = error * d;    // Bounded by ±d
ratio = 1 - adjustment;    // Resampling ratio
```

**Behavior:**

- Buffer empty (fill=0): error=+1 → produce MORE samples → fill buffer
- Buffer half (fill=0.5): error=0 → maintain equilibrium
- Buffer full (fill=1): error=-1 → produce FEWER samples → drain buffer

The paper proves this converges exponentially to a stable equilibrium.

### Why Pure P Works

Our 1% Hz tolerance for vsync mode ensures we're within the paper's "reasonably close" bounds:

- **Arntzen tested with:** 0.36% Hz mismatch, d=0.5% → 1.4x headroom
- **Our parameters:** up to 1% Hz mismatch, d=0.8% → 1.25x headroom better than Arntzen's ratio

The 1% gate ensures devices in vsync mode have mismatch bounded within what proportional control can handle. Devices outside this range fall back to audio-clock mode where rate control isn't needed.

### Parameters

| Parameter  | Value    | Purpose                                                     |
| ---------- | -------- | ----------------------------------------------------------- |
| **d**      | 0.8%     | Proportional gain. Handles frame-to-frame jitter.           |
| **buffer** | 8 frames | ~133ms latency. Matches RetroArch handheld default (128ms). |

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

### Sample Rate Policy

Platforms must respect the core's native sample rate:

```c
int PLAT_pickSampleRate(int requested, int max) {
    return MIN(requested, max);  // Use core's rate if supported
}
```

Forcing a different rate (e.g., always 48kHz when core wants 32kHz) causes unnecessary resampling and wider buffer swings.

## Code References

- Sync manager: `workspace/all/player/sync_manager.c` (mode selection, Hz measurement)
- Rate control: `workspace/all/common/api.c` (`SND_calculateRateAdjust`)
- Sync callbacks: `workspace/all/common/api.c` (`SND_setSyncCallbacks`)
- Parameters: `workspace/all/common/defines.h` (`SND_RATE_CONTROL_D`)
- Resampler: `workspace/all/common/audio_resampler.c`
- Sample rate policy: `workspace/<platform>/platform/platform.c` (`PLAT_pickSampleRate`)

## References

- Arntzen, H.K. (2012). ["Dynamic Rate Control for Retro Game Emulators"](https://docs.libretro.com/guides/ratecontrol.pdf)
