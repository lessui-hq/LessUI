/**
 * @file test_rate_meter.c
 * @brief Unit tests for rate_meter.c
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/rate_meter.h"

void setUp(void) {}
void tearDown(void) {}

// =============================================================================
// Initialization Tests
// =============================================================================

void test_init_sets_window_size(void) {
	RateMeter meter;
	RateMeter_init(&meter, 20, 1.0f);
	TEST_ASSERT_EQUAL_INT(0, meter.count);
	TEST_ASSERT_EQUAL_INT(20, meter.window_size);
}

void test_init_clamps_window_size_to_max(void) {
	RateMeter meter;
	RateMeter_init(&meter, 100, 1.0f);  // Over max
	TEST_ASSERT_EQUAL_INT(RATE_METER_MAX_WINDOW, meter.window_size);
}

void test_init_clamps_window_size_to_min(void) {
	RateMeter meter;
	RateMeter_init(&meter, 0, 1.0f);
	TEST_ASSERT_EQUAL_INT(1, meter.window_size);
}

void test_init_sets_stability_threshold(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 5.0f);
	TEST_ASSERT_EQUAL_FLOAT(5.0f, meter.stability_threshold);
}

void test_init_null_safe(void) {
	// Should not crash
	RateMeter_init(NULL, 10, 1.0f);
}

// =============================================================================
// Sample Adding Tests
// =============================================================================

void test_add_sample_increments_count(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 1.0f);

	RateMeter_addSample(&meter, 60.0f);
	TEST_ASSERT_EQUAL_INT(1, meter.count);

	RateMeter_addSample(&meter, 60.0f);
	TEST_ASSERT_EQUAL_INT(2, meter.count);
}

void test_add_sample_count_saturates_at_window(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f);

	for (int i = 0; i < 10; i++)
		RateMeter_addSample(&meter, 60.0f);

	TEST_ASSERT_EQUAL_INT(5, meter.count);  // Stops at window size
}

void test_add_sample_ignores_zero_hz(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 1.0f);

	RateMeter_addSample(&meter, 0.0f);
	TEST_ASSERT_EQUAL_INT(0, meter.count);
}

void test_add_sample_ignores_negative_hz(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 1.0f);

	RateMeter_addSample(&meter, -60.0f);
	TEST_ASSERT_EQUAL_INT(0, meter.count);
}

void test_add_sample_null_safe(void) {
	// Should not crash
	RateMeter_addSample(NULL, 60.0f);
}

// =============================================================================
// Statistics Tests
// =============================================================================

void test_get_rate_returns_zero_with_insufficient_samples(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 1.0f);

	TEST_ASSERT_EQUAL_FLOAT(0.0f, RateMeter_getRate(&meter));

	RateMeter_addSample(&meter, 60.0f);
	TEST_ASSERT_EQUAL_FLOAT(0.0f, RateMeter_getRate(&meter));  // Need 3+

	RateMeter_addSample(&meter, 60.0f);
	TEST_ASSERT_EQUAL_FLOAT(0.0f, RateMeter_getRate(&meter));  // Need 3+
}

void test_get_rate_returns_zero_before_stable(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f); // 1 Hz threshold

	// Add values with 4 Hz spread (not stable)
	RateMeter_addSample(&meter, 58.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 62.0f);

	// Median is calculated internally but getRate returns 0 until stable
	TEST_ASSERT_EQUAL_FLOAT(60.0f, meter.median);
	TEST_ASSERT_EQUAL_FLOAT(0.0f, RateMeter_getRate(&meter));  // Not stable yet
}

void test_get_rate_returns_locked_value_when_stable(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f); // 1 Hz threshold

	// Add stable values (spread < 1 Hz)
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.1f);
	RateMeter_addSample(&meter, 60.2f);
	RateMeter_addSample(&meter, 60.3f);
	RateMeter_addSample(&meter, 60.4f);

	TEST_ASSERT_TRUE(RateMeter_isStable(&meter));
	TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.2f, RateMeter_getRate(&meter));
}

void test_get_swing_returns_max_minus_min(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 1.0f);

	RateMeter_addSample(&meter, 58.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 62.0f);

	// Swing = 62 - 58 = 4
	TEST_ASSERT_EQUAL_FLOAT(4.0f, RateMeter_getSwing(&meter));
}

void test_get_swing_with_outliers(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 100.0f);  // High threshold so we don't care about stability

	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 55.0f);  // Outlier low
	RateMeter_addSample(&meter, 65.0f);  // Outlier high

	// Swing = 65 - 55 = 10
	TEST_ASSERT_EQUAL_FLOAT(10.0f, RateMeter_getSwing(&meter));
}

void test_min_max_tracking(void) {
	RateMeter meter;
	RateMeter_init(&meter, 10, 100.0f);

	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 58.0f);
	RateMeter_addSample(&meter, 62.0f);

	TEST_ASSERT_EQUAL_FLOAT(58.0f, meter.min);
	TEST_ASSERT_EQUAL_FLOAT(62.0f, meter.max);
}

// =============================================================================
// Stability Tests
// =============================================================================

void test_is_stable_false_until_window_full(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 10.0f);  // Lenient threshold

	// Add samples within threshold but not full window
	for (int i = 0; i < 4; i++) {
		RateMeter_addSample(&meter, 60.0f);
		TEST_ASSERT_FALSE(RateMeter_isStable(&meter));
	}
}

void test_is_stable_true_when_spread_below_threshold(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f);  // Strict 1 Hz threshold

	// All within 0.5 Hz spread
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.1f);
	RateMeter_addSample(&meter, 60.2f);
	RateMeter_addSample(&meter, 60.3f);
	RateMeter_addSample(&meter, 60.4f);

	TEST_ASSERT_TRUE(RateMeter_isStable(&meter));
}

void test_is_stable_false_when_spread_above_threshold(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f);  // Strict 1 Hz threshold

	// 2 Hz spread
	RateMeter_addSample(&meter, 59.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 61.0f);

	TEST_ASSERT_FALSE(RateMeter_isStable(&meter));
}

void test_stability_can_recover(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f);

	// Start unstable (2 Hz spread)
	RateMeter_addSample(&meter, 59.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 61.0f);
	TEST_ASSERT_FALSE(RateMeter_isStable(&meter));

	// Push out outliers with stable values
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);
	TEST_ASSERT_TRUE(RateMeter_isStable(&meter));
}

// =============================================================================
// Ring Buffer Tests
// =============================================================================

void test_ring_buffer_overwrites_old_values(void) {
	RateMeter meter;
	// Use tight threshold (1.0 Hz) so 58-62 spread (4 Hz) won't be stable
	RateMeter_init(&meter, 5, 1.0f);

	// Fill with 58-62 (spread=4 Hz > threshold=1 Hz, so NOT stable)
	RateMeter_addSample(&meter, 58.0f);
	RateMeter_addSample(&meter, 59.0f);
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 61.0f);
	RateMeter_addSample(&meter, 62.0f);

	TEST_ASSERT_FALSE(RateMeter_isStable(&meter)); // Not stable yet
	TEST_ASSERT_EQUAL_FLOAT(58.0f, meter.min);
	TEST_ASSERT_EQUAL_FLOAT(62.0f, meter.max);

	// Since not stable, new samples will overwrite old ones
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.0f);

	// Now window is [60, 61, 62, 60, 60] - spread=2 Hz still > 1 Hz
	TEST_ASSERT_FALSE(RateMeter_isStable(&meter));
	TEST_ASSERT_EQUAL_FLOAT(60.0f, meter.min);
	TEST_ASSERT_EQUAL_FLOAT(62.0f, meter.max);
}

void test_meter_locks_when_stable(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f); // 1 Hz threshold

	// Add values with 0.4 Hz spread (stable)
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.1f);
	RateMeter_addSample(&meter, 60.2f);
	RateMeter_addSample(&meter, 60.3f);
	RateMeter_addSample(&meter, 60.4f);

	TEST_ASSERT_TRUE(RateMeter_isStable(&meter));
	float first_rate = RateMeter_getRate(&meter);
	TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.2f, first_rate);

	// Add values that would make spread worse - should NOT update locked value
	RateMeter_addSample(&meter, 59.5f); // Would increase spread
	RateMeter_addSample(&meter, 60.8f); // Would increase spread

	// Rate should still be the original locked value
	TEST_ASSERT_EQUAL_FLOAT(first_rate, RateMeter_getRate(&meter));
}

void test_meter_improves_with_better_data(void) {
	RateMeter meter;
	RateMeter_init(&meter, 5, 1.0f); // 1 Hz threshold

	// Start with 0.8 Hz spread (stable but not great)
	RateMeter_addSample(&meter, 60.0f);
	RateMeter_addSample(&meter, 60.2f);
	RateMeter_addSample(&meter, 60.4f);
	RateMeter_addSample(&meter, 60.6f);
	RateMeter_addSample(&meter, 60.8f);

	TEST_ASSERT_TRUE(RateMeter_isStable(&meter));
	float first_spread = meter.locked_spread;
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.8f, first_spread);

	// Add tighter values - should improve the lock
	RateMeter_addSample(&meter, 60.3f);
	RateMeter_addSample(&meter, 60.35f);
	RateMeter_addSample(&meter, 60.4f);
	RateMeter_addSample(&meter, 60.45f);
	RateMeter_addSample(&meter, 60.5f);

	// Spread is now 0.2 Hz - should have updated
	TEST_ASSERT_TRUE(meter.locked_spread < first_spread);
	TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.4f, RateMeter_getRate(&meter));
}

// =============================================================================
// Null Safety Tests
// =============================================================================

void test_get_rate_null_safe(void) {
	TEST_ASSERT_EQUAL_FLOAT(0.0f, RateMeter_getRate(NULL));
}

void test_get_swing_null_safe(void) {
	TEST_ASSERT_EQUAL_FLOAT(0.0f, RateMeter_getSwing(NULL));
}

void test_is_stable_null_safe(void) {
	TEST_ASSERT_FALSE(RateMeter_isStable(NULL));
}

void test_get_sample_count_null_safe(void) {
	TEST_ASSERT_EQUAL_INT(0, RateMeter_getSampleCount(NULL));
}

// =============================================================================
// Realistic Scenarios
// =============================================================================

void test_display_rate_scenario(void) {
	RateMeter meter;
	RateMeter_init(&meter, RATE_METER_DISPLAY_WINDOW, RATE_METER_DISPLAY_STABILITY);

	// Simulate 30 frames of vsync at ~59.7 Hz with some jitter
	float hz_values[] = {
		59.71f, 59.68f, 59.73f, 59.70f, 59.69f,
		59.72f, 59.71f, 59.70f, 59.69f, 59.71f,
		59.70f, 59.72f, 59.69f, 59.71f, 59.70f,
		59.71f, 59.70f, 59.69f, 59.72f, 59.71f,
		59.70f, 59.71f, 59.69f, 59.70f, 59.72f,
		59.71f, 59.70f, 59.69f, 59.71f, 59.70f
	};

	for (int i = 0; i < 30; i++)
		RateMeter_addSample(&meter, hz_values[i]);

	// Should be stable (spread ~0.04 Hz, well under 1.0 Hz)
	TEST_ASSERT_TRUE(RateMeter_isStable(&meter));

	// Median should be around 59.70
	float rate = RateMeter_getRate(&meter);
	TEST_ASSERT_FLOAT_WITHIN(0.02f, 59.70f, rate);
}

void test_audio_rate_scenario_with_jitter(void) {
	RateMeter meter;
	RateMeter_init(&meter, RATE_METER_AUDIO_WINDOW, RATE_METER_AUDIO_STABILITY);

	// Simulate audio measurements with moderate SDL callback jitter
	// ~400 Hz spread, under 500 Hz threshold - WILL be stable
	float hz_values[] = {
		47855.0f, 47900.0f, 47850.0f, 48100.0f, 47880.0f,
		47860.0f, 48050.0f, 47700.0f, 47855.0f, 47900.0f
	};

	for (int i = 0; i < 10; i++)
		RateMeter_addSample(&meter, hz_values[i]);

	// With ~400 Hz spread and 500 Hz threshold, IS stable
	TEST_ASSERT_TRUE(RateMeter_isStable(&meter));

	// getRate returns the locked median
	float rate = RateMeter_getRate(&meter);
	TEST_ASSERT_FLOAT_WITHIN(100.0f, 47870.0f, rate);

	// Swing shows the SDL jitter
	float swing = RateMeter_getSwing(&meter);
	TEST_ASSERT_TRUE(swing > 300.0f);
	TEST_ASSERT_TRUE(swing < 500.0f);
}

// =============================================================================
// Test Runner
// =============================================================================

int main(void) {
	UNITY_BEGIN();

	// Initialization
	RUN_TEST(test_init_sets_window_size);
	RUN_TEST(test_init_clamps_window_size_to_max);
	RUN_TEST(test_init_clamps_window_size_to_min);
	RUN_TEST(test_init_sets_stability_threshold);
	RUN_TEST(test_init_null_safe);

	// Sample adding
	RUN_TEST(test_add_sample_increments_count);
	RUN_TEST(test_add_sample_count_saturates_at_window);
	RUN_TEST(test_add_sample_ignores_zero_hz);
	RUN_TEST(test_add_sample_ignores_negative_hz);
	RUN_TEST(test_add_sample_null_safe);

	// Statistics
	RUN_TEST(test_get_rate_returns_zero_with_insufficient_samples);
	RUN_TEST(test_get_rate_returns_zero_before_stable);
	RUN_TEST(test_get_rate_returns_locked_value_when_stable);
	RUN_TEST(test_get_swing_returns_max_minus_min);
	RUN_TEST(test_get_swing_with_outliers);
	RUN_TEST(test_min_max_tracking);

	// Stability
	RUN_TEST(test_is_stable_false_until_window_full);
	RUN_TEST(test_is_stable_true_when_spread_below_threshold);
	RUN_TEST(test_is_stable_false_when_spread_above_threshold);
	RUN_TEST(test_stability_can_recover);

	// Ring buffer and locking
	RUN_TEST(test_ring_buffer_overwrites_old_values);
	RUN_TEST(test_meter_locks_when_stable);
	RUN_TEST(test_meter_improves_with_better_data);

	// Null safety
	RUN_TEST(test_get_rate_null_safe);
	RUN_TEST(test_get_swing_null_safe);
	RUN_TEST(test_is_stable_null_safe);
	RUN_TEST(test_get_sample_count_null_safe);

	// Realistic scenarios
	RUN_TEST(test_display_rate_scenario);
	RUN_TEST(test_audio_rate_scenario_with_jitter);

	return UNITY_END();
}
