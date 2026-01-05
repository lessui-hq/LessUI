/**
 * test_player_cpu.c - Unit tests for auto CPU scaling
 *
 * Tests the CPU frequency scaling algorithm including:
 * - Frequency detection and preset calculation
 * - Nearest frequency index lookup
 * - Utilization-based scaling decisions
 * - Panic path (underrun handling)
 * - Granular vs fallback modes
 * - Frame timing percentile calculation
 * - Single-frequency/scaling-disabled scenarios (M17-like devices)
 *
 * 46 tests organized by functionality.
 */

#include "unity.h"
#include "player_cpu.h"

#include <string.h>

///////////////////////////////
// Stubs for API functions called by player_cpu.c
// These allow unit testing without linking api.c
///////////////////////////////

// Track calls for verification in tests
static int stub_governor_calls = 0;
static int stub_last_policy_id = -1;
static char stub_last_governor[32] = {0};
static int stub_affinity_calls = 0;
static int stub_last_affinity_mask = 0;

int PWR_setCPUGovernor(int policy_id, const char* governor) {
	stub_governor_calls++;
	stub_last_policy_id = policy_id;
	if (governor) {
		strncpy(stub_last_governor, governor, sizeof(stub_last_governor) - 1);
		stub_last_governor[sizeof(stub_last_governor) - 1] = '\0';
	}
	return 0; // Success
}

int PWR_setThreadAffinity(int cpu_mask) {
	stub_affinity_calls++;
	stub_last_affinity_mask = cpu_mask;
	return 0; // Success
}

static void reset_stubs(void) {
	stub_governor_calls = 0;
	stub_last_policy_id = -1;
	stub_last_governor[0] = '\0';
	stub_affinity_calls = 0;
	stub_last_affinity_mask = 0;
}

// Test state and config
static PlayerCPUState state;
static PlayerCPUConfig config;

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	PlayerCPU_initState(&state);
	PlayerCPU_initConfig(&config);
	reset_stubs();
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Config Initialization Tests
///////////////////////////////

void test_initConfig_sets_defaults(void) {
	PlayerCPUConfig c;
	PlayerCPU_initConfig(&c);

	// Verify values are sensible (not testing exact defaults)
	TEST_ASSERT_GREATER_THAN(0, c.window_frames);
	TEST_ASSERT_GREATER_THAN(c.util_low, c.util_high);
	TEST_ASSERT_GREATER_THAN(0, c.util_low);
	TEST_ASSERT_LESS_THAN(100, c.util_high);
	TEST_ASSERT_GREATER_THAN(0, c.boost_windows);
	TEST_ASSERT_GREATER_THAN(0, c.reduce_windows);
	TEST_ASSERT_GREATER_THAN(0, c.startup_grace);
	TEST_ASSERT_GREATER_OR_EQUAL(0, c.min_freq_khz); // Can be 0 (panic failsafe handles low freqs)
	TEST_ASSERT_GREATER_THAN(0, c.target_util);
	TEST_ASSERT_LESS_OR_EQUAL(100, c.target_util);
	TEST_ASSERT_GREATER_THAN(0, c.max_step_down);
	TEST_ASSERT_GREATER_THAN(0, c.panic_step_up);
}

void test_initState_zeros_state(void) {
	PlayerCPUState s;
	memset(&s, 0xFF, sizeof(s)); // Fill with garbage
	PlayerCPU_initState(&s);

	TEST_ASSERT_EQUAL(0, s.freq_count);
	TEST_ASSERT_EQUAL(0, s.target_index);
	TEST_ASSERT_EQUAL(0, s.use_granular);
	TEST_ASSERT_EQUAL(0, s.frame_count);
	TEST_ASSERT_EQUAL(16667, s.frame_budget_us); // 60fps default
}

///////////////////////////////
// findNearestIndex Tests
///////////////////////////////

void test_findNearestIndex_empty_array(void) {
	int result = PlayerCPU_findNearestIndex(NULL, 0, 1000000);
	TEST_ASSERT_EQUAL(0, result);
}

void test_findNearestIndex_exact_match(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	int result = PlayerCPU_findNearestIndex(freqs, 4, 800000);
	TEST_ASSERT_EQUAL(2, result);
}

void test_findNearestIndex_nearest_lower(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	// 750000 is closer to 800000 than 600000
	int result = PlayerCPU_findNearestIndex(freqs, 4, 750000);
	TEST_ASSERT_EQUAL(2, result);
}

void test_findNearestIndex_nearest_higher(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	// 650000 is closer to 600000 than 800000
	int result = PlayerCPU_findNearestIndex(freqs, 4, 650000);
	TEST_ASSERT_EQUAL(1, result);
}

void test_findNearestIndex_below_min(void) {
	int freqs[] = {400000, 600000, 800000};
	int result = PlayerCPU_findNearestIndex(freqs, 3, 100000);
	TEST_ASSERT_EQUAL(0, result);
}

void test_findNearestIndex_above_max(void) {
	int freqs[] = {400000, 600000, 800000};
	int result = PlayerCPU_findNearestIndex(freqs, 3, 2000000);
	TEST_ASSERT_EQUAL(2, result);
}

///////////////////////////////
// detectFrequencies Tests
///////////////////////////////

void test_detectFrequencies_filters_below_minimum(void) {
	// Set explicit min_freq_khz to test filtering behavior
	config.min_freq_khz = 400000;
	int raw[] = {100000, 200000, 300000, 400000, 600000, 800000};
	PlayerCPU_detectFrequencies(&state, &config, raw, 6);

	// Should only keep 400000, 600000, 800000 (at or above min_freq_khz)
	TEST_ASSERT_EQUAL(3, state.freq_count);
	TEST_ASSERT_EQUAL(400000, state.frequencies[0]);
	TEST_ASSERT_EQUAL(600000, state.frequencies[1]);
	TEST_ASSERT_EQUAL(800000, state.frequencies[2]);
}

void test_detectFrequencies_enables_granular_mode(void) {
	int raw[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, raw, 4);

	TEST_ASSERT_EQUAL(1, state.use_granular);
	TEST_ASSERT_EQUAL(1, state.frequencies_detected);
}

void test_detectFrequencies_disables_scaling_with_one_freq(void) {
	int raw[] = {800000}; // Only one frequency
	PlayerCPU_detectFrequencies(&state, &config, raw, 1);

	TEST_ASSERT_EQUAL(1, state.scaling_disabled); // Scaling disabled
	TEST_ASSERT_EQUAL(0, state.use_granular);
	TEST_ASSERT_EQUAL(1, state.freq_count);
	TEST_ASSERT_EQUAL(1, state.frequencies_detected);
}

void test_detectFrequencies_disables_scaling_with_zero_freqs(void) {
	PlayerCPU_detectFrequencies(&state, &config, NULL, 0);

	TEST_ASSERT_EQUAL(1, state.scaling_disabled); // Scaling disabled
	TEST_ASSERT_EQUAL(0, state.use_granular);
	TEST_ASSERT_EQUAL(0, state.freq_count);
	TEST_ASSERT_EQUAL(1, state.frequencies_detected);
}

void test_detectFrequencies_enables_scaling_with_multiple_freqs(void) {
	int raw[] = {400000, 600000, 800000};
	PlayerCPU_detectFrequencies(&state, &config, raw, 3);

	TEST_ASSERT_EQUAL(0, state.scaling_disabled); // Scaling enabled
	TEST_ASSERT_EQUAL(1, state.use_granular);
	TEST_ASSERT_EQUAL(3, state.freq_count);
}

void test_detectFrequencies_calculates_preset_indices(void) {
	// Frequencies: 400, 600, 800, 1000 MHz
	// Max = 1000000
	// POWERSAVE (55%): 550000 -> nearest is 600000 (index 1)
	// NORMAL (80%): 800000 -> exact match (index 2)
	// PERFORMANCE (100%): 1000000 (index 3)
	int raw[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, raw, 4);

	TEST_ASSERT_EQUAL(1, state.preset_indices[PLAYER_CPU_LEVEL_POWERSAVE]);
	TEST_ASSERT_EQUAL(2, state.preset_indices[PLAYER_CPU_LEVEL_NORMAL]);
	TEST_ASSERT_EQUAL(3, state.preset_indices[PLAYER_CPU_LEVEL_PERFORMANCE]);
}

///////////////////////////////
// reset Tests
///////////////////////////////

void test_reset_clears_monitoring_state(void) {
	state.frame_count = 100;
	state.high_util_windows = 5;
	state.low_util_windows = 3;
	state.panic_cooldown = 8;

	PlayerCPU_reset(&state, &config, 60.0, 0);

	TEST_ASSERT_EQUAL(0, state.frame_count);
	TEST_ASSERT_EQUAL(0, state.high_util_windows);
	TEST_ASSERT_EQUAL(0, state.low_util_windows);
	TEST_ASSERT_EQUAL(0, state.panic_cooldown);
	TEST_ASSERT_EQUAL(0, state.startup_frames);
}

void test_reset_calculates_frame_budget_60fps(void) {
	PlayerCPU_reset(&state, &config, 60.0, 0);
	TEST_ASSERT_EQUAL(16666, state.frame_budget_us); // 1000000/60
}

void test_reset_calculates_frame_budget_50fps(void) {
	PlayerCPU_reset(&state, &config, 50.0, 0);
	TEST_ASSERT_EQUAL(20000, state.frame_budget_us); // 1000000/50
}

void test_reset_defaults_to_60fps_on_zero(void) {
	PlayerCPU_reset(&state, &config, 0.0, 0);
	TEST_ASSERT_EQUAL(16667, state.frame_budget_us);
}

void test_reset_stores_initial_underruns(void) {
	PlayerCPU_reset(&state, &config, 60.0, 42);
	TEST_ASSERT_EQUAL(42, state.last_underrun);
}

///////////////////////////////
// recordFrameTime Tests
///////////////////////////////

void test_recordFrameTime_stores_in_ring_buffer(void) {
	PlayerCPU_recordFrameTime(&state, 15000);
	PlayerCPU_recordFrameTime(&state, 16000);
	PlayerCPU_recordFrameTime(&state, 17000);

	TEST_ASSERT_EQUAL(15000, state.frame_times[0]);
	TEST_ASSERT_EQUAL(16000, state.frame_times[1]);
	TEST_ASSERT_EQUAL(17000, state.frame_times[2]);
	TEST_ASSERT_EQUAL(3, state.frame_time_index);
}

void test_recordFrameTime_wraps_at_buffer_size(void) {
	// Fill buffer
	for (int i = 0; i < PLAYER_CPU_FRAME_BUFFER_SIZE; i++) {
		PlayerCPU_recordFrameTime(&state, 10000 + i);
	}
	// Add one more - should wrap to index 0
	PlayerCPU_recordFrameTime(&state, 99999);

	TEST_ASSERT_EQUAL(99999, state.frame_times[0]);
	TEST_ASSERT_EQUAL(PLAYER_CPU_FRAME_BUFFER_SIZE + 1, state.frame_time_index);
}

///////////////////////////////
// percentile90 Tests
///////////////////////////////

void test_percentile90_empty_returns_zero(void) {
	uint64_t result = PlayerCPU_percentile90(NULL, 0);
	TEST_ASSERT_EQUAL(0, result);
}

void test_percentile90_single_value(void) {
	uint64_t times[] = {12345};
	uint64_t result = PlayerCPU_percentile90(times, 1);
	TEST_ASSERT_EQUAL(12345, result);
}

void test_percentile90_ten_values(void) {
	// Values 1-10, 90th percentile index = (10 * 90) / 100 = 9, sorted[9] = 10
	uint64_t times[] = {5, 3, 8, 1, 9, 2, 7, 4, 10, 6};
	uint64_t result = PlayerCPU_percentile90(times, 10);
	TEST_ASSERT_EQUAL(10, result);
}

void test_percentile90_ignores_outliers(void) {
	// 9 normal values + 1 huge outlier
	// Sorted: 10,11,12,13,14,15,16,17,18,1000000
	// 90% of 10 = 9, so index 9 = 1000000
	// But we want the frame times to show typical load, not spikes
	uint64_t times[] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 1000000};
	uint64_t result = PlayerCPU_percentile90(times, 10);
	// Index 9 (90%) is the outlier
	TEST_ASSERT_EQUAL(1000000, result);
}

///////////////////////////////
// predictFrequency Tests
///////////////////////////////

void test_predictFrequency_boost_case(void) {
	// At 1000MHz with 90% util, want 70% util
	// new_freq = 1000 * 90 / 70 = 1285
	int result = PlayerCPU_predictFrequency(1000000, 90, 70);
	TEST_ASSERT_EQUAL(1285714, result);
}

void test_predictFrequency_reduce_case(void) {
	// At 1000MHz with 40% util, want 70% util
	// new_freq = 1000 * 40 / 70 = 571
	int result = PlayerCPU_predictFrequency(1000000, 40, 70);
	TEST_ASSERT_EQUAL(571428, result);
}

void test_predictFrequency_zero_target_returns_current(void) {
	int result = PlayerCPU_predictFrequency(1000000, 50, 0);
	TEST_ASSERT_EQUAL(1000000, result);
}

///////////////////////////////
// getPresetPercentage Tests
///////////////////////////////

void test_getPresetPercentage_powersave(void) {
	TEST_ASSERT_EQUAL(55, PlayerCPU_getPresetPercentage(PLAYER_CPU_LEVEL_POWERSAVE));
}

void test_getPresetPercentage_normal(void) {
	TEST_ASSERT_EQUAL(80, PlayerCPU_getPresetPercentage(PLAYER_CPU_LEVEL_NORMAL));
}

void test_getPresetPercentage_performance(void) {
	TEST_ASSERT_EQUAL(100, PlayerCPU_getPresetPercentage(PLAYER_CPU_LEVEL_PERFORMANCE));
}

///////////////////////////////
// update Tests - Skip Conditions
///////////////////////////////

void test_update_skips_during_fast_forward(void) {
	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, true, false, 0, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_SKIP, decision);
	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_SKIP, result.decision);
}

void test_update_skips_during_menu(void) {
	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, true, 0, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_SKIP, decision);
}

void test_update_skips_during_grace_period(void) {
	config.startup_grace = 300;
	state.startup_frames = 100; // Not yet at grace period

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_SKIP, decision);
	TEST_ASSERT_EQUAL(101, state.startup_frames); // Incremented
}

void test_update_skips_when_scaling_disabled(void) {
	// Simulate M17-like single-frequency device
	int raw[] = {1200000}; // Only one frequency (like M17)
	PlayerCPU_detectFrequencies(&state, &config, raw, 1);

	TEST_ASSERT_EQUAL(1, state.scaling_disabled); // Pre-condition: scaling disabled

	// Even with valid state and frame times, should skip
	state.startup_frames = config.startup_grace;
	state.frame_count = config.window_frames - 1;
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 15000); // High utilization
	}

	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_SKIP, decision);
	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_SKIP, result.decision);
}

void test_update_skips_when_no_frequencies(void) {
	// Edge case: no frequencies at all
	PlayerCPU_detectFrequencies(&state, &config, NULL, 0);

	TEST_ASSERT_EQUAL(1, state.scaling_disabled);

	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_SKIP, decision);
}

///////////////////////////////
// update Tests - Panic Path
///////////////////////////////

void test_update_panic_on_underrun_granular(void) {
	// Setup: granular mode, not at max
	int freqs[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace; // Past grace
	state.target_index = 1; // At 600MHz
	state.last_underrun = 0;

	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 1, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_PANIC, decision);
	TEST_ASSERT_EQUAL(3, state.target_index); // Boosted by panic_step_up=2 (1+2=3)
	TEST_ASSERT_EQUAL(8, state.panic_cooldown);
}

void test_update_panic_on_underrun_fallback(void) {
	// Setup: fallback mode
	state.use_granular = 0;
	state.startup_frames = config.startup_grace;
	state.target_level = 0; // At powersave
	state.last_underrun = 0;

	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 1, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_PANIC, decision);
	TEST_ASSERT_EQUAL(2, state.target_level); // Boosted to max
}

void test_update_no_panic_when_at_max(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 3; // Already at max
	state.last_underrun = 0;

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 1, NULL);

	// Should not panic, just update underrun tracking
	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_NONE, decision);
	TEST_ASSERT_EQUAL(3, state.target_index); // Still at max
}

///////////////////////////////
// update Tests - Window Completion
///////////////////////////////

void test_update_waits_for_full_window(void) {
	int freqs[] = {400000, 600000, 800000};
	PlayerCPU_detectFrequencies(&state, &config, freqs, 3);
	state.startup_frames = config.startup_grace;
	state.frame_count = 10; // Not yet at window_frames (30)

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_NONE, decision);
	TEST_ASSERT_EQUAL(11, state.frame_count); // Incremented
}

///////////////////////////////
// update Tests - Boost/Reduce
///////////////////////////////

void test_update_boost_on_high_util_granular(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 1;                  // 600MHz
	state.frame_count = config.window_frames - 1;
	state.high_util_windows = config.boost_windows - 1; // About to trigger

	// Add frame times that result in high utilization (~90%)
	state.frame_budget_us = 16667; // 60fps
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 15000); // 90% of 16667
	}

	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_BOOST, decision);
	TEST_ASSERT_TRUE(state.target_index > 1); // Moved up
}

void test_update_reduce_on_low_util_granular(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 3;                  // 1000MHz
	state.frame_count = config.window_frames - 1;
	state.low_util_windows = config.reduce_windows - 1;
	state.panic_cooldown = 0;

	// Add frame times that result in low utilization (~40%)
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 6667); // 40% of 16667
	}

	PlayerCPUResult result;
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, &result);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_REDUCE, decision);
	TEST_ASSERT_TRUE(state.target_index < 3); // Moved down
}

void test_update_no_reduce_during_cooldown(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 3;
	state.frame_count = config.window_frames - 1;
	state.low_util_windows = config.reduce_windows; // Would trigger reduce
	state.panic_cooldown = 5;                       // But in cooldown!

	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 6667); // Low util
	}

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	// Should NOT reduce due to cooldown
	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_NONE, decision);
	TEST_ASSERT_EQUAL(3, state.target_index);
	TEST_ASSERT_EQUAL(4, state.panic_cooldown); // Decremented
}

void test_update_boost_fallback_mode(void) {
	state.use_granular = 0;
	state.startup_frames = config.startup_grace;
	state.target_level = 0;
	state.frame_count = config.window_frames - 1;
	state.high_util_windows = config.boost_windows - 1;

	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 15000);
	}

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_BOOST, decision);
	TEST_ASSERT_EQUAL(1, state.target_level);
}

void test_update_reduce_fallback_mode(void) {
	state.use_granular = 0;
	state.startup_frames = config.startup_grace;
	state.target_level = 2;
	state.frame_count = config.window_frames - 1;
	state.low_util_windows = config.reduce_windows - 1;

	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 6667);
	}

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_REDUCE, decision);
	TEST_ASSERT_EQUAL(1, state.target_level);
}

void test_update_sweet_spot_resets_counters(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	PlayerCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 2;
	state.frame_count = config.window_frames - 1;
	state.high_util_windows = 1;
	state.low_util_windows = 1;

	// Add frame times that result in sweet spot utilization (~70%)
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 11667); // ~70% of 16667
	}

	PlayerCPU_update(&state, &config, false, false, 0, NULL);

	// Counters should be reset
	TEST_ASSERT_EQUAL(0, state.high_util_windows);
	TEST_ASSERT_EQUAL(0, state.low_util_windows);
}

///////////////////////////////
// Topology Tests
///////////////////////////////

void test_initTopology_zeros_topology(void) {
	PlayerCPUTopology t;
	memset(&t, 0xFF, sizeof(t)); // Fill with garbage
	PlayerCPU_initTopology(&t);

	TEST_ASSERT_EQUAL(0, t.cluster_count);
	TEST_ASSERT_EQUAL(0, t.state_count);
	TEST_ASSERT_EQUAL(0, t.topology_detected);
}

void test_parseCPUList_single_cpu(void) {
	int count = 0;
	int mask = PlayerCPU_parseCPUList("0", &count);
	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL(0x1, mask); // CPU 0
}

void test_parseCPUList_range(void) {
	int count = 0;
	int mask = PlayerCPU_parseCPUList("0-3", &count);
	TEST_ASSERT_EQUAL(4, count);
	TEST_ASSERT_EQUAL(0xF, mask); // CPUs 0-3
}

void test_parseCPUList_mixed(void) {
	int count = 0;
	int mask = PlayerCPU_parseCPUList("0-3,7", &count);
	TEST_ASSERT_EQUAL(5, count);
	TEST_ASSERT_EQUAL(0x8F, mask); // CPUs 0-3 and 7
}

void test_parseCPUList_single_high_cpu(void) {
	int count = 0;
	int mask = PlayerCPU_parseCPUList("7", &count);
	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL(0x80, mask); // CPU 7
}

void test_parseCPUList_empty_string(void) {
	int count = 0;
	int mask = PlayerCPU_parseCPUList("", &count);
	TEST_ASSERT_EQUAL(0, count);
	TEST_ASSERT_EQUAL(0, mask);
}

void test_classifyClusters_single_is_little(void) {
	PlayerCPUCluster clusters[1];
	clusters[0].max_khz = 1800000;
	clusters[0].cpu_count = 4;

	PlayerCPU_classifyClusters(clusters, 1);

	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_LITTLE, clusters[0].type);
}

void test_classifyClusters_dual_little_big(void) {
	PlayerCPUCluster clusters[2];
	// Sorted by max_khz ascending
	// Use frequencies with <10% gap to get BIG (not PRIME) classification
	clusters[0].max_khz = 1800000;
	clusters[0].cpu_count = 4;
	clusters[1].max_khz = 1900000; // ~5.5% higher, should be BIG
	clusters[1].cpu_count = 4;

	PlayerCPU_classifyClusters(clusters, 2);

	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_LITTLE, clusters[0].type);
	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_BIG, clusters[1].type);
}

void test_classifyClusters_tri_little_big_prime(void) {
	PlayerCPUCluster clusters[3];
	// SD865-like: Silver, Gold, Prime
	clusters[0].max_khz = 1800000;
	clusters[0].cpu_count = 4;
	clusters[1].max_khz = 2420000;
	clusters[1].cpu_count = 3;
	clusters[2].max_khz = 2840000;
	clusters[2].cpu_count = 1; // Prime is single-core

	PlayerCPU_classifyClusters(clusters, 3);

	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_LITTLE, clusters[0].type);
	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_BIG, clusters[1].type);
	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_PRIME, clusters[2].type);
}

void test_classifyClusters_dual_prime_by_frequency_gap(void) {
	PlayerCPUCluster clusters[2];
	// >10% frequency gap makes highest PRIME even with multiple cores
	clusters[0].max_khz = 1800000;
	clusters[0].cpu_count = 4;
	clusters[1].max_khz = 2200000; // >10% higher
	clusters[1].cpu_count = 4;

	PlayerCPU_classifyClusters(clusters, 2);

	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_LITTLE, clusters[0].type);
	TEST_ASSERT_EQUAL(PLAYER_CPU_CLUSTER_PRIME, clusters[1].type);
}

void test_pickRepresentativeFreqs_single_freq(void) {
	PlayerCPUCluster c;
	c.frequencies[0] = 1800000;
	c.freq_count = 1;

	int low, mid, high;
	PlayerCPU_pickRepresentativeFreqs(&c, &low, &mid, &high);

	TEST_ASSERT_EQUAL(1800000, low);
	TEST_ASSERT_EQUAL(1800000, mid);
	TEST_ASSERT_EQUAL(1800000, high);
}

void test_pickRepresentativeFreqs_multiple_freqs(void) {
	PlayerCPUCluster c;
	c.frequencies[0] = 400000;
	c.frequencies[1] = 800000;
	c.frequencies[2] = 1200000;
	c.frequencies[3] = 1600000;
	c.frequencies[4] = 2000000;
	c.freq_count = 5;

	int low, mid, high;
	PlayerCPU_pickRepresentativeFreqs(&c, &low, &mid, &high);

	TEST_ASSERT_EQUAL(400000, low);
	TEST_ASSERT_EQUAL(1200000, mid); // freqs[5/2] = freqs[2]
	TEST_ASSERT_EQUAL(2000000, high);
}

// Helper to set up a dual-cluster topology
static void setup_dual_cluster_topology(PlayerCPUState* s) {
	s->topology.cluster_count = 2;
	s->topology.topology_detected = 1; // Mark as detected so buildPerfStates works

	// LITTLE cluster (policy 0, CPUs 0-3)
	s->topology.clusters[0].policy_id = 0;
	s->topology.clusters[0].cpu_mask = 0x0F;
	s->topology.clusters[0].cpu_count = 4;
	s->topology.clusters[0].frequencies[0] = 600000;
	s->topology.clusters[0].frequencies[1] = 1200000;
	s->topology.clusters[0].frequencies[2] = 1800000;
	s->topology.clusters[0].freq_count = 3;
	s->topology.clusters[0].min_khz = 600000;
	s->topology.clusters[0].max_khz = 1800000;
	s->topology.clusters[0].type = PLAYER_CPU_CLUSTER_LITTLE;

	// BIG cluster (policy 4, CPUs 4-7)
	s->topology.clusters[1].policy_id = 4;
	s->topology.clusters[1].cpu_mask = 0xF0;
	s->topology.clusters[1].cpu_count = 4;
	s->topology.clusters[1].frequencies[0] = 800000;
	s->topology.clusters[1].frequencies[1] = 1600000;
	s->topology.clusters[1].frequencies[2] = 2400000;
	s->topology.clusters[1].freq_count = 3;
	s->topology.clusters[1].min_khz = 800000;
	s->topology.clusters[1].max_khz = 2400000;
	s->topology.clusters[1].type = PLAYER_CPU_CLUSTER_BIG;
}

void test_buildPerfStates_dual_cluster_creates_six_states(void) {
	setup_dual_cluster_topology(&state);

	PlayerCPU_buildPerfStates(&state, &config);

	TEST_ASSERT_EQUAL(6, state.topology.state_count);
	TEST_ASSERT_EQUAL(1, state.use_topology);
}

void test_buildPerfStates_dual_cluster_state_progression(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	// State 0: LITTLE powersave, BIG powersave, affinity = LITTLE
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_POWERSAVE, state.topology.states[0].cluster_governor[0]);
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_POWERSAVE, state.topology.states[0].cluster_governor[1]);
	TEST_ASSERT_EQUAL(0, state.topology.states[0].active_cluster_idx);
	TEST_ASSERT_EQUAL(0x0F, state.topology.states[0].cpu_affinity_mask); // LITTLE CPUs

	// State 1: LITTLE schedutil, BIG powersave
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_SCHEDUTIL, state.topology.states[1].cluster_governor[0]);
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_POWERSAVE, state.topology.states[1].cluster_governor[1]);

	// State 2: LITTLE performance, BIG powersave
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_PERFORMANCE, state.topology.states[2].cluster_governor[0]);
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_POWERSAVE, state.topology.states[2].cluster_governor[1]);

	// State 3: BIG powersave, LITTLE powersave, affinity = BIG
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_POWERSAVE, state.topology.states[3].cluster_governor[0]);
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_POWERSAVE, state.topology.states[3].cluster_governor[1]);
	TEST_ASSERT_EQUAL(1, state.topology.states[3].active_cluster_idx);
	TEST_ASSERT_EQUAL(0xF0, state.topology.states[3].cpu_affinity_mask); // BIG CPUs

	// State 5: BIG performance (highest state)
	TEST_ASSERT_EQUAL(PLAYER_CPU_GOV_PERFORMANCE, state.topology.states[5].cluster_governor[1]);
}

void test_buildPerfStates_single_cluster_skips_topology(void) {
	state.topology.cluster_count = 1;

	PlayerCPU_buildPerfStates(&state, &config);

	TEST_ASSERT_EQUAL(0, state.topology.state_count);
	TEST_ASSERT_EQUAL(0, state.use_topology);
}

void test_applyPerfState_calls_governors(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.target_state = 0;
	state.current_state = -1;

	int result = PlayerCPU_applyPerfState(&state);

	TEST_ASSERT_EQUAL(0, result);
	// Should call governor for each cluster (2 clusters = 2 calls)
	TEST_ASSERT_EQUAL(2, stub_governor_calls);
}

void test_applyPerfState_does_not_set_affinity_directly(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.target_state = 0;
	state.current_state = -1;
	state.pending_affinity = 0;

	PlayerCPU_applyPerfState(&state);

	// applyPerfState should NOT set pending_affinity or call PWR_setThreadAffinity
	// The caller is responsible for setting pending_affinity under mutex
	TEST_ASSERT_EQUAL(0, state.pending_affinity);
	TEST_ASSERT_EQUAL(0, stub_affinity_calls);
}

void test_applyPerfState_updates_current_state(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.target_state = 3;
	state.current_state = -1;

	PlayerCPU_applyPerfState(&state);

	TEST_ASSERT_EQUAL(3, state.current_state);
}

void test_update_topology_boost_increments_state(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.startup_frames = config.startup_grace;
	state.target_state = 2;
	state.current_state = 2;
	state.frame_count = config.window_frames - 1;
	state.high_util_windows = config.boost_windows - 1;

	// High utilization frames (>85%)
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 15000); // ~90%
	}

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_BOOST, decision);
	TEST_ASSERT_EQUAL(3, state.target_state);
}

void test_update_topology_reduce_decrements_state(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.startup_frames = config.startup_grace;
	state.target_state = 4;
	state.current_state = 4;
	state.frame_count = config.window_frames - 1;
	state.low_util_windows = config.reduce_windows - 1;

	// Low utilization frames (<55%)
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 6667); // ~40%
	}

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_REDUCE, decision);
	TEST_ASSERT_LESS_THAN(4, state.target_state);
}

void test_update_topology_panic_jumps_states(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.startup_frames = config.startup_grace;
	state.target_state = 1;
	state.current_state = 1;
	state.last_underrun = 0;

	// Underrun detected
	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 1, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_PANIC, decision);
	TEST_ASSERT_GREATER_THAN(1, state.target_state);
}

void test_update_topology_no_boost_at_max_state(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.startup_frames = config.startup_grace;
	state.target_state = 5; // Already at max
	state.current_state = 5;
	state.frame_count = config.window_frames - 1;
	state.high_util_windows = config.boost_windows - 1;

	// High utilization frames
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 15000);
	}

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_NONE, decision);
	TEST_ASSERT_EQUAL(5, state.target_state);
}

void test_update_topology_no_reduce_at_min_state(void) {
	setup_dual_cluster_topology(&state);
	PlayerCPU_buildPerfStates(&state, &config);

	state.startup_frames = config.startup_grace;
	state.target_state = 0; // Already at min
	state.current_state = 0;
	state.frame_count = config.window_frames - 1;
	state.low_util_windows = config.reduce_windows - 1;

	// Low utilization frames
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		PlayerCPU_recordFrameTime(&state, 6667);
	}

	PlayerCPUDecision decision = PlayerCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(PLAYER_CPU_DECISION_NONE, decision);
	TEST_ASSERT_EQUAL(0, state.target_state);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Config/State init
	RUN_TEST(test_initConfig_sets_defaults);
	RUN_TEST(test_initState_zeros_state);

	// findNearestIndex
	RUN_TEST(test_findNearestIndex_empty_array);
	RUN_TEST(test_findNearestIndex_exact_match);
	RUN_TEST(test_findNearestIndex_nearest_lower);
	RUN_TEST(test_findNearestIndex_nearest_higher);
	RUN_TEST(test_findNearestIndex_below_min);
	RUN_TEST(test_findNearestIndex_above_max);

	// detectFrequencies
	RUN_TEST(test_detectFrequencies_filters_below_minimum);
	RUN_TEST(test_detectFrequencies_enables_granular_mode);
	RUN_TEST(test_detectFrequencies_disables_scaling_with_one_freq);
	RUN_TEST(test_detectFrequencies_disables_scaling_with_zero_freqs);
	RUN_TEST(test_detectFrequencies_enables_scaling_with_multiple_freqs);
	RUN_TEST(test_detectFrequencies_calculates_preset_indices);

	// reset
	RUN_TEST(test_reset_clears_monitoring_state);
	RUN_TEST(test_reset_calculates_frame_budget_60fps);
	RUN_TEST(test_reset_calculates_frame_budget_50fps);
	RUN_TEST(test_reset_defaults_to_60fps_on_zero);
	RUN_TEST(test_reset_stores_initial_underruns);

	// recordFrameTime
	RUN_TEST(test_recordFrameTime_stores_in_ring_buffer);
	RUN_TEST(test_recordFrameTime_wraps_at_buffer_size);

	// percentile90
	RUN_TEST(test_percentile90_empty_returns_zero);
	RUN_TEST(test_percentile90_single_value);
	RUN_TEST(test_percentile90_ten_values);
	RUN_TEST(test_percentile90_ignores_outliers);

	// predictFrequency
	RUN_TEST(test_predictFrequency_boost_case);
	RUN_TEST(test_predictFrequency_reduce_case);
	RUN_TEST(test_predictFrequency_zero_target_returns_current);

	// getPresetPercentage
	RUN_TEST(test_getPresetPercentage_powersave);
	RUN_TEST(test_getPresetPercentage_normal);
	RUN_TEST(test_getPresetPercentage_performance);

	// update - skip conditions
	RUN_TEST(test_update_skips_during_fast_forward);
	RUN_TEST(test_update_skips_during_menu);
	RUN_TEST(test_update_skips_during_grace_period);
	RUN_TEST(test_update_skips_when_scaling_disabled);
	RUN_TEST(test_update_skips_when_no_frequencies);

	// update - panic
	RUN_TEST(test_update_panic_on_underrun_granular);
	RUN_TEST(test_update_panic_on_underrun_fallback);
	RUN_TEST(test_update_no_panic_when_at_max);

	// update - window
	RUN_TEST(test_update_waits_for_full_window);

	// update - boost/reduce
	RUN_TEST(test_update_boost_on_high_util_granular);
	RUN_TEST(test_update_reduce_on_low_util_granular);
	RUN_TEST(test_update_no_reduce_during_cooldown);
	RUN_TEST(test_update_boost_fallback_mode);
	RUN_TEST(test_update_reduce_fallback_mode);
	RUN_TEST(test_update_sweet_spot_resets_counters);

	// Topology - initialization
	RUN_TEST(test_initTopology_zeros_topology);

	// Topology - CPU list parsing
	RUN_TEST(test_parseCPUList_single_cpu);
	RUN_TEST(test_parseCPUList_range);
	RUN_TEST(test_parseCPUList_mixed);
	RUN_TEST(test_parseCPUList_single_high_cpu);
	RUN_TEST(test_parseCPUList_empty_string);

	// Topology - cluster classification
	RUN_TEST(test_classifyClusters_single_is_little);
	RUN_TEST(test_classifyClusters_dual_little_big);
	RUN_TEST(test_classifyClusters_tri_little_big_prime);
	RUN_TEST(test_classifyClusters_dual_prime_by_frequency_gap);

	// Topology - representative frequencies
	RUN_TEST(test_pickRepresentativeFreqs_single_freq);
	RUN_TEST(test_pickRepresentativeFreqs_multiple_freqs);

	// Topology - PerfState building
	RUN_TEST(test_buildPerfStates_dual_cluster_creates_six_states);
	RUN_TEST(test_buildPerfStates_dual_cluster_state_progression);
	RUN_TEST(test_buildPerfStates_single_cluster_skips_topology);

	// Topology - PerfState application
	RUN_TEST(test_applyPerfState_calls_governors);
	RUN_TEST(test_applyPerfState_does_not_set_affinity_directly);
	RUN_TEST(test_applyPerfState_updates_current_state);

	// Topology - update decisions
	RUN_TEST(test_update_topology_boost_increments_state);
	RUN_TEST(test_update_topology_reduce_decrements_state);
	RUN_TEST(test_update_topology_panic_jumps_states);
	RUN_TEST(test_update_topology_no_boost_at_max_state);
	RUN_TEST(test_update_topology_no_reduce_at_min_state);

	return UNITY_END();
}
