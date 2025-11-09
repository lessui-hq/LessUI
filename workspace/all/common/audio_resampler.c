/**
 * audio_resampler.c - Sample rate conversion for audio
 *
 * Implements nearest-neighbor resampling using a Bresenham-like algorithm.
 * Extracted from api.c for testability.
 */

#include "audio_resampler.h"

/**
 * Initializes a resampler for given sample rates.
 *
 * @param resampler Resampler instance to initialize
 * @param rate_in Input sample rate in Hz
 * @param rate_out Output sample rate in Hz
 */
void AudioResampler_init(AudioResampler* resampler, int rate_in, int rate_out) {
	resampler->sample_rate_in = rate_in;
	resampler->sample_rate_out = rate_out;
	resampler->diff = 0;
}

/**
 * Resets the resampler's internal state.
 *
 * @param resampler Resampler instance to reset
 */
void AudioResampler_reset(AudioResampler* resampler) {
	resampler->diff = 0;
}

/**
 * Processes a single audio frame through nearest-neighbor resampling.
 *
 * Uses Bresenham-like algorithm to determine when to write frames to the
 * ring buffer and when to advance the input.
 *
 * Algorithm:
 * 1. If diff < sample_rate_out: Write frame to buffer, add sample_rate_in to diff
 * 2. If diff >= sample_rate_out: Consumed=1, subtract sample_rate_out from diff
 *
 * For upsampling (e.g., 44100 -> 48000):
 * - Some frames return consumed=0, meaning "I wrote it, give me same frame again"
 * - This duplicates frames to increase output rate
 *
 * For downsampling (e.g., 48000 -> 44100):
 * - When diff gets too high, frames are skipped (not written)
 * - consumed=1 means "advance input even though I didn't write"
 *
 * @param resampler Resampler instance with state
 * @param buffer Ring buffer to write to
 * @param frame Input audio frame to process
 * @return ResampleResult with wrote_frame and consumed flags
 */
ResampleResult AudioResampler_processFrame(AudioResampler* resampler, AudioRingBuffer* buffer,
                                           SND_Frame frame) {
	ResampleResult result = {0, 0};

	// Decide if we should write this frame to output
	if (resampler->diff < resampler->sample_rate_out) {
		// Write frame to ring buffer
		buffer->frames[buffer->write_pos] = frame;
		buffer->write_pos++;
		if (buffer->write_pos >= buffer->capacity)
			buffer->write_pos = 0;

		result.wrote_frame = 1;
		resampler->diff += resampler->sample_rate_in;
	}

	// Decide if we've consumed an input frame
	if (resampler->diff >= resampler->sample_rate_out) {
		result.consumed = 1;
		resampler->diff -= resampler->sample_rate_out;
	}

	return result;
}

/**
 * Checks if resampling is needed for given sample rates.
 *
 * @param rate_in Input sample rate
 * @param rate_out Output sample rate
 * @return 1 if rates differ (resampling needed), 0 if rates match
 */
int AudioResampler_isNeeded(int rate_in, int rate_out) {
	return rate_in != rate_out;
}
