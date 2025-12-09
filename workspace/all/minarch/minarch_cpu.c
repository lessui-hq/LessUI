/**
 * minarch_cpu.c - Auto CPU scaling utilities
 *
 * Implements dynamic CPU frequency scaling based on emulation performance.
 * Extracted from minarch.c for testability.
 *
 * The algorithm uses frame execution time (90th percentile) to determine
 * CPU utilization, then adjusts frequency to maintain target utilization.
 *
 * Key concepts:
 * - Performance scales linearly with frequency
 * - Boost aggressively (jump to predicted frequency) to avoid stuttering
 * - Reduce conservatively (limited steps) to avoid oscillation
 * - Panic path on audio underrun with cooldown
 */

#include "minarch_cpu.h"

#include <stdlib.h>
#include <string.h>

///////////////////////////////
// Comparison for qsort
///////////////////////////////

static int compare_uint64(const void* a, const void* b) {
	uint64_t va = *(const uint64_t*)a;
	uint64_t vb = *(const uint64_t*)b;
	if (va < vb)
		return -1;
	if (va > vb)
		return 1;
	return 0;
}

///////////////////////////////
// Public Functions
///////////////////////////////

void MinArchCPU_initConfig(MinArchCPUConfig* config) {
	config->window_frames = MINARCH_CPU_DEFAULT_WINDOW_FRAMES;
	config->util_high = MINARCH_CPU_DEFAULT_UTIL_HIGH;
	config->util_low = MINARCH_CPU_DEFAULT_UTIL_LOW;
	config->boost_windows = MINARCH_CPU_DEFAULT_BOOST_WINDOWS;
	config->reduce_windows = MINARCH_CPU_DEFAULT_REDUCE_WINDOWS;
	config->startup_grace = MINARCH_CPU_DEFAULT_STARTUP_GRACE;
	config->min_freq_khz = MINARCH_CPU_DEFAULT_MIN_FREQ_KHZ;
	config->target_util = MINARCH_CPU_DEFAULT_TARGET_UTIL;
	config->max_step = MINARCH_CPU_DEFAULT_MAX_STEP;
}

void MinArchCPU_initState(MinArchCPUState* state) {
	memset(state, 0, sizeof(MinArchCPUState));
	// Set sensible defaults
	state->frame_budget_us = 16667; // 60fps default
}

int MinArchCPU_findNearestIndex(const int* frequencies, int count, int target_khz) {
	if (count <= 0)
		return 0;

	int best_idx = 0;
	int best_diff = abs(frequencies[0] - target_khz);

	for (int i = 1; i < count; i++) {
		int diff = abs(frequencies[i] - target_khz);
		if (diff < best_diff) {
			best_diff = diff;
			best_idx = i;
		}
	}
	return best_idx;
}

void MinArchCPU_detectFrequencies(MinArchCPUState* state, const MinArchCPUConfig* config,
                                  const int* raw_frequencies, int raw_count) {
	// Filter frequencies below minimum threshold
	state->freq_count = 0;
	for (int i = 0; i < raw_count && state->freq_count < MINARCH_CPU_MAX_FREQUENCIES; i++) {
		if (raw_frequencies[i] >= config->min_freq_khz) {
			state->frequencies[state->freq_count++] = raw_frequencies[i];
		}
	}

	if (state->freq_count >= 2) {
		state->use_granular = 1;

		// Calculate preset indices based on percentage of max frequency
		int max_freq = state->frequencies[state->freq_count - 1];

		// POWERSAVE: 55% of max
		int ps_target = max_freq * 55 / 100;
		state->preset_indices[MINARCH_CPU_LEVEL_POWERSAVE] =
		    MinArchCPU_findNearestIndex(state->frequencies, state->freq_count, ps_target);

		// NORMAL: 80% of max
		int normal_target = max_freq * 80 / 100;
		state->preset_indices[MINARCH_CPU_LEVEL_NORMAL] =
		    MinArchCPU_findNearestIndex(state->frequencies, state->freq_count, normal_target);

		// PERFORMANCE: max frequency
		state->preset_indices[MINARCH_CPU_LEVEL_PERFORMANCE] = state->freq_count - 1;
	} else {
		state->use_granular = 0;
	}

	state->frequencies_detected = 1;
}

void MinArchCPU_reset(MinArchCPUState* state, const MinArchCPUConfig* config, double fps,
                      unsigned current_underruns) {
	(void)config; // May be used in future for configurable grace period

	state->frame_count = 0;
	state->high_util_windows = 0;
	state->low_util_windows = 0;
	state->last_underrun = current_underruns;
	state->startup_frames = 0;
	state->panic_cooldown = 0;
	state->frame_time_index = 0;

	// Calculate frame budget from FPS
	if (fps > 0) {
		state->frame_budget_us = (uint64_t)(1000000.0 / fps);
	} else {
		state->frame_budget_us = 16667; // Default to 60fps
	}

	// Clear frame time buffer
	memset(state->frame_times, 0, sizeof(state->frame_times));
}

void MinArchCPU_recordFrameTime(MinArchCPUState* state, uint64_t frame_time_us) {
	state->frame_times[state->frame_time_index % MINARCH_CPU_FRAME_BUFFER_SIZE] = frame_time_us;
	state->frame_time_index++;
}

uint64_t MinArchCPU_percentile90(const uint64_t* frame_times, int count) {
	if (count <= 0)
		return 0;

	// Limit to buffer size
	if (count > MINARCH_CPU_FRAME_BUFFER_SIZE)
		count = MINARCH_CPU_FRAME_BUFFER_SIZE;

	// Copy and sort
	uint64_t sorted[MINARCH_CPU_FRAME_BUFFER_SIZE];
	memcpy(sorted, frame_times, count * sizeof(uint64_t));
	qsort(sorted, count, sizeof(uint64_t), compare_uint64);

	// 90th percentile index
	int p90_idx = (count * 90) / 100;
	if (p90_idx >= count)
		p90_idx = count - 1;

	return sorted[p90_idx];
}

int MinArchCPU_predictFrequency(int current_freq, int current_util, int target_util) {
	if (target_util <= 0)
		return current_freq;

	// Linear scaling: new_freq = current_freq * current_util / target_util
	return current_freq * current_util / target_util;
}

int MinArchCPU_getPresetPercentage(MinArchCPULevel level) {
	switch (level) {
	case MINARCH_CPU_LEVEL_POWERSAVE:
		return 55;
	case MINARCH_CPU_LEVEL_NORMAL:
		return 80;
	case MINARCH_CPU_LEVEL_PERFORMANCE:
	default:
		return 100;
	}
}

MinArchCPUDecision MinArchCPU_update(MinArchCPUState* state, const MinArchCPUConfig* config,
                                     bool fast_forward, bool show_menu, unsigned current_underruns,
                                     MinArchCPUResult* result) {
	// Initialize result if provided
	if (result) {
		result->decision = MINARCH_CPU_DECISION_NONE;
		result->new_index = state->target_index;
		result->new_level = state->target_level;
		result->utilization = 0;
		result->p90_time = 0;
	}

	// Skip during special states
	if (fast_forward || show_menu) {
		if (result)
			result->decision = MINARCH_CPU_DECISION_SKIP;
		return MINARCH_CPU_DECISION_SKIP;
	}

	// Startup grace period
	if (state->startup_frames < config->startup_grace) {
		state->startup_frames++;
		if (result)
			result->decision = MINARCH_CPU_DECISION_SKIP;
		return MINARCH_CPU_DECISION_SKIP;
	}

	// Get current indices
	int current_idx = state->target_index;
	int current_level = state->target_level;
	int max_idx = state->freq_count - 1;
	if (max_idx < 0)
		max_idx = 0;

	// Check if at max
	bool at_max = state->use_granular ? (current_idx >= max_idx) : (current_level >= 2);

	// Emergency: check for underruns (panic path)
	if (current_underruns > state->last_underrun && !at_max) {
		// Underrun detected - boost by up to max_step
		if (state->use_granular) {
			int new_idx = current_idx + config->max_step;
			if (new_idx > max_idx)
				new_idx = max_idx;
			state->target_index = new_idx;
			if (result) {
				result->decision = MINARCH_CPU_DECISION_PANIC;
				result->new_index = new_idx;
			}
		} else {
			int new_level = current_level + config->max_step;
			if (new_level > 2)
				new_level = 2;
			state->target_level = new_level;
			if (result) {
				result->decision = MINARCH_CPU_DECISION_PANIC;
				result->new_level = new_level;
			}
		}

		state->high_util_windows = 0;
		state->low_util_windows = 0;
		state->panic_cooldown = 8; // ~4 seconds before allowing reduction
		state->last_underrun = 0; // Reset after handling

		return MINARCH_CPU_DECISION_PANIC;
	}

	// Update underrun tracking (even if at max)
	if (current_underruns > state->last_underrun) {
		state->last_underrun = current_underruns;
	}

	// Count frames in current window
	state->frame_count++;

	// Check if window is complete
	if (state->frame_count < config->window_frames) {
		return MINARCH_CPU_DECISION_NONE;
	}

	// Calculate 90th percentile frame time
	int samples = state->frame_time_index;
	if (samples > MINARCH_CPU_FRAME_BUFFER_SIZE)
		samples = MINARCH_CPU_FRAME_BUFFER_SIZE;

	if (samples < 5) {
		// Not enough samples - reset and wait
		state->frame_count = 0;
		return MINARCH_CPU_DECISION_NONE;
	}

	uint64_t p90_time = MinArchCPU_percentile90(state->frame_times, samples);

	// Calculate utilization as percentage of frame budget
	unsigned util = 0;
	if (state->frame_budget_us > 0) {
		util = (unsigned)((p90_time * 100) / state->frame_budget_us);
		if (util > 200)
			util = 200; // Cap at 200% for sanity
	}

	if (result) {
		result->utilization = util;
		result->p90_time = p90_time;
	}

	MinArchCPUDecision decision = MINARCH_CPU_DECISION_NONE;

	if (state->use_granular) {
		// Granular mode: linear frequency scaling
		int current_freq = state->frequencies[current_idx];

		// Decrement panic cooldown
		if (state->panic_cooldown > 0) {
			state->panic_cooldown--;
		}

		if (util > config->util_high) {
			// Need more performance
			state->high_util_windows++;
			state->low_util_windows = 0;

			if (state->high_util_windows >= config->boost_windows && current_idx < max_idx) {
				// Predict optimal frequency using linear scaling
				int needed_freq =
				    MinArchCPU_predictFrequency(current_freq, util, config->target_util);
				int new_idx =
				    MinArchCPU_findNearestIndex(state->frequencies, state->freq_count, needed_freq);

				// Ensure we actually go higher
				if (new_idx <= current_idx)
					new_idx = current_idx + 1;
				if (new_idx > max_idx)
					new_idx = max_idx;

				state->target_index = new_idx;
				state->high_util_windows = 0;
				decision = MINARCH_CPU_DECISION_BOOST;

				if (result) {
					result->decision = MINARCH_CPU_DECISION_BOOST;
					result->new_index = new_idx;
				}
			}
		} else if (util < config->util_low) {
			// Can reduce power
			state->low_util_windows++;
			state->high_util_windows = 0;

			// Only reduce if enough windows AND panic cooldown expired
			bool reduce_ok = (state->low_util_windows >= config->reduce_windows) &&
			                 (state->panic_cooldown == 0) && (current_idx > 0);

			if (reduce_ok) {
				// Predict lower frequency
				int needed_freq =
				    MinArchCPU_predictFrequency(current_freq, util, config->target_util);
				int new_idx =
				    MinArchCPU_findNearestIndex(state->frequencies, state->freq_count, needed_freq);

				// Ensure we actually go lower
				if (new_idx >= current_idx)
					new_idx = current_idx - 1;
				if (new_idx < 0)
					new_idx = 0;

				// Limit reduction to max_step
				if (current_idx - new_idx > config->max_step) {
					new_idx = current_idx - config->max_step;
				}

				state->target_index = new_idx;
				state->low_util_windows = 0;
				decision = MINARCH_CPU_DECISION_REDUCE;

				if (result) {
					result->decision = MINARCH_CPU_DECISION_REDUCE;
					result->new_index = new_idx;
				}
			}
		} else {
			// In sweet spot - reset counters
			state->high_util_windows = 0;
			state->low_util_windows = 0;
		}
	} else {
		// Fallback mode: 3-level scaling
		if (util > config->util_high) {
			state->high_util_windows++;
			state->low_util_windows = 0;
		} else if (util < config->util_low) {
			state->low_util_windows++;
			state->high_util_windows = 0;
		} else {
			state->high_util_windows = 0;
			state->low_util_windows = 0;
		}

		// Boost if sustained high utilization
		if (state->high_util_windows >= config->boost_windows && current_level < 2) {
			int new_level = current_level + 1;
			state->target_level = new_level;
			state->high_util_windows = 0;
			decision = MINARCH_CPU_DECISION_BOOST;

			if (result) {
				result->decision = MINARCH_CPU_DECISION_BOOST;
				result->new_level = new_level;
			}
		}

		// Reduce if sustained low utilization
		if (state->low_util_windows >= config->reduce_windows && current_level > 0) {
			int new_level = current_level - 1;
			state->target_level = new_level;
			state->low_util_windows = 0;
			decision = MINARCH_CPU_DECISION_REDUCE;

			if (result) {
				result->decision = MINARCH_CPU_DECISION_REDUCE;
				result->new_level = new_level;
			}
		}
	}

	// Reset window counter
	state->frame_count = 0;

	return decision;
}
