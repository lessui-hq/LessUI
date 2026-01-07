/**
 * test_sync_manager.c - Unit tests for audio/video sync mode management
 *
 * Tests the runtime-adaptive sync mode switching including:
 * - Initialization (starts in AUDIO_CLOCK mode)
 * - Vsync measurement with EMA smoothing
 * - Mode switching based on measured Hz
 * - Drift detection and fallback to AUDIO_CLOCK
 * - shouldRunCore (always returns true)
 * - shouldUseRateControl/shouldBlockAudio based on mode
 */

#include "unity.h"
#include "sync_manager.h"
#include <math.h>
#include <stdarg.h>

// Stub for getMicroseconds - returns controllable time for vsync measurement
static uint64_t mock_time_us = 0;
uint64_t getMicroseconds(void) {
	return mock_time_us;
}

// Stub for LOG_info - suppress output during tests
void LOG_info(const char* fmt, ...) {
	(void)fmt;
}

// Test state
static SyncManager manager;

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	SyncManager_init(&manager, 60.0, 60.0);
	mock_time_us = 1000000; // Start at 1 second
}

void tearDown(void) {
	// No cleanup needed
}

///////////////////////////////
// Initialization Tests
///////////////////////////////

void test_init_starts_in_audio_clock_mode(void) {
	TEST_ASSERT_EQUAL(SYNC_MODE_AUDIO_CLOCK, SyncManager_getMode(&manager));
}

void test_init_stores_game_fps(void) {
	SyncManager_init(&manager, 59.94, 60.0);
	TEST_ASSERT_EQUAL_FLOAT(59.94, manager.game_fps);
}

void test_init_stores_display_hz(void) {
	SyncManager_init(&manager, 60.0, 72.0);
	TEST_ASSERT_EQUAL_FLOAT(72.0, manager.display_hz);
}

void test_init_with_zero_hz_defaults_to_60(void) {
	SyncManager_init(&manager, 60.0, 0.0);
	TEST_ASSERT_EQUAL_FLOAT(60.0, manager.display_hz);
}

void test_init_measurement_not_stable(void) {
	TEST_ASSERT_FALSE(SyncManager_isMeasurementStable(&manager));
}

///////////////////////////////
// Vsync Measurement Tests
///////////////////////////////

void test_first_vsync_just_records_timestamp(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);
	TEST_ASSERT_EQUAL_UINT64(1000000, manager.last_vsync_time);
	TEST_ASSERT_EQUAL_FLOAT(0.0, manager.measured_hz);
}

void test_second_vsync_calculates_hz(void) {
	// First call
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Second call - 16.667ms later (60Hz)
	mock_time_us = 1016667;
	SyncManager_recordVsync(&manager);

	// Should have initial Hz measurement (not averaged yet, first sample)
	TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, manager.measured_hz);
}

void test_rejects_outlier_too_low(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// 30Hz (too low, < 50Hz threshold)
	mock_time_us = 1033333;
	SyncManager_recordVsync(&manager);

	// Should be rejected, no measurement
	TEST_ASSERT_EQUAL_FLOAT(0.0, manager.measured_hz);
	TEST_ASSERT_EQUAL(0, manager.measurement_samples);
}

void test_rejects_outlier_too_high(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// 150Hz (too high, > 120Hz threshold)
	mock_time_us = 1006667;
	SyncManager_recordVsync(&manager);

	// Should be rejected
	TEST_ASSERT_EQUAL_FLOAT(0.0, manager.measured_hz);
}

void test_rejects_zero_interval(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Same timestamp (division by zero protection)
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Should be rejected
	TEST_ASSERT_EQUAL_FLOAT(0.0, manager.measured_hz);
}

///////////////////////////////
// Mode Switching Tests
///////////////////////////////

void test_switches_to_vsync_when_compatible(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Simulate 120 samples at 60Hz (compatible with 60fps game)
	for (int i = 0; i < 120; i++) {
		mock_time_us += 16667; // 60Hz interval
		SyncManager_recordVsync(&manager);
	}

	// Should switch to VSYNC mode (< 1% mismatch)
	TEST_ASSERT_EQUAL(SYNC_MODE_VSYNC, SyncManager_getMode(&manager));
	TEST_ASSERT_TRUE(SyncManager_isMeasurementStable(&manager));
}

void test_stays_in_audio_clock_when_incompatible(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Simulate 120 samples at 68Hz (incompatible with 60fps game, 13% mismatch)
	for (int i = 0; i < 120; i++) {
		mock_time_us += 14706; // 68Hz interval
		SyncManager_recordVsync(&manager);
	}

	// Should stay in AUDIO_CLOCK mode (> 1% mismatch)
	TEST_ASSERT_EQUAL(SYNC_MODE_AUDIO_CLOCK, SyncManager_getMode(&manager));
	TEST_ASSERT_TRUE(SyncManager_isMeasurementStable(&manager));
}

void test_measurement_stable_after_120_samples(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Need 120 samples after initial baseline
	for (int i = 0; i < 120; i++) {
		mock_time_us += 16667;
		SyncManager_recordVsync(&manager);
		if (i < 119) {
			TEST_ASSERT_FALSE(SyncManager_isMeasurementStable(&manager));
		}
	}

	// After 120 samples - now stable
	TEST_ASSERT_TRUE(SyncManager_isMeasurementStable(&manager));
}

void test_drift_detection_switches_back_to_audio_clock(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Initial measurement at 60Hz - should switch to VSYNC
	for (int i = 0; i < 120; i++) {
		mock_time_us += 16667;
		SyncManager_recordVsync(&manager);
	}
	TEST_ASSERT_EQUAL(SYNC_MODE_VSYNC, SyncManager_getMode(&manager));

	// Drift to 65Hz over 300 frames (> 1% mismatch)
	for (int i = 0; i < 300; i++) {
		mock_time_us += 15385; // 65Hz interval
		SyncManager_recordVsync(&manager);
	}

	// Should detect drift and switch back to AUDIO_CLOCK
	TEST_ASSERT_EQUAL(SYNC_MODE_AUDIO_CLOCK, SyncManager_getMode(&manager));
}

///////////////////////////////
// API Tests
///////////////////////////////

void test_should_run_core_always_returns_true(void) {
	TEST_ASSERT_TRUE(SyncManager_shouldRunCore(&manager));

	// Even after switching to VSYNC
	manager.mode = SYNC_MODE_VSYNC;
	TEST_ASSERT_TRUE(SyncManager_shouldRunCore(&manager));
}

void test_should_use_rate_control_in_vsync_mode(void) {
	manager.mode = SYNC_MODE_VSYNC;
	TEST_ASSERT_TRUE(SyncManager_shouldUseRateControl(&manager));
}

void test_should_not_use_rate_control_in_audio_clock(void) {
	manager.mode = SYNC_MODE_AUDIO_CLOCK;
	TEST_ASSERT_FALSE(SyncManager_shouldUseRateControl(&manager));
}

void test_should_block_audio_in_audio_clock_mode(void) {
	manager.mode = SYNC_MODE_AUDIO_CLOCK;
	TEST_ASSERT_TRUE(SyncManager_shouldBlockAudio(&manager));
}

void test_should_not_block_audio_in_vsync_mode(void) {
	manager.mode = SYNC_MODE_VSYNC;
	TEST_ASSERT_FALSE(SyncManager_shouldBlockAudio(&manager));
}

void test_get_measured_hz_returns_zero_when_not_stable(void) {
	TEST_ASSERT_EQUAL_FLOAT(0.0, SyncManager_getMeasuredHz(&manager));
}

void test_get_measured_hz_returns_value_when_stable(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// 120 samples at 60Hz
	for (int i = 0; i < 120; i++) {
		mock_time_us += 16667;
		SyncManager_recordVsync(&manager);
	}

	double measured = SyncManager_getMeasuredHz(&manager);
	TEST_ASSERT_FLOAT_WITHIN(0.5, 60.0, measured);
}

void test_mode_name_audio_clock(void) {
	TEST_ASSERT_EQUAL_STRING("Audio Clock", SyncManager_getModeName(SYNC_MODE_AUDIO_CLOCK));
}

void test_mode_name_vsync(void) {
	TEST_ASSERT_EQUAL_STRING("Vsync", SyncManager_getModeName(SYNC_MODE_VSYNC));
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_ema_smooths_noisy_measurements(void) {
	mock_time_us = 1000000;
	SyncManager_recordVsync(&manager);

	// Alternate between 59Hz and 61Hz (simulating jitter)
	for (int i = 0; i < 120; i++) {
		if (i % 2 == 0) {
			mock_time_us += 16949; // 59Hz
		} else {
			mock_time_us += 16393; // 61Hz
		}
		SyncManager_recordVsync(&manager);
	}

	// EMA should smooth to ~60Hz
	double measured = SyncManager_getMeasuredHz(&manager);
	TEST_ASSERT_FLOAT_WITHIN(1.0, 60.0, measured);
}

void test_drift_check_only_after_stable(void) {
	// Before stable, drift check shouldn't run
	manager.measurement_stable = false;
	manager.mode = SYNC_MODE_VSYNC;

	mock_time_us = 1000000;
	for (int i = 0; i < 300; i++) {
		mock_time_us += 15385; // 65Hz (should trigger drift)
		SyncManager_recordVsync(&manager);
	}

	// Mode shouldn't change (not stable yet)
	TEST_ASSERT_EQUAL(SYNC_MODE_VSYNC, manager.mode);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Initialization tests
	RUN_TEST(test_init_starts_in_audio_clock_mode);
	RUN_TEST(test_init_stores_game_fps);
	RUN_TEST(test_init_stores_display_hz);
	RUN_TEST(test_init_with_zero_hz_defaults_to_60);
	RUN_TEST(test_init_measurement_not_stable);

	// Vsync measurement tests
	RUN_TEST(test_first_vsync_just_records_timestamp);
	RUN_TEST(test_second_vsync_calculates_hz);
	RUN_TEST(test_rejects_outlier_too_low);
	RUN_TEST(test_rejects_outlier_too_high);
	RUN_TEST(test_rejects_zero_interval);

	// Mode switching tests
	RUN_TEST(test_switches_to_vsync_when_compatible);
	RUN_TEST(test_stays_in_audio_clock_when_incompatible);
	RUN_TEST(test_measurement_stable_after_120_samples);
	RUN_TEST(test_drift_detection_switches_back_to_audio_clock);

	// API tests
	RUN_TEST(test_should_run_core_always_returns_true);
	RUN_TEST(test_should_use_rate_control_in_vsync_mode);
	RUN_TEST(test_should_not_use_rate_control_in_audio_clock);
	RUN_TEST(test_should_block_audio_in_audio_clock_mode);
	RUN_TEST(test_should_not_block_audio_in_vsync_mode);
	RUN_TEST(test_get_measured_hz_returns_zero_when_not_stable);
	RUN_TEST(test_get_measured_hz_returns_value_when_stable);
	RUN_TEST(test_mode_name_audio_clock);
	RUN_TEST(test_mode_name_vsync);

	// Edge cases
	RUN_TEST(test_ema_smooths_noisy_measurements);
	RUN_TEST(test_drift_check_only_after_stable);

	return UNITY_END();
}
