/**
 * cpu.c - CPU scaling and topology utilities
 *
 * Implements CPU topology detection and dynamic frequency scaling.
 * Used by both launcher (topology detection) and player (autoscaling).
 *
 * The autoscaling algorithm uses frame execution time (90th percentile) to
 * determine CPU utilization, then adjusts frequency to maintain target.
 *
 * Key concepts:
 * - Performance scales linearly with frequency
 * - Boost aggressively (jump to predicted frequency) to avoid stuttering
 * - Reduce conservatively (limited steps) to avoid oscillation
 * - Panic path on audio underrun with cooldown
 */

#include "cpu.h"

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

void CPU_initConfig(CPUConfig* config) {
	config->window_frames = CPU_DEFAULT_WINDOW_FRAMES;
	config->util_high = CPU_DEFAULT_UTIL_HIGH;
	config->util_low = CPU_DEFAULT_UTIL_LOW;
	config->boost_windows = CPU_DEFAULT_BOOST_WINDOWS;
	config->reduce_windows = CPU_DEFAULT_REDUCE_WINDOWS;
	config->startup_grace = CPU_DEFAULT_STARTUP_GRACE;
	config->min_freq_khz = CPU_DEFAULT_MIN_FREQ_KHZ;
	config->target_util = CPU_DEFAULT_TARGET_UTIL;
	config->max_step_down = CPU_DEFAULT_MAX_STEP_DOWN;
	config->panic_step_up = CPU_DEFAULT_PANIC_STEP_UP;
	config->min_buffer_for_reduce = CPU_DEFAULT_MIN_BUFFER_FOR_REDUCE;
}

void CPU_initState(CPUState* state) {
	memset(state, 0, sizeof(CPUState));
	// Set sensible defaults
	state->frame_budget_us = 16667; // 60fps default
}

int CPU_findNearestIndex(const int* frequencies, int count, int target_khz) {
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

void CPU_detectFrequencies(CPUState* state, const CPUConfig* config, const int* raw_frequencies,
                           int raw_count) {
	// Filter frequencies below minimum threshold
	state->freq_count = 0;
	for (int i = 0; i < raw_count && state->freq_count < CPU_MAX_FREQUENCIES; i++) {
		if (raw_frequencies[i] >= config->min_freq_khz) {
			state->frequencies[state->freq_count++] = raw_frequencies[i];
		}
	}

	// Disable scaling if only 0 or 1 frequency available (nothing to scale)
	if (state->freq_count <= 1) {
		state->scaling_disabled = 1;
		state->use_granular = 0;
		state->frequencies_detected = 1;
		return;
	}

	state->scaling_disabled = 0;
	state->use_granular = 1;

	// Calculate preset indices based on percentage of max frequency
	int max_freq = state->frequencies[state->freq_count - 1];

	// POWERSAVE: 55% of max
	int ps_target = max_freq * 55 / 100;
	state->preset_indices[CPU_LEVEL_POWERSAVE] =
	    CPU_findNearestIndex(state->frequencies, state->freq_count, ps_target);

	// NORMAL: 80% of max
	int normal_target = max_freq * 80 / 100;
	state->preset_indices[CPU_LEVEL_NORMAL] =
	    CPU_findNearestIndex(state->frequencies, state->freq_count, normal_target);

	// PERFORMANCE: max frequency
	state->preset_indices[CPU_LEVEL_PERFORMANCE] = state->freq_count - 1;

	state->frequencies_detected = 1;
}

void CPU_reset(CPUState* state, const CPUConfig* config, double fps, unsigned current_underruns) {
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

void CPU_recordFrameTime(CPUState* state, uint64_t frame_time_us) {
	state->frame_times[state->frame_time_index % CPU_FRAME_BUFFER_SIZE] = frame_time_us;
	state->frame_time_index++;
}

uint64_t CPU_percentile90(const uint64_t* frame_times, int count) {
	if (count <= 0)
		return 0;

	// Limit to buffer size
	if (count > CPU_FRAME_BUFFER_SIZE)
		count = CPU_FRAME_BUFFER_SIZE;

	// Copy and sort
	uint64_t sorted[CPU_FRAME_BUFFER_SIZE];
	memcpy(sorted, frame_times, count * sizeof(uint64_t));
	qsort(sorted, count, sizeof(uint64_t), compare_uint64);

	// 90th percentile index
	int p90_idx = (count * 90) / 100;
	if (p90_idx >= count)
		p90_idx = count - 1;

	return sorted[p90_idx];
}

int CPU_predictFrequency(int current_freq, int current_util, int target_util) {
	if (target_util <= 0)
		return current_freq;

	// Linear scaling: new_freq = current_freq * current_util / target_util
	return current_freq * current_util / target_util;
}

int CPU_getPresetPercentage(CPULevel level) {
	switch (level) {
	case CPU_LEVEL_POWERSAVE:
		return 55;
	case CPU_LEVEL_NORMAL:
		return 80;
	case CPU_LEVEL_PERFORMANCE:
	default:
		return 100;
	}
}

int CPU_getPerformancePercent(const CPUState* state) {
	if (!state)
		return -1;

	if (state->scaling_disabled && !state->use_topology) {
		return -1;
	}

	if (state->use_topology) {
		// Topology mode: normalize state index to 0-100
		int max_state = state->topology.state_count - 1;
		if (max_state <= 0)
			return 100;
		int current = state->current_state;
		if (current < 0)
			current = state->target_state;
		return (current * 100) / max_state;
	} else if (state->use_granular) {
		// Granular mode: normalize frequency index to 0-100
		int max_idx = state->freq_count - 1;
		if (max_idx <= 0)
			return 100;
		return (state->current_index * 100) / max_idx;
	} else {
		// Fallback mode: 0=0%, 1=50%, 2=100%
		return state->current_level * 50;
	}
}

const char* CPU_getModeName(const CPUState* state) {
	if (!state)
		return "disabled";

	if (state->scaling_disabled && !state->use_topology) {
		return "disabled";
	}

	if (state->use_topology) {
		return "topology";
	} else if (state->use_granular) {
		return "granular";
	} else {
		return "fallback";
	}
}

CPUDecision CPU_update(CPUState* state, const CPUConfig* config, bool fast_forward, bool show_menu,
                       unsigned current_underruns, unsigned buffer_fill_percent,
                       CPUResult* result) {
	// Initialize result if provided
	if (result) {
		result->decision = CPU_DECISION_NONE;
		result->new_index = state->target_index;
		result->new_level = state->target_level;
		result->utilization = 0;
		result->p90_time = 0;
	}

	// Skip if scaling is disabled (0 or 1 frequency available) AND not using topology mode
	if (state->scaling_disabled && !state->use_topology) {
		if (result)
			result->decision = CPU_DECISION_SKIP;
		return CPU_DECISION_SKIP;
	}

	// Skip during special states
	if (fast_forward || show_menu) {
		if (result)
			result->decision = CPU_DECISION_SKIP;
		return CPU_DECISION_SKIP;
	}

	// Startup grace period
	if (state->startup_frames < config->startup_grace) {
		state->startup_frames++;
		if (result)
			result->decision = CPU_DECISION_SKIP;
		return CPU_DECISION_SKIP;
	}

	// Decrement panic grace period (ignore underruns after frequency change)
	if (state->panic_grace > 0) {
		state->panic_grace--;
	}

	// Get current indices based on mode
	int current_idx = state->target_index;
	int current_level = state->target_level;
	int current_state_idx = state->target_state;
	int max_idx = state->freq_count - 1;
	if (max_idx < 0)
		max_idx = 0;
	int max_state = state->topology.state_count - 1;
	if (max_state < 0)
		max_state = 0;

	// Check if at max based on mode
	bool at_max;
	if (state->use_topology) {
		at_max = (current_state_idx >= max_state);
	} else if (state->use_granular) {
		at_max = (current_idx >= max_idx);
	} else {
		at_max = (current_level >= 2);
	}

	// Track underruns during grace period
	bool underrun_detected = (current_underruns > state->last_underrun);
	if (underrun_detected && state->panic_grace > 0) {
		state->grace_underruns++;
	}

	// Emergency: check for underruns (panic path)
	// Skip if in grace period UNLESS too many underruns (catastrophic failure)
	bool grace_exceeded = (state->grace_underruns >= CPU_PANIC_GRACE_MAX_UNDERRUNS);
	if (underrun_detected && !at_max && (state->panic_grace == 0 || grace_exceeded)) {
		// Underrun detected - boost by panic_step_up
		if (state->use_topology) {
			int new_state = current_state_idx + config->panic_step_up;
			if (new_state > max_state)
				new_state = max_state;
			state->target_state = new_state;
			if (result) {
				result->decision = CPU_DECISION_PANIC;
				result->new_index = new_state; // Use new_index for state index
			}
		} else if (state->use_granular) {
			int new_idx = current_idx + config->panic_step_up;
			if (new_idx > max_idx)
				new_idx = max_idx;
			state->target_index = new_idx;
			if (result) {
				result->decision = CPU_DECISION_PANIC;
				result->new_index = new_idx;
			}
		} else {
			int new_level = current_level + config->panic_step_up;
			if (new_level > 2)
				new_level = 2;
			state->target_level = new_level;
			if (result) {
				result->decision = CPU_DECISION_PANIC;
				result->new_level = new_level;
			}
		}

		state->high_util_windows = 0;
		state->low_util_windows = 0;
		state->stability_streak = 0;
		state->panic_cooldown = 8; // ~4 seconds before allowing reduction
		state->panic_grace = CPU_PANIC_GRACE_FRAMES; // Ignore underruns while new freq settles
		state->grace_underruns = 0;
		state->last_underrun = 0; // Reset after handling

		return CPU_DECISION_PANIC;
	}

	// Update underrun tracking (even if at max)
	if (current_underruns > state->last_underrun) {
		state->last_underrun = current_underruns;
	}

	// Count frames in current window
	state->frame_count++;

	// Check if window is complete
	if (state->frame_count < config->window_frames) {
		return CPU_DECISION_NONE;
	}

	// Calculate 90th percentile frame time
	int samples = state->frame_time_index;
	if (samples > CPU_FRAME_BUFFER_SIZE)
		samples = CPU_FRAME_BUFFER_SIZE;

	if (samples < 5) {
		// Not enough samples - reset and wait
		state->frame_count = 0;
		return CPU_DECISION_NONE;
	}

	uint64_t p90_time = CPU_percentile90(state->frame_times, samples);

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

	CPUDecision decision = CPU_DECISION_NONE;

	if (state->use_topology) {
		// Topology mode: multi-cluster PerfState scaling
		// Decrement panic cooldown
		if (state->panic_cooldown > 0) {
			state->panic_cooldown--;
		}

		if (util > config->util_high) {
			// Need more performance
			state->high_util_windows++;
			state->low_util_windows = 0;

			if (state->high_util_windows >= config->boost_windows &&
			    current_state_idx < max_state) {
				// Step up one state at a time (conservative approach for multi-cluster)
				int new_state = current_state_idx + 1;
				if (new_state > max_state)
					new_state = max_state;

				state->target_state = new_state;
				state->high_util_windows = 0;
				decision = CPU_DECISION_BOOST;

				if (result) {
					result->decision = CPU_DECISION_BOOST;
					result->new_index = new_state;
				}
			}
		} else if (util < config->util_low) {
			// Can reduce power
			state->low_util_windows++;
			state->high_util_windows = 0;

			// Only reduce if: enough windows, cooldown expired, buffer healthy
			bool reduce_ok = (state->low_util_windows >= config->reduce_windows) &&
			                 (state->panic_cooldown == 0) && (current_state_idx > 0) &&
			                 (buffer_fill_percent >= config->min_buffer_for_reduce);

			if (reduce_ok) {
				// Step down one state at a time
				int new_state = current_state_idx - config->max_step_down;
				if (new_state < 0)
					new_state = 0;

				state->target_state = new_state;
				state->low_util_windows = 0;
				decision = CPU_DECISION_REDUCE;

				if (result) {
					result->decision = CPU_DECISION_REDUCE;
					result->new_index = new_state;
				}
			}
		} else {
			// In sweet spot - reset counters
			state->high_util_windows = 0;
			state->low_util_windows = 0;
		}
	} else if (state->use_granular) {
		// Granular mode: linear frequency scaling

		// Decrement panic cooldown
		if (state->panic_cooldown > 0) {
			state->panic_cooldown--;
		}

		if (util > config->util_high) {
			// Need more performance
			state->high_util_windows++;
			state->low_util_windows = 0;

			if (state->high_util_windows >= config->boost_windows && current_idx < max_idx) {
				// Step up by 1 - simple and predictable
				int new_idx = current_idx + 1;
				if (new_idx > max_idx)
					new_idx = max_idx;

				state->target_index = new_idx;
				state->high_util_windows = 0;
				state->panic_grace = CPU_PANIC_GRACE_FRAMES;
				state->grace_underruns = 0;
				decision = CPU_DECISION_BOOST;

				if (result) {
					result->decision = CPU_DECISION_BOOST;
					result->new_index = new_idx;
				}
			}
		} else if (util < config->util_low) {
			// Can reduce power
			state->low_util_windows++;
			state->high_util_windows = 0;

			// Only reduce if: enough windows, panic cooldown expired, buffer healthy
			bool reduce_ok = (state->low_util_windows >= config->reduce_windows) &&
			                 (state->panic_cooldown == 0) && (current_idx > 0) &&
			                 (buffer_fill_percent >= config->min_buffer_for_reduce);

			if (reduce_ok) {
				// Step down by 1 - simple and predictable
				int new_idx = current_idx - 1;

				// Skip blocked frequencies
				while (new_idx >= 0 && state->panic_count[new_idx] >= CPU_PANIC_THRESHOLD) {
					new_idx--;
				}

				if (new_idx >= 0) {
					state->target_index = new_idx;
					state->low_util_windows = 0;
					// No grace period on reduce - if we underrun, frequency is too slow
					decision = CPU_DECISION_REDUCE;

					if (result) {
						result->decision = CPU_DECISION_REDUCE;
						result->new_index = new_idx;
					}
				}
			}
		} else {
			// In sweet spot - reset counters
			state->high_util_windows = 0;
			state->low_util_windows = 0;
		}
	} else {
		// Fallback mode: 3-level scaling

		// Decrement panic cooldown
		if (state->panic_cooldown > 0) {
			state->panic_cooldown--;
		}

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
			state->panic_grace = CPU_PANIC_GRACE_FRAMES;
			state->grace_underruns = 0;
			decision = CPU_DECISION_BOOST;

			if (result) {
				result->decision = CPU_DECISION_BOOST;
				result->new_level = new_level;
			}
		}

		// Reduce if sustained low utilization (and panic cooldown expired, buffer healthy)
		if (state->low_util_windows >= config->reduce_windows && current_level > 0 &&
		    state->panic_cooldown == 0 && buffer_fill_percent >= config->min_buffer_for_reduce) {
			int new_level = current_level - 1;
			state->target_level = new_level;
			state->low_util_windows = 0;
			// No grace period on reduce - if we underrun, frequency is too slow
			decision = CPU_DECISION_REDUCE;

			if (result) {
				result->decision = CPU_DECISION_REDUCE;
				result->new_level = new_level;
			}
		}
	}

	// Track stability for panic count decay
	// If we reached here, no panic happened during this window
	state->stability_streak++;
	if (state->stability_streak >= CPU_STABILITY_DECAY_WINDOWS) {
		// Earned stability - decay panic counts for current freq and above only
		// Being stable at 600MHz proves 800/1000/1200 are fine too, but not 400MHz
		for (int i = current_idx; i < state->freq_count; i++) {
			if (state->panic_count[i] > 0) {
				state->panic_count[i]--;
			}
		}
		state->stability_streak = 0;
	}

	// Reset window counter
	state->frame_count = 0;

	return decision;
}

///////////////////////////////
// Multi-cluster topology functions
///////////////////////////////

// Forward declaration for PWR functions (defined in api.c)
extern int PWR_setCPUGovernor(int policy_id, const char* governor);
extern int PWR_setThreadAffinity(int cpu_mask);

/**
 * Returns the governor string for a given governor type.
 */
static const char* governor_name(CPUGovernor gov) {
	switch (gov) {
	case CPU_GOV_POWERSAVE:
		return "powersave";
	case CPU_GOV_SCHEDUTIL:
		return "schedutil";
	case CPU_GOV_PERFORMANCE:
		return "performance";
	default:
		return "schedutil";
	}
}

void CPU_initTopology(CPUTopology* topology) {
	memset(topology, 0, sizeof(CPUTopology));
}

int CPU_parseCPUList(const char* str, int* cpu_count) {
	if (!str || !cpu_count) {
		if (cpu_count)
			*cpu_count = 0;
		return 0;
	}

	int mask = 0;
	*cpu_count = 0;

	const char* ptr = str;
	while (*ptr) {
		// Skip whitespace and commas
		while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == ',')
			ptr++;
		if (!*ptr)
			break;

		// Parse number
		int start = 0;
		while (*ptr >= '0' && *ptr <= '9') {
			start = start * 10 + (*ptr - '0');
			ptr++;
		}

		int end = start;
		if (*ptr == '-') {
			// Range: "0-3"
			ptr++;
			end = 0;
			while (*ptr >= '0' && *ptr <= '9') {
				end = end * 10 + (*ptr - '0');
				ptr++;
			}
		}

		// Add CPUs to mask
		for (int cpu = start; cpu <= end && cpu < 32; cpu++) {
			if (!(mask & (1 << cpu))) {
				mask |= (1 << cpu);
				(*cpu_count)++;
			}
		}
	}

	return mask;
}

void CPU_classifyClusters(CPUCluster* clusters, int count) {
	if (!clusters || count <= 0)
		return;

	for (int i = 0; i < count; i++) {
		CPUCluster* cluster = &clusters[i];

		if (i == 0) {
			// First cluster (lowest max_khz) is always LITTLE
			cluster->type = CPU_CLUSTER_LITTLE;
		} else if (i == count - 1) {
			// Last cluster might be PRIME if single CPU or significantly faster
			int prev_max = clusters[i - 1].max_khz;
			int freq_gap_percent = 0;
			if (prev_max > 0) {
				freq_gap_percent = ((cluster->max_khz - prev_max) * 100) / prev_max;
			}

			if (cluster->cpu_count == 1 || freq_gap_percent > 10) {
				cluster->type = CPU_CLUSTER_PRIME;
			} else {
				cluster->type = CPU_CLUSTER_BIG;
			}
		} else {
			// Middle clusters are BIG
			cluster->type = CPU_CLUSTER_BIG;
		}
	}
}

void CPU_pickRepresentativeFreqs(const CPUCluster* cluster, int* low_khz, int* mid_khz,
                                 int* high_khz) {
	if (!cluster || cluster->freq_count <= 0) {
		if (low_khz)
			*low_khz = 0;
		if (mid_khz)
			*mid_khz = 0;
		if (high_khz)
			*high_khz = 0;
		return;
	}

	// Low: first frequency
	if (low_khz) {
		*low_khz = cluster->frequencies[0];
	}

	// Mid: middle frequency
	if (mid_khz) {
		int mid_idx = cluster->freq_count / 2;
		*mid_khz = cluster->frequencies[mid_idx];
	}

	// High: last frequency
	if (high_khz) {
		*high_khz = cluster->frequencies[cluster->freq_count - 1];
	}
}

/**
 * Builds a single PerfState entry using governors instead of frequency bounds.
 *
 * @param state PerfState to populate
 * @param cluster_count Number of clusters in topology
 * @param active_cluster_idx Index of the active cluster for this state
 * @param clusters Array of cluster info
 * @param governor_level 0=powersave, 1=schedutil, 2=performance for active cluster
 */
static void build_perf_state(CPUPerfState* state, int cluster_count, int active_cluster_idx,
                             const CPUCluster* clusters, int governor_level) {
	memset(state, 0, sizeof(*state));

	state->active_cluster_idx = active_cluster_idx;
	state->cpu_affinity_mask = 0;

	// Set governors for all clusters
	for (int i = 0; i < cluster_count && i < CPU_MAX_CLUSTERS; i++) {
		const CPUCluster* cluster = &clusters[i];

		if (i == active_cluster_idx) {
			// Active cluster: use the specified governor level
			switch (governor_level) {
			case 0:
				state->cluster_governor[i] = CPU_GOV_POWERSAVE;
				break;
			case 1:
				state->cluster_governor[i] = CPU_GOV_SCHEDUTIL;
				break;
			case 2:
			default:
				state->cluster_governor[i] = CPU_GOV_PERFORMANCE;
				break;
			}
			// Add active cluster to affinity
			state->cpu_affinity_mask |= cluster->cpu_mask;
		} else {
			// Inactive clusters: powersave (let them idle/sleep)
			state->cluster_governor[i] = CPU_GOV_POWERSAVE;
		}
	}
}

void CPU_buildPerfStates(CPUState* state, const CPUConfig* config) {
	(void)config; // Reserved for future configuration

	CPUTopology* topo = &state->topology;

	if (!topo->topology_detected || topo->cluster_count <= 1) {
		// Single-cluster or no topology: don't use PerfState mode
		topo->state_count = 0;
		state->use_topology = 0;
		return;
	}

	int cluster_count = topo->cluster_count;
	int state_idx = 0;

	// Build states for each cluster tier using governors
	// Structure: 3 governor levels per cluster (powersave/schedutil/performance)
	//
	// Dual-cluster (LITTLE + BIG):
	//   0: LITTLE powersave, BIG powersave - lightest workloads
	//   1: LITTLE schedutil, BIG powersave - light workloads (kernel finds sweet spot)
	//   2: LITTLE performance, BIG powersave - moderate workloads
	//   3: BIG powersave, LITTLE powersave - heavier workloads (conserve power)
	//   4: BIG schedutil, LITTLE powersave - heavy workloads (kernel scales)
	//   5: BIG performance, LITTLE powersave - demanding workloads
	//
	// Tri-cluster adds 3 more states for PRIME (6-8)

	for (int cluster_idx = 0; cluster_idx < cluster_count && state_idx < CPU_MAX_PERF_STATES;
	     cluster_idx++) {
		// 3 governor levels per cluster
		for (int gov_level = 0; gov_level < 3 && state_idx < CPU_MAX_PERF_STATES; gov_level++) {
			CPUPerfState* ps = &topo->states[state_idx];
			build_perf_state(ps, cluster_count, cluster_idx, topo->clusters, gov_level);

			// For PRIME cluster, include BIG in affinity (allow scheduler some flexibility)
			if (cluster_idx == cluster_count - 1 && cluster_count >= 3 &&
			    topo->clusters[cluster_idx].type == CPU_CLUSTER_PRIME) {
				// Add BIG cluster(s) to affinity
				for (int i = 1; i < cluster_idx; i++) {
					if (topo->clusters[i].type == CPU_CLUSTER_BIG) {
						ps->cpu_affinity_mask |= topo->clusters[i].cpu_mask;
					}
				}
			}

			state_idx++;
		}
	}

	topo->state_count = state_idx;
	state->use_topology = 1;
	state->target_state = state_idx - 1; // Start at highest (performance on fastest cluster)
	state->current_state = -1; // Not yet applied
}

int CPU_applyPerfState(CPUState* state) {
	CPUTopology* topo = &state->topology;

	if (!state->use_topology || topo->state_count <= 0) {
		return -1;
	}

	int target = state->target_state;
	if (target < 0)
		target = 0;
	if (target >= topo->state_count)
		target = topo->state_count - 1;

	CPUPerfState* ps = &topo->states[target];
	int result = 0;

	// Apply governors to each cluster
	for (int i = 0; i < topo->cluster_count; i++) {
		int policy_id = topo->clusters[i].policy_id;
		const char* gov = governor_name(ps->cluster_governor[i]);

		if (PWR_setCPUGovernor(policy_id, gov) != 0) {
			result = -1;
		}
	}

	// Note: pending_affinity is NOT set here to avoid race conditions.
	// The caller is responsible for setting pending_affinity under mutex
	// after this function returns. See auto_cpu_scaling_thread().

	// Update current state
	state->current_state = target;

	return result;
}
