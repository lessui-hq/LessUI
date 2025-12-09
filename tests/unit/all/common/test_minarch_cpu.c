/**
 * test_minarch_cpu.c - Unit tests for auto CPU scaling
 *
 * Tests the CPU frequency scaling algorithm including:
 * - Frequency detection and preset calculation
 * - Nearest frequency index lookup
 * - Utilization-based scaling decisions
 * - Panic path (underrun handling)
 * - Granular vs fallback modes
 * - Frame timing percentile calculation
 *
 * 30 tests organized by functionality.
 */

#include "../../../support/unity/unity.h"
#include "minarch_cpu.h"

#include <string.h>

// Test state and config
static MinArchCPUState state;
static MinArchCPUConfig config;

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	MinArchCPU_initState(&state);
	MinArchCPU_initConfig(&config);
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Config Initialization Tests
///////////////////////////////

void test_initConfig_sets_defaults(void) {
	MinArchCPUConfig c;
	MinArchCPU_initConfig(&c);

	TEST_ASSERT_EQUAL(30, c.window_frames);
	TEST_ASSERT_EQUAL(85, c.util_high);
	TEST_ASSERT_EQUAL(55, c.util_low);
	TEST_ASSERT_EQUAL(2, c.boost_windows);
	TEST_ASSERT_EQUAL(4, c.reduce_windows);
	TEST_ASSERT_EQUAL(300, c.startup_grace);
	TEST_ASSERT_EQUAL(400000, c.min_freq_khz);
	TEST_ASSERT_EQUAL(70, c.target_util);
	TEST_ASSERT_EQUAL(2, c.max_step);
}

void test_initState_zeros_state(void) {
	MinArchCPUState s;
	memset(&s, 0xFF, sizeof(s)); // Fill with garbage
	MinArchCPU_initState(&s);

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
	int result = MinArchCPU_findNearestIndex(NULL, 0, 1000000);
	TEST_ASSERT_EQUAL(0, result);
}

void test_findNearestIndex_exact_match(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	int result = MinArchCPU_findNearestIndex(freqs, 4, 800000);
	TEST_ASSERT_EQUAL(2, result);
}

void test_findNearestIndex_nearest_lower(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	// 750000 is closer to 800000 than 600000
	int result = MinArchCPU_findNearestIndex(freqs, 4, 750000);
	TEST_ASSERT_EQUAL(2, result);
}

void test_findNearestIndex_nearest_higher(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	// 650000 is closer to 600000 than 800000
	int result = MinArchCPU_findNearestIndex(freqs, 4, 650000);
	TEST_ASSERT_EQUAL(1, result);
}

void test_findNearestIndex_below_min(void) {
	int freqs[] = {400000, 600000, 800000};
	int result = MinArchCPU_findNearestIndex(freqs, 3, 100000);
	TEST_ASSERT_EQUAL(0, result);
}

void test_findNearestIndex_above_max(void) {
	int freqs[] = {400000, 600000, 800000};
	int result = MinArchCPU_findNearestIndex(freqs, 3, 2000000);
	TEST_ASSERT_EQUAL(2, result);
}

///////////////////////////////
// detectFrequencies Tests
///////////////////////////////

void test_detectFrequencies_filters_below_minimum(void) {
	int raw[] = {100000, 200000, 300000, 400000, 600000, 800000};
	MinArchCPU_detectFrequencies(&state, &config, raw, 6);

	// Should only keep 400000, 600000, 800000
	TEST_ASSERT_EQUAL(3, state.freq_count);
	TEST_ASSERT_EQUAL(400000, state.frequencies[0]);
	TEST_ASSERT_EQUAL(600000, state.frequencies[1]);
	TEST_ASSERT_EQUAL(800000, state.frequencies[2]);
}

void test_detectFrequencies_enables_granular_mode(void) {
	int raw[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, raw, 4);

	TEST_ASSERT_EQUAL(1, state.use_granular);
	TEST_ASSERT_EQUAL(1, state.frequencies_detected);
}

void test_detectFrequencies_fallback_with_one_freq(void) {
	int raw[] = {800000}; // Only one frequency
	MinArchCPU_detectFrequencies(&state, &config, raw, 1);

	TEST_ASSERT_EQUAL(0, state.use_granular);
	TEST_ASSERT_EQUAL(1, state.freq_count);
}

void test_detectFrequencies_calculates_preset_indices(void) {
	// Frequencies: 400, 600, 800, 1000 MHz
	// Max = 1000000
	// POWERSAVE (55%): 550000 -> nearest is 600000 (index 1)
	// NORMAL (80%): 800000 -> exact match (index 2)
	// PERFORMANCE (100%): 1000000 (index 3)
	int raw[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, raw, 4);

	TEST_ASSERT_EQUAL(1, state.preset_indices[MINARCH_CPU_LEVEL_POWERSAVE]);
	TEST_ASSERT_EQUAL(2, state.preset_indices[MINARCH_CPU_LEVEL_NORMAL]);
	TEST_ASSERT_EQUAL(3, state.preset_indices[MINARCH_CPU_LEVEL_PERFORMANCE]);
}

///////////////////////////////
// reset Tests
///////////////////////////////

void test_reset_clears_monitoring_state(void) {
	state.frame_count = 100;
	state.high_util_windows = 5;
	state.low_util_windows = 3;
	state.panic_cooldown = 8;

	MinArchCPU_reset(&state, &config, 60.0, 0);

	TEST_ASSERT_EQUAL(0, state.frame_count);
	TEST_ASSERT_EQUAL(0, state.high_util_windows);
	TEST_ASSERT_EQUAL(0, state.low_util_windows);
	TEST_ASSERT_EQUAL(0, state.panic_cooldown);
	TEST_ASSERT_EQUAL(0, state.startup_frames);
}

void test_reset_calculates_frame_budget_60fps(void) {
	MinArchCPU_reset(&state, &config, 60.0, 0);
	TEST_ASSERT_EQUAL(16666, state.frame_budget_us); // 1000000/60
}

void test_reset_calculates_frame_budget_50fps(void) {
	MinArchCPU_reset(&state, &config, 50.0, 0);
	TEST_ASSERT_EQUAL(20000, state.frame_budget_us); // 1000000/50
}

void test_reset_defaults_to_60fps_on_zero(void) {
	MinArchCPU_reset(&state, &config, 0.0, 0);
	TEST_ASSERT_EQUAL(16667, state.frame_budget_us);
}

void test_reset_stores_initial_underruns(void) {
	MinArchCPU_reset(&state, &config, 60.0, 42);
	TEST_ASSERT_EQUAL(42, state.last_underrun);
}

///////////////////////////////
// recordFrameTime Tests
///////////////////////////////

void test_recordFrameTime_stores_in_ring_buffer(void) {
	MinArchCPU_recordFrameTime(&state, 15000);
	MinArchCPU_recordFrameTime(&state, 16000);
	MinArchCPU_recordFrameTime(&state, 17000);

	TEST_ASSERT_EQUAL(15000, state.frame_times[0]);
	TEST_ASSERT_EQUAL(16000, state.frame_times[1]);
	TEST_ASSERT_EQUAL(17000, state.frame_times[2]);
	TEST_ASSERT_EQUAL(3, state.frame_time_index);
}

void test_recordFrameTime_wraps_at_buffer_size(void) {
	// Fill buffer
	for (int i = 0; i < MINARCH_CPU_FRAME_BUFFER_SIZE; i++) {
		MinArchCPU_recordFrameTime(&state, 10000 + i);
	}
	// Add one more - should wrap to index 0
	MinArchCPU_recordFrameTime(&state, 99999);

	TEST_ASSERT_EQUAL(99999, state.frame_times[0]);
	TEST_ASSERT_EQUAL(MINARCH_CPU_FRAME_BUFFER_SIZE + 1, state.frame_time_index);
}

///////////////////////////////
// percentile90 Tests
///////////////////////////////

void test_percentile90_empty_returns_zero(void) {
	uint64_t result = MinArchCPU_percentile90(NULL, 0);
	TEST_ASSERT_EQUAL(0, result);
}

void test_percentile90_single_value(void) {
	uint64_t times[] = {12345};
	uint64_t result = MinArchCPU_percentile90(times, 1);
	TEST_ASSERT_EQUAL(12345, result);
}

void test_percentile90_ten_values(void) {
	// Values 1-10, 90th percentile index = (10 * 90) / 100 = 9, sorted[9] = 10
	uint64_t times[] = {5, 3, 8, 1, 9, 2, 7, 4, 10, 6};
	uint64_t result = MinArchCPU_percentile90(times, 10);
	TEST_ASSERT_EQUAL(10, result);
}

void test_percentile90_ignores_outliers(void) {
	// 9 normal values + 1 huge outlier
	// Sorted: 10,11,12,13,14,15,16,17,18,1000000
	// 90% of 10 = 9, so index 9 = 1000000
	// But we want the frame times to show typical load, not spikes
	uint64_t times[] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 1000000};
	uint64_t result = MinArchCPU_percentile90(times, 10);
	// Index 9 (90%) is the outlier
	TEST_ASSERT_EQUAL(1000000, result);
}

///////////////////////////////
// predictFrequency Tests
///////////////////////////////

void test_predictFrequency_boost_case(void) {
	// At 1000MHz with 90% util, want 70% util
	// new_freq = 1000 * 90 / 70 = 1285
	int result = MinArchCPU_predictFrequency(1000000, 90, 70);
	TEST_ASSERT_EQUAL(1285714, result);
}

void test_predictFrequency_reduce_case(void) {
	// At 1000MHz with 40% util, want 70% util
	// new_freq = 1000 * 40 / 70 = 571
	int result = MinArchCPU_predictFrequency(1000000, 40, 70);
	TEST_ASSERT_EQUAL(571428, result);
}

void test_predictFrequency_zero_target_returns_current(void) {
	int result = MinArchCPU_predictFrequency(1000000, 50, 0);
	TEST_ASSERT_EQUAL(1000000, result);
}

///////////////////////////////
// getPresetPercentage Tests
///////////////////////////////

void test_getPresetPercentage_powersave(void) {
	TEST_ASSERT_EQUAL(55, MinArchCPU_getPresetPercentage(MINARCH_CPU_LEVEL_POWERSAVE));
}

void test_getPresetPercentage_normal(void) {
	TEST_ASSERT_EQUAL(80, MinArchCPU_getPresetPercentage(MINARCH_CPU_LEVEL_NORMAL));
}

void test_getPresetPercentage_performance(void) {
	TEST_ASSERT_EQUAL(100, MinArchCPU_getPresetPercentage(MINARCH_CPU_LEVEL_PERFORMANCE));
}

///////////////////////////////
// update Tests - Skip Conditions
///////////////////////////////

void test_update_skips_during_fast_forward(void) {
	MinArchCPUResult result;
	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, true, false, 0, &result);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_SKIP, decision);
	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_SKIP, result.decision);
}

void test_update_skips_during_menu(void) {
	MinArchCPUResult result;
	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, true, 0, &result);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_SKIP, decision);
}

void test_update_skips_during_grace_period(void) {
	config.startup_grace = 300;
	state.startup_frames = 100; // Not yet at grace period

	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_SKIP, decision);
	TEST_ASSERT_EQUAL(101, state.startup_frames); // Incremented
}

///////////////////////////////
// update Tests - Panic Path
///////////////////////////////

void test_update_panic_on_underrun_granular(void) {
	// Setup: granular mode, not at max
	int freqs[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace; // Past grace
	state.target_index = 1; // At 600MHz
	state.last_underrun = 0;

	MinArchCPUResult result;
	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 1, &result);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_PANIC, decision);
	TEST_ASSERT_EQUAL(3, state.target_index); // Boosted by max_step=2 (1+2=3)
	TEST_ASSERT_EQUAL(8, state.panic_cooldown);
}

void test_update_panic_on_underrun_fallback(void) {
	// Setup: fallback mode
	state.use_granular = 0;
	state.startup_frames = config.startup_grace;
	state.target_level = 0; // At powersave
	state.last_underrun = 0;

	MinArchCPUResult result;
	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 1, &result);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_PANIC, decision);
	TEST_ASSERT_EQUAL(2, state.target_level); // Boosted to max
}

void test_update_no_panic_when_at_max(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 3; // Already at max
	state.last_underrun = 0;

	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 1, NULL);

	// Should not panic, just update underrun tracking
	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_NONE, decision);
	TEST_ASSERT_EQUAL(3, state.target_index); // Still at max
}

///////////////////////////////
// update Tests - Window Completion
///////////////////////////////

void test_update_waits_for_full_window(void) {
	int freqs[] = {400000, 600000, 800000};
	MinArchCPU_detectFrequencies(&state, &config, freqs, 3);
	state.startup_frames = config.startup_grace;
	state.frame_count = 10; // Not yet at window_frames (30)

	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_NONE, decision);
	TEST_ASSERT_EQUAL(11, state.frame_count); // Incremented
}

///////////////////////////////
// update Tests - Boost/Reduce
///////////////////////////////

void test_update_boost_on_high_util_granular(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 1;                  // 600MHz
	state.frame_count = config.window_frames - 1;
	state.high_util_windows = config.boost_windows - 1; // About to trigger

	// Add frame times that result in high utilization (~90%)
	state.frame_budget_us = 16667; // 60fps
	for (int i = 0; i < 30; i++) {
		MinArchCPU_recordFrameTime(&state, 15000); // 90% of 16667
	}

	MinArchCPUResult result;
	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 0, &result);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_BOOST, decision);
	TEST_ASSERT_TRUE(state.target_index > 1); // Moved up
}

void test_update_reduce_on_low_util_granular(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 3;                  // 1000MHz
	state.frame_count = config.window_frames - 1;
	state.low_util_windows = config.reduce_windows - 1;
	state.panic_cooldown = 0;

	// Add frame times that result in low utilization (~40%)
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		MinArchCPU_recordFrameTime(&state, 6667); // 40% of 16667
	}

	MinArchCPUResult result;
	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 0, &result);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_REDUCE, decision);
	TEST_ASSERT_TRUE(state.target_index < 3); // Moved down
}

void test_update_no_reduce_during_cooldown(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 3;
	state.frame_count = config.window_frames - 1;
	state.low_util_windows = config.reduce_windows; // Would trigger reduce
	state.panic_cooldown = 5;                       // But in cooldown!

	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		MinArchCPU_recordFrameTime(&state, 6667); // Low util
	}

	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 0, NULL);

	// Should NOT reduce due to cooldown
	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_NONE, decision);
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
		MinArchCPU_recordFrameTime(&state, 15000);
	}

	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_BOOST, decision);
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
		MinArchCPU_recordFrameTime(&state, 6667);
	}

	MinArchCPUDecision decision = MinArchCPU_update(&state, &config, false, false, 0, NULL);

	TEST_ASSERT_EQUAL(MINARCH_CPU_DECISION_REDUCE, decision);
	TEST_ASSERT_EQUAL(1, state.target_level);
}

void test_update_sweet_spot_resets_counters(void) {
	int freqs[] = {400000, 600000, 800000, 1000000};
	MinArchCPU_detectFrequencies(&state, &config, freqs, 4);
	state.startup_frames = config.startup_grace;
	state.target_index = 2;
	state.frame_count = config.window_frames - 1;
	state.high_util_windows = 1;
	state.low_util_windows = 1;

	// Add frame times that result in sweet spot utilization (~70%)
	state.frame_budget_us = 16667;
	for (int i = 0; i < 30; i++) {
		MinArchCPU_recordFrameTime(&state, 11667); // ~70% of 16667
	}

	MinArchCPU_update(&state, &config, false, false, 0, NULL);

	// Counters should be reset
	TEST_ASSERT_EQUAL(0, state.high_util_windows);
	TEST_ASSERT_EQUAL(0, state.low_util_windows);
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
	RUN_TEST(test_detectFrequencies_fallback_with_one_freq);
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

	return UNITY_END();
}
