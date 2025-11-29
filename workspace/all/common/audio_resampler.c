/**
 * audio_resampler.c - Sample rate conversion for audio
 *
 * Implements linear interpolation resampling using fixed-point math.
 * Supports dynamic rate adjustment for buffer-level-based rate control.
 */

#include "audio_resampler.h"

/**
 * Linear interpolation between two int16 values using fixed-point fraction.
 *
 * @param a First value (at frac=0)
 * @param b Second value (at frac=FRAC_ONE)
 * @param frac Interpolation position (0 to FRAC_ONE-1)
 * @return Interpolated value
 */
static inline int16_t lerp_s16(int16_t a, int16_t b, uint32_t frac) {
	// Use 32-bit math to avoid overflow
	// result = a + (b - a) * frac / FRAC_ONE
	int32_t diff = (int32_t)b - (int32_t)a;
	return (int16_t)(a + ((diff * (int32_t)frac) >> FRAC_BITS));
}

/**
 * Initializes a resampler for given sample rates.
 */
void AudioResampler_init(AudioResampler* resampler, int rate_in, int rate_out) {
	resampler->sample_rate_in = rate_in;
	resampler->sample_rate_out = rate_out;

	// Guard against division by zero
	if (rate_out <= 0) {
		resampler->frac_step = FRAC_ONE; // Default to 1:1
	} else {
		// Calculate base step size in fixed-point
		// step = rate_in / rate_out * FRAC_ONE
		// For 44100->48000: step = 44100/48000 * 65536 = 60211 (0.918... in fixed-point)
		// For 48000->44100: step = 48000/44100 * 65536 = 71347 (1.088... in fixed-point)
		resampler->frac_step = ((uint64_t)rate_in << FRAC_BITS) / rate_out;
	}

	resampler->frac_pos = 0;
	resampler->prev_frame = (SND_Frame){0, 0};
	resampler->has_prev = 0;
}

/**
 * Resets the resampler's internal state.
 */
void AudioResampler_reset(AudioResampler* resampler) {
	resampler->frac_pos = 0;
	resampler->prev_frame = (SND_Frame){0, 0};
	resampler->has_prev = 0;
}

/**
 * Resamples a batch of audio frames using linear interpolation.
 */
ResampleResult AudioResampler_resample(AudioResampler* resampler, AudioRingBuffer* buffer,
                                       const SND_Frame* frames, int frame_count,
                                       float ratio_adjust) {
	ResampleResult result = {0, 0};

	if (frame_count <= 0) {
		return result;
	}

	// Apply dynamic rate adjustment to step size
	// ratio_adjust > 1.0 = larger steps = consume input faster = fewer outputs
	// ratio_adjust < 1.0 = smaller steps = consume input slower = more outputs
	uint32_t adjusted_step = (uint32_t)(resampler->frac_step * ratio_adjust);

	// Clamp to bounds matching the dynamic rate control algorithm's d parameter.
	// The rate controller uses d = 0.005 (0.5%), so clamp to ±0.5% of base step.
	// This prevents any code path from causing larger pitch shifts than intended.
	// Using slightly wider bounds (1%) to allow for floating point rounding.
	const float max_deviation = 0.01f;
	uint32_t min_step = (uint32_t)(resampler->frac_step * (1.0f - max_deviation));
	uint32_t max_step = (uint32_t)(resampler->frac_step * (1.0f + max_deviation));
	if (adjusted_step < min_step)
		adjusted_step = min_step;
	if (adjusted_step > max_step)
		adjusted_step = max_step;

	int input_idx = 0;
	uint32_t frac_pos = resampler->frac_pos;
	SND_Frame prev = resampler->prev_frame;
	int has_prev = resampler->has_prev;

	// Process until we've consumed all input or can't continue
	while (input_idx < frame_count) {
		SND_Frame curr = frames[input_idx];

		// If this is the first sample ever, just store it and continue
		if (!has_prev) {
			prev = curr;
			has_prev = 1;
			input_idx++;
			result.frames_consumed++;
			continue;
		}

		// Generate output samples while frac_pos < FRAC_ONE
		// (we're still interpolating between prev and curr)
		while (frac_pos < FRAC_ONE) {
			// Check for buffer overflow before writing
			if (buffer) {
				int next_pos = buffer->write_pos + 1;
				if (next_pos >= buffer->capacity)
					next_pos = 0;
				if (next_pos == buffer->read_pos) {
					// Buffer full - stop and save state for next call
					goto done;
				}
			}

			// Interpolate between prev and curr
			SND_Frame out;
			out.left = lerp_s16(prev.left, curr.left, frac_pos);
			out.right = lerp_s16(prev.right, curr.right, frac_pos);

			// Write to buffer
			if (buffer) {
				buffer->frames[buffer->write_pos] = out;
				buffer->write_pos++;
				if (buffer->write_pos >= buffer->capacity)
					buffer->write_pos = 0;
			}
			result.frames_written++;

			// Advance fractional position
			frac_pos += adjusted_step;
		}

		// We've passed curr, move to next input sample
		frac_pos -= FRAC_ONE; // Keep fractional remainder
		prev = curr;
		input_idx++;
		result.frames_consumed++;
	}

done:
	// Save state for next call
	resampler->frac_pos = frac_pos;
	resampler->prev_frame = prev;
	resampler->has_prev = has_prev;

	return result;
}

/**
 * Checks if resampling is needed for given sample rates.
 */
int AudioResampler_isNeeded(int rate_in, int rate_out) {
	return rate_in != rate_out;
}

/**
 * Calculates the approximate number of output frames for given input.
 */
int AudioResampler_estimateOutput(AudioResampler* resampler, int input_frames, float ratio_adjust) {
	// Guard against division by zero
	if (resampler->sample_rate_in <= 0 || ratio_adjust <= 0.0f)
		return input_frames;

	// output = input * (rate_out / rate_in) / ratio_adjust
	double ratio = (double)resampler->sample_rate_out / resampler->sample_rate_in;
	return (int)(input_frames * ratio / ratio_adjust + 0.5);
}
