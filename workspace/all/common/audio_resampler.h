/**
 * audio_resampler.h - Sample rate conversion for audio
 *
 * Provides nearest-neighbor resampling for converting between different
 * audio sample rates. Uses a Bresenham-like algorithm to determine when
 * to drop or duplicate samples.
 *
 * Extracted from api.c for testability without SDL/ring buffer dependencies.
 */

#ifndef __AUDIO_RESAMPLER_H__
#define __AUDIO_RESAMPLER_H__

#include <stdint.h>

/**
 * Stereo audio frame (left/right channels)
 */
typedef struct SND_Frame {
	int16_t left;
	int16_t right;
} SND_Frame;

/**
 * Ring buffer abstraction for audio resampling
 */
typedef struct AudioRingBuffer {
	SND_Frame* frames; // Buffer array
	int capacity; // Total buffer size
	int write_pos; // Current write position
} AudioRingBuffer;

/**
 * Resampler state for nearest-neighbor sample rate conversion
 */
typedef struct AudioResampler {
	int sample_rate_in; // Input sample rate (e.g., 44100)
	int sample_rate_out; // Output sample rate (e.g., 48000)
	int diff; // Accumulated error for Bresenham algorithm
} AudioResampler;

/**
 * Result of processing a frame through the resampler
 */
typedef struct ResampleResult {
	int wrote_frame; // 1 if frame was written to buffer, 0 if skipped
	int consumed; // 1 if should advance to next input frame, 0 if reuse
} ResampleResult;

/**
 * Initializes a resampler for given sample rates.
 *
 * @param resampler Resampler instance to initialize
 * @param rate_in Input sample rate in Hz
 * @param rate_out Output sample rate in Hz
 */
void AudioResampler_init(AudioResampler* resampler, int rate_in, int rate_out);

/**
 * Resets the resampler's internal state.
 *
 * @param resampler Resampler instance to reset
 */
void AudioResampler_reset(AudioResampler* resampler);

/**
 * Processes a single audio frame through nearest-neighbor resampling.
 *
 * Uses Bresenham-like algorithm to determine:
 * 1. Whether to write this frame to the output buffer
 * 2. Whether to consume (advance) the input or reuse the same frame
 *
 * The algorithm maintains an accumulated difference to decide when to
 * write/skip frames for sample rate conversion.
 *
 * Example - Upsampling (44100 Hz -> 48000 Hz):
 *   Call 1: wrote=1, consumed=0 (write F1, reuse it)
 *   Call 2: wrote=1, consumed=1 (write F1 again, advance to F2)
 *   Call 3: wrote=1, consumed=1 (write F2, advance to F3)
 *   (Roughly 8.8% of frames are duplicated)
 *
 * Example - Downsampling (48000 Hz -> 44100 Hz):
 *   Call 1: wrote=1, consumed=1 (write F1, advance)
 *   Call 2: wrote=0, consumed=1 (skip F2, advance to F3)
 *   (Roughly 8.1% of frames are skipped)
 *
 * @param resampler Resampler instance with state
 * @param buffer Ring buffer to write to
 * @param frame Input audio frame to process
 * @return ResampleResult indicating write/consume decisions
 */
ResampleResult AudioResampler_processFrame(AudioResampler* resampler, AudioRingBuffer* buffer,
                                           SND_Frame frame);

/**
 * Checks if resampling is needed for given sample rates.
 *
 * @param rate_in Input sample rate
 * @param rate_out Output sample rate
 * @return 1 if rates differ (resampling needed), 0 if rates match
 */
int AudioResampler_isNeeded(int rate_in, int rate_out);

#endif // __AUDIO_RESAMPLER_H__
