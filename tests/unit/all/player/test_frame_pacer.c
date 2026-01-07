/**
 * test_frame_pacer.c - Unit tests for frame pacing
 *
 * Tests the Bresenham-style frame pacing algorithm including:
 * - Initialization with Q16.16 fixed-point
 * - Direct mode detection
 * - Paced mode accumulator behavior
 * - First frame always steps
 * - Long-run stability (no drift)
 * - Reset functionality
 */

#include "unity.h"
#include "frame_pacer.h"
#include <stdarg.h>

// Stub for PLAT_getDisplayHz - not tested here, just needed for linking
double PLAT_getDisplayHz(void) {
	return 60.0;
}

// Stub for getMicroseconds - returns incrementing time for vsync measurement tests
static uint64_t mock_time_us = 0;
uint64_t getMicroseconds(void) {
	return mock_time_us;
}

// Stub for LOG_info - just suppress output during tests
void LOG_info(const char* fmt, ...) {
	(void)fmt;
}

// Q16.16 scale factor for test assertions
#define Q16_SCALE 65536

// Test state
static FramePacer pacer;

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	// Fresh pacer for each test
	FramePacer_init(&pacer, 60.0, 60.0);
	// Reset mock time to non-zero (recordVsync checks last_vsync_time > 0)
	mock_time_us = 1000000; // Start at 1 second
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Initialization Tests
///////////////////////////////

void test_init_60fps_60hz_direct_mode(void) {
	FramePacer_init(&pacer, 60.0, 60.0);

	TEST_ASSERT_EQUAL_INT32(60 * Q16_SCALE, pacer.game_fps_q16);
	TEST_ASSERT_EQUAL_INT32(60 * Q16_SCALE, pacer.display_hz_q16);
	TEST_ASSERT_TRUE(pacer.direct_mode);
	// Accumulator initialized to display_hz for first-frame-steps
	TEST_ASSERT_EQUAL_INT32(60 * Q16_SCALE, pacer.accumulator);
}

void test_init_5994fps_60hz_direct_mode(void) {
	// NTSC 59.94fps @ 60Hz = 0.1% diff → direct mode (within 2% tolerance)
	FramePacer_init(&pacer, 59.94, 60.0);

	TEST_ASSERT_TRUE(pacer.direct_mode);
}

void test_init_60fps_60_5hz_direct_mode(void) {
	// 60fps @ 60.5Hz = 0.83% diff → direct mode (within 1% tolerance)
	// This is the kind of hardware variance audio rate control can handle
	FramePacer_init(&pacer, 60.0, 60.5);

	TEST_ASSERT_TRUE(pacer.direct_mode);
}

void test_init_60fps_61hz_paced_mode(void) {
	// 60fps @ 61Hz = 1.6% diff → paced mode (outside 1% tolerance)
	FramePacer_init(&pacer, 60.0, 61.0);

	TEST_ASSERT_FALSE(pacer.direct_mode);
}

void test_init_60fps_72hz_paced_mode(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	TEST_ASSERT_EQUAL_INT32(60 * Q16_SCALE, pacer.game_fps_q16);
	TEST_ASSERT_EQUAL_INT32(72 * Q16_SCALE, pacer.display_hz_q16);
	TEST_ASSERT_FALSE(pacer.direct_mode);
	// Accumulator initialized to display_hz for first-frame-steps
	TEST_ASSERT_EQUAL_INT32(72 * Q16_SCALE, pacer.accumulator);
}

void test_init_50fps_60hz_paced_mode(void) {
	// PAL games on NTSC display
	FramePacer_init(&pacer, 50.0, 60.0);

	TEST_ASSERT_FALSE(pacer.direct_mode);
}

void test_init_30fps_60hz_paced_mode(void) {
	// Half-speed games
	FramePacer_init(&pacer, 30.0, 60.0);

	TEST_ASSERT_FALSE(pacer.direct_mode);
}

void test_init_preserves_fractional_fps(void) {
	// 59.73fps (SNES) should preserve precision
	FramePacer_init(&pacer, 59.73, 60.0);

	// 59.73 * 65536 = 3,913,359.28 -> 3,913,359
	int32_t expected = (int32_t)(59.73 * Q16_SCALE);
	TEST_ASSERT_EQUAL_INT32(expected, pacer.game_fps_q16);
}

///////////////////////////////
// Direct Mode Tests
///////////////////////////////

void test_direct_mode_always_steps(void) {
	FramePacer_init(&pacer, 60.0, 60.0);
	TEST_ASSERT_TRUE(pacer.direct_mode);

	// Every call should return true
	for (int i = 0; i < 100; i++) {
		TEST_ASSERT_TRUE(FramePacer_step(&pacer));
	}
}

void test_isDirectMode_returns_correct_value(void) {
	FramePacer_init(&pacer, 60.0, 60.0);
	TEST_ASSERT_TRUE(FramePacer_isDirectMode(&pacer));

	FramePacer_init(&pacer, 60.0, 72.0);
	TEST_ASSERT_FALSE(FramePacer_isDirectMode(&pacer));
}

///////////////////////////////
// Paced Mode Tests (60fps @ 72Hz)
///////////////////////////////

void test_60fps_72hz_first_vsync_steps(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	// First vsync: acc = 72, >= 72 -> step (first frame always steps)
	TEST_ASSERT_TRUE(FramePacer_step(&pacer));
}

void test_60fps_72hz_second_vsync_repeats(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	// First vsync: step
	FramePacer_step(&pacer);

	// Second vsync: acc = 60, < 72 -> repeat
	TEST_ASSERT_FALSE(FramePacer_step(&pacer));
}

void test_60fps_72hz_pattern_6_vsyncs(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	// With acc starting at 72 (display_hz), pattern is:
	// Vsync 1: acc=72, >=72 -> step, acc = 72 - 72 + 60 = 60
	// Vsync 2: acc=60, <72 -> repeat, acc = 60 + 60 = 120
	// Vsync 3: acc=120, >=72 -> step, acc = 120 - 72 + 60 = 108
	// Vsync 4: acc=108, >=72 -> step, acc = 108 - 72 + 60 = 96
	// Vsync 5: acc=96, >=72 -> step, acc = 96 - 72 + 60 = 84
	// Vsync 6: acc=84, >=72 -> step, acc = 84 - 72 + 60 = 72
	// Result: 5 steps, 1 repeat in 6 vsyncs = 83.3% = 60/72

	bool results[6];
	for (int i = 0; i < 6; i++) {
		results[i] = FramePacer_step(&pacer);
	}

	TEST_ASSERT_TRUE(results[0]);  // step
	TEST_ASSERT_FALSE(results[1]); // repeat
	TEST_ASSERT_TRUE(results[2]);  // step
	TEST_ASSERT_TRUE(results[3]);  // step
	TEST_ASSERT_TRUE(results[4]);  // step
	TEST_ASSERT_TRUE(results[5]);  // step

	// Count: 5 steps, 1 repeat
	int steps = 0;
	for (int i = 0; i < 6; i++) {
		if (results[i]) steps++;
	}
	TEST_ASSERT_EQUAL(5, steps);
}

///////////////////////////////
// Paced Mode Tests (50fps @ 60Hz - PAL)
///////////////////////////////

void test_50fps_60hz_pattern_6_vsyncs(void) {
	FramePacer_init(&pacer, 50.0, 60.0);

	// 50fps @ 60Hz = step 50/60 = 83.3% of vsyncs
	// Pattern with acc starting at 60:
	// Vsync 1: acc=60, >=60 -> step, acc = 60 - 60 + 50 = 50
	// Vsync 2: acc=50, <60 -> repeat, acc = 50 + 50 = 100
	// Vsync 3: acc=100, >=60 -> step, acc = 100 - 60 + 50 = 90
	// Vsync 4: acc=90, >=60 -> step, acc = 90 - 60 + 50 = 80
	// Vsync 5: acc=80, >=60 -> step, acc = 80 - 60 + 50 = 70
	// Vsync 6: acc=70, >=60 -> step, acc = 70 - 60 + 50 = 60

	bool results[6];
	for (int i = 0; i < 6; i++) {
		results[i] = FramePacer_step(&pacer);
	}

	TEST_ASSERT_TRUE(results[0]);  // step
	TEST_ASSERT_FALSE(results[1]); // repeat
	TEST_ASSERT_TRUE(results[2]);  // step
	TEST_ASSERT_TRUE(results[3]);  // step
	TEST_ASSERT_TRUE(results[4]);  // step
	TEST_ASSERT_TRUE(results[5]);  // step
}

///////////////////////////////
// Paced Mode Tests (30fps @ 60Hz)
///////////////////////////////

void test_30fps_60hz_alternates(void) {
	FramePacer_init(&pacer, 30.0, 60.0);

	// 30fps @ 60Hz = step every other frame
	// Vsync 1: acc=60, >=60 -> step, acc = 60 - 60 + 30 = 30
	// Vsync 2: acc=30, <60 -> repeat, acc = 30 + 30 = 60
	// Vsync 3: acc=60, >=60 -> step, acc = 60 - 60 + 30 = 30
	// Vsync 4: acc=30, <60 -> repeat, acc = 30 + 30 = 60

	TEST_ASSERT_TRUE(FramePacer_step(&pacer));  // step
	TEST_ASSERT_FALSE(FramePacer_step(&pacer)); // repeat
	TEST_ASSERT_TRUE(FramePacer_step(&pacer));  // step
	TEST_ASSERT_FALSE(FramePacer_step(&pacer)); // repeat
}

///////////////////////////////
// Long-Run Stability Tests
///////////////////////////////

void test_60fps_72hz_long_run_correct_ratio(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	int steps = 0;
	int total_vsyncs = 7200; // 100 seconds at 72Hz

	for (int i = 0; i < total_vsyncs; i++) {
		if (FramePacer_step(&pacer)) {
			steps++;
		}
	}

	// Expected: 60/72 * 7200 = 6000 steps exactly
	TEST_ASSERT_EQUAL(6000, steps);
}

void test_50fps_60hz_long_run_correct_ratio(void) {
	FramePacer_init(&pacer, 50.0, 60.0);

	int steps = 0;
	int total_vsyncs = 6000; // 100 seconds at 60Hz

	for (int i = 0; i < total_vsyncs; i++) {
		if (FramePacer_step(&pacer)) {
			steps++;
		}
	}

	// Expected: 50/60 * 6000 = 5000 steps exactly
	TEST_ASSERT_EQUAL(5000, steps);
}

void test_30fps_60hz_long_run_correct_ratio(void) {
	FramePacer_init(&pacer, 30.0, 60.0);

	int steps = 0;
	int total_vsyncs = 6000;

	for (int i = 0; i < total_vsyncs; i++) {
		if (FramePacer_step(&pacer)) {
			steps++;
		}
	}

	// Expected: 30/60 * 6000 = 3000 steps exactly
	TEST_ASSERT_EQUAL(3000, steps);
}

void test_accumulator_stays_bounded(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	// Run for many iterations and verify accumulator never exceeds 2x display_hz
	// (theoretical max is display_hz + game_fps - 1)
	int32_t max_expected = pacer.display_hz_q16 + pacer.game_fps_q16;
	for (int i = 0; i < 10000; i++) {
		FramePacer_step(&pacer);
		TEST_ASSERT_LESS_THAN(max_expected, pacer.accumulator);
	}
}

///////////////////////////////
// Reset Tests
///////////////////////////////

void test_reset_to_display_hz(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	// Build up some accumulator
	FramePacer_step(&pacer);
	FramePacer_step(&pacer);

	// Reset
	FramePacer_reset(&pacer);

	// Should be back to display_hz
	TEST_ASSERT_EQUAL_INT32(pacer.display_hz_q16, pacer.accumulator);
}

void test_reset_ensures_next_step(void) {
	FramePacer_init(&pacer, 60.0, 72.0);

	// Drain accumulator
	for (int i = 0; i < 10; i++) {
		FramePacer_step(&pacer);
	}

	// Reset
	FramePacer_reset(&pacer);

	// Next call should step (accumulator = display_hz)
	TEST_ASSERT_TRUE(FramePacer_step(&pacer));
}

void test_reset_preserves_settings(void) {
	FramePacer_init(&pacer, 60.0, 72.0);
	FramePacer_step(&pacer);

	FramePacer_reset(&pacer);

	// Settings should be preserved
	TEST_ASSERT_EQUAL_INT32(60 * Q16_SCALE, pacer.game_fps_q16);
	TEST_ASSERT_EQUAL_INT32(72 * Q16_SCALE, pacer.display_hz_q16);
	TEST_ASSERT_FALSE(pacer.direct_mode);
}

///////////////////////////////
// Vsync Measurement Tests
///////////////////////////////

void test_vsync_measurement_not_stable_initially(void) {
	FramePacer_init(&pacer, 60.0, 60.0);

	TEST_ASSERT_FALSE(FramePacer_isMeasurementStable(&pacer));
	// getMeasuredHz returns 0 when not stable
	double hz = FramePacer_getMeasuredHz(&pacer);
	TEST_ASSERT_TRUE(hz == 0.0);
}

void test_vsync_measurement_accumulates_samples(void) {
	FramePacer_init(&pacer, 60.0, 60.0);

	// First call just sets baseline, doesn't count as sample
	FramePacer_recordVsync(&pacer);

	// Simulate 60Hz vsync (16667µs intervals)
	for (int i = 0; i < 50; i++) {
		mock_time_us += 16667; // ~60Hz
		FramePacer_recordVsync(&pacer);
	}

	// Should have samples but not stable yet (need 120)
	TEST_ASSERT_FALSE(FramePacer_isMeasurementStable(&pacer));
	TEST_ASSERT_EQUAL(50, pacer.vsync_samples);
}

// Helper to check if a double is within tolerance
static int within_tolerance(double actual, double expected, double tolerance) {
	double diff = actual - expected;
	if (diff < 0) diff = -diff;
	return diff <= tolerance;
}

void test_vsync_measurement_becomes_stable(void) {
	FramePacer_init(&pacer, 60.0, 60.0);

	// First call sets baseline
	FramePacer_recordVsync(&pacer);

	// Simulate 60Hz vsync (16667µs intervals) for warmup period
	for (int i = 0; i < FRAME_PACER_VSYNC_WARMUP + 10; i++) {
		mock_time_us += 16667;
		FramePacer_recordVsync(&pacer);
	}

	TEST_ASSERT_TRUE(FramePacer_isMeasurementStable(&pacer));
	// Should be approximately 60Hz (within 0.5Hz)
	double measured = FramePacer_getMeasuredHz(&pacer);
	TEST_ASSERT_TRUE(within_tolerance(measured, 60.0, 0.5));
}

void test_vsync_measurement_detects_higher_hz(void) {
	FramePacer_init(&pacer, 60.0, 60.0);

	// First call sets baseline
	FramePacer_recordVsync(&pacer);

	// Simulate 60.05Hz vsync (16653µs intervals instead of 16667µs)
	for (int i = 0; i < FRAME_PACER_VSYNC_WARMUP + 10; i++) {
		mock_time_us += 16653; // ~60.05Hz
		FramePacer_recordVsync(&pacer);
	}

	TEST_ASSERT_TRUE(FramePacer_isMeasurementStable(&pacer));
	double measured = FramePacer_getMeasuredHz(&pacer);
	// Should be approximately 60.05Hz (within 0.1Hz)
	TEST_ASSERT_TRUE(within_tolerance(measured, 60.05, 0.1));
}

void test_vsync_measurement_rejects_outliers(void) {
	FramePacer_init(&pacer, 60.0, 60.0);

	// First call sets baseline
	FramePacer_recordVsync(&pacer);

	// Simulate normal 60Hz vsync
	for (int i = 0; i < 50; i++) {
		mock_time_us += 16667;
		FramePacer_recordVsync(&pacer);
	}
	int samples_before = pacer.vsync_samples;

	// Simulate a frame drop (long interval = low Hz, rejected)
	mock_time_us += 50000; // ~20Hz, should be rejected
	FramePacer_recordVsync(&pacer);

	// Sample count should not have increased (outlier rejected)
	TEST_ASSERT_EQUAL(samples_before, pacer.vsync_samples);

	// Simulate a fast frame (very short interval = high Hz, rejected)
	mock_time_us += 5000; // ~200Hz, should be rejected
	FramePacer_recordVsync(&pacer);

	// Sample count should still not have increased
	TEST_ASSERT_EQUAL(samples_before, pacer.vsync_samples);
}

void test_vsync_measurement_reinits_pacer_when_hz_differs(void) {
	// Start with reported 60Hz but actual 60.05Hz
	FramePacer_init(&pacer, 60.0, 60.0);

	// Originally in direct mode (60fps @ 60Hz)
	TEST_ASSERT_TRUE(pacer.direct_mode);

	// First call sets baseline
	FramePacer_recordVsync(&pacer);

	// Simulate 60.05Hz vsync for warmup period
	for (int i = 0; i < FRAME_PACER_VSYNC_WARMUP + 10; i++) {
		mock_time_us += 16653; // ~60.05Hz
		FramePacer_recordVsync(&pacer);
	}

	// After measurement, display_hz_q16 should be updated to ~60.05 (within 0.1Hz)
	double updated_hz = pacer.display_hz_q16 / (double)Q16_SCALE;
	TEST_ASSERT_TRUE(within_tolerance(updated_hz, 60.05, 0.1));
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Initialization and tolerance tests
	RUN_TEST(test_init_60fps_60hz_direct_mode);
	RUN_TEST(test_init_5994fps_60hz_direct_mode);
	RUN_TEST(test_init_60fps_60_5hz_direct_mode); // within 1% tolerance
	RUN_TEST(test_init_60fps_61hz_paced_mode);    // outside 1% tolerance
	RUN_TEST(test_init_60fps_72hz_paced_mode);
	RUN_TEST(test_init_50fps_60hz_paced_mode);
	RUN_TEST(test_init_30fps_60hz_paced_mode);
	RUN_TEST(test_init_preserves_fractional_fps);

	// Direct mode
	RUN_TEST(test_direct_mode_always_steps);
	RUN_TEST(test_isDirectMode_returns_correct_value);

	// 60fps @ 72Hz
	RUN_TEST(test_60fps_72hz_first_vsync_steps);
	RUN_TEST(test_60fps_72hz_second_vsync_repeats);
	RUN_TEST(test_60fps_72hz_pattern_6_vsyncs);

	// 50fps @ 60Hz (PAL)
	RUN_TEST(test_50fps_60hz_pattern_6_vsyncs);

	// 30fps @ 60Hz
	RUN_TEST(test_30fps_60hz_alternates);

	// Long-run stability
	RUN_TEST(test_60fps_72hz_long_run_correct_ratio);
	RUN_TEST(test_50fps_60hz_long_run_correct_ratio);
	RUN_TEST(test_30fps_60hz_long_run_correct_ratio);
	RUN_TEST(test_accumulator_stays_bounded);

	// Reset
	RUN_TEST(test_reset_to_display_hz);
	RUN_TEST(test_reset_ensures_next_step);
	RUN_TEST(test_reset_preserves_settings);

	// Vsync measurement
	RUN_TEST(test_vsync_measurement_not_stable_initially);
	RUN_TEST(test_vsync_measurement_accumulates_samples);
	RUN_TEST(test_vsync_measurement_becomes_stable);
	RUN_TEST(test_vsync_measurement_detects_higher_hz);
	RUN_TEST(test_vsync_measurement_rejects_outliers);
	RUN_TEST(test_vsync_measurement_reinits_pacer_when_hz_differs);

	return UNITY_END();
}
