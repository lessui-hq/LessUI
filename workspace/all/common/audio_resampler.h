/**
 * audio_resampler.h - Sample rate conversion for audio
 *
 * Provides linear interpolation resampling for converting between different
 * audio sample rates. Uses fixed-point math for efficiency on ARM devices.
 *
 * Supports dynamic rate adjustment for buffer-level-based rate control,
 * which helps prevent audio underruns and overruns.
 *
 * Extracted from api.c for testability without SDL/ring buffer dependencies.
 */

#ifndef __AUDIO_RESAMPLER_H__
#define __AUDIO_RESAMPLER_H__

#include <stdint.h>

/**
 * Fixed-point format: 16.16 (16 bits integer, 16 bits fraction)
 * This gives us sub-sample precision for smooth interpolation.
 */
#define FRAC_BITS 16
#define FRAC_ONE (1 << FRAC_BITS)
#define FRAC_MASK (FRAC_ONE - 1)

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
 * Resampler state for linear interpolation sample rate conversion
 */
typedef struct AudioResampler {
	int sample_rate_in; // Input sample rate (e.g., 44100)
	int sample_rate_out; // Output sample rate (e.g., 48000)

	// Fixed-point 16.16 format for sub-sample precision
	uint32_t frac_pos; // Fractional position within current input sample pair
	uint32_t frac_step; // Base step size per output sample (rate_in/rate_out in fixed-point)

	// Previous frame for interpolation
	SND_Frame prev_frame;
	int has_prev; // 1 if prev_frame is valid
} AudioResampler;

/**
 * Result of processing frames through the resampler
 */
typedef struct ResampleResult {
	int frames_written; // Number of frames written to output buffer
	int frames_consumed; // Number of input frames consumed
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
 * Call this when seeking or switching content to avoid interpolation
 * artifacts from stale samples.
 *
 * @param resampler Resampler instance to reset
 */
void AudioResampler_reset(AudioResampler* resampler);

/**
 * Resamples a batch of audio frames using linear interpolation.
 *
 * Linear interpolation smoothly blends between adjacent samples based on
 * the fractional position, eliminating the clicks and pops that occur
 * with nearest-neighbor resampling.
 *
 * Example - Upsampling (44100 Hz -> 48000 Hz):
 *   Input samples: [A, B, C, D]
 *   Output might be: [A, lerp(A,B,0.3), lerp(A,B,0.6), B, lerp(B,C,0.2), ...]
 *   More output samples than input, with smooth transitions.
 *
 * Example - Downsampling (48000 Hz -> 44100 Hz):
 *   Input samples: [A, B, C, D, E]
 *   Output might be: [A, lerp(B,C,0.4), lerp(D,E,0.2), ...]
 *   Fewer output samples, some inputs blended together.
 *
 * @param resampler Resampler instance with state
 * @param buffer Ring buffer to write to (NULL for dry run to calculate output size)
 * @param frames Input audio frames to process
 * @param frame_count Number of input frames
 * @param ratio_adjust Dynamic rate adjustment (1.0 = normal, <1.0 = slower, >1.0 = faster)
 *                     Use this for buffer-level-based rate control. Range: 0.95 to 1.05
 * @return ResampleResult with frames_written and frames_consumed
 */
ResampleResult AudioResampler_resample(AudioResampler* resampler, AudioRingBuffer* buffer,
                                       const SND_Frame* frames, int frame_count,
                                       float ratio_adjust);

/**
 * Checks if resampling is needed for given sample rates.
 *
 * @param rate_in Input sample rate
 * @param rate_out Output sample rate
 * @return 1 if rates differ (resampling needed), 0 if rates match
 */
int AudioResampler_isNeeded(int rate_in, int rate_out);

/**
 * Calculates the approximate number of output frames for given input.
 *
 * Useful for buffer sizing. The actual output may vary slightly due to
 * fractional accumulation.
 *
 * @param resampler Resampler instance
 * @param input_frames Number of input frames
 * @param ratio_adjust Dynamic rate adjustment factor
 * @return Estimated number of output frames
 */
int AudioResampler_estimateOutput(AudioResampler* resampler, int input_frames, float ratio_adjust);

// ============================================================================
// Legacy API (for backwards compatibility during transition)
// ============================================================================

/**
 * Result of processing a single frame (legacy API)
 * @deprecated Use AudioResampler_resample instead
 */
typedef struct ResampleResultLegacy {
	int wrote_frame; // 1 if frame was written to buffer, 0 if skipped
	int consumed; // 1 if should advance to next input frame, 0 if reuse
} ResampleResultLegacy;

/**
 * Processes a single audio frame (legacy nearest-neighbor API).
 * @deprecated Use AudioResampler_resample for better quality
 *
 * Maintained for backwards compatibility. Uses the old Bresenham-like
 * algorithm that drops or duplicates samples.
 */
ResampleResultLegacy AudioResampler_processFrame(AudioResampler* resampler, AudioRingBuffer* buffer,
                                                 SND_Frame frame);

#endif // __AUDIO_RESAMPLER_H__
