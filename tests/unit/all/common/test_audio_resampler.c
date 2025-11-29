/**
 * test_audio_resampler.c - Unit tests for audio resampling algorithm
 *
 * Tests the linear interpolation resampling with dynamic rate control.
 *
 * Test coverage:
 * - Initialization and reset
 * - Linear interpolation quality (smooth transitions)
 * - Upsampling (e.g., 44100 -> 48000)
 * - Downsampling (e.g., 48000 -> 44100)
 * - Equal rates (passthrough)
 * - Dynamic rate adjustment
 * - Buffer wrapping behavior
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/audio_resampler.h"

#include <math.h>
#include <string.h>

// Test buffer
#define TEST_BUFFER_SIZE 1000
static SND_Frame test_buffer_data[TEST_BUFFER_SIZE];
static AudioRingBuffer test_buffer;

void setUp(void) {
	// Clear test buffer
	memset(test_buffer_data, 0, sizeof(test_buffer_data));
	test_buffer.frames = test_buffer_data;
	test_buffer.capacity = TEST_BUFFER_SIZE;
	test_buffer.write_pos = 0;
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Initialization Tests
///////////////////////////////

void test_AudioResampler_init_sets_rates(void) {
	AudioResampler resampler;

	AudioResampler_init(&resampler, 44100, 48000);

	TEST_ASSERT_EQUAL_INT(44100, resampler.sample_rate_in);
	TEST_ASSERT_EQUAL_INT(48000, resampler.sample_rate_out);
	TEST_ASSERT_EQUAL_INT(0, resampler.frac_pos);
	TEST_ASSERT_EQUAL_INT(0, resampler.has_prev);
}

void test_AudioResampler_init_calculates_step(void) {
	AudioResampler resampler;

	// 44100 -> 48000: step = 44100/48000 * 65536 = 60211 (approximately)
	AudioResampler_init(&resampler, 44100, 48000);

	// Step should be less than FRAC_ONE for upsampling
	TEST_ASSERT_LESS_THAN(FRAC_ONE, resampler.frac_step);
	TEST_ASSERT_INT_WITHIN(100, 60211, resampler.frac_step);
}

void test_AudioResampler_init_downsampling_step(void) {
	AudioResampler resampler;

	// 48000 -> 44100: step = 48000/44100 * 65536 = 71347 (approximately)
	AudioResampler_init(&resampler, 48000, 44100);

	// Step should be greater than FRAC_ONE for downsampling
	TEST_ASSERT_GREATER_THAN(FRAC_ONE, resampler.frac_step);
	TEST_ASSERT_INT_WITHIN(100, 71347, resampler.frac_step);
}

void test_AudioResampler_reset_clears_state(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Accumulate some state
	resampler.frac_pos = 12345;
	resampler.has_prev = 1;
	resampler.prev_frame = (SND_Frame){100, 200};

	AudioResampler_reset(&resampler);

	TEST_ASSERT_EQUAL_INT(0, resampler.frac_pos);
	TEST_ASSERT_EQUAL_INT(0, resampler.has_prev);
	// Rates and step should be unchanged
	TEST_ASSERT_EQUAL_INT(44100, resampler.sample_rate_in);
	TEST_ASSERT_EQUAL_INT(48000, resampler.sample_rate_out);
}

void test_AudioResampler_isNeeded_returns_true_for_different_rates(void) {
	TEST_ASSERT_TRUE(AudioResampler_isNeeded(44100, 48000));
	TEST_ASSERT_TRUE(AudioResampler_isNeeded(48000, 44100));
	TEST_ASSERT_TRUE(AudioResampler_isNeeded(22050, 48000));
}

void test_AudioResampler_isNeeded_returns_false_for_same_rates(void) {
	TEST_ASSERT_FALSE(AudioResampler_isNeeded(44100, 44100));
	TEST_ASSERT_FALSE(AudioResampler_isNeeded(48000, 48000));
}

///////////////////////////////
// Linear Interpolation Tests
///////////////////////////////

void test_linear_interpolation_produces_smooth_output(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Create input with a step change: 0, 0, 1000, 1000
	SND_Frame input[4] = {
	    {0, 0},
	    {0, 0},
	    {1000, 1000},
	    {1000, 1000},
	};

	ResampleResult result = AudioResampler_resample(&resampler, &test_buffer, input, 4, 1.0f);

	// With linear interpolation, output should gradually transition from 0 to 1000
	// Not just jump like nearest-neighbor would

	// Check that we got output
	TEST_ASSERT_GREATER_THAN(0, result.frames_written);

	// Find values between 0 and 1000 (interpolated)
	int found_intermediate = 0;
	for (int i = 0; i < result.frames_written && i < TEST_BUFFER_SIZE; i++) {
		if (test_buffer.frames[i].left > 0 && test_buffer.frames[i].left < 1000) {
			found_intermediate = 1;
			break;
		}
	}
	TEST_ASSERT_TRUE(found_intermediate);
}

void test_linear_interpolation_preserves_constant_signal(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Constant input
	SND_Frame input[10];
	for (int i = 0; i < 10; i++) {
		input[i] = (SND_Frame){500, -500};
	}

	ResampleResult result = AudioResampler_resample(&resampler, &test_buffer, input, 10, 1.0f);

	// All output should also be constant (interpolation between equal values = same value)
	for (int i = 0; i < result.frames_written && i < TEST_BUFFER_SIZE; i++) {
		TEST_ASSERT_EQUAL_INT(500, test_buffer.frames[i].left);
		TEST_ASSERT_EQUAL_INT(-500, test_buffer.frames[i].right);
	}
}

///////////////////////////////
// Upsampling Tests (44100 -> 48000)
///////////////////////////////

void test_upsample_produces_more_output_than_input(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	SND_Frame input[100];
	for (int i = 0; i < 100; i++) {
		input[i] = (SND_Frame){(int16_t)i, (int16_t)i};
	}

	ResampleResult result = AudioResampler_resample(&resampler, &test_buffer, input, 100, 1.0f);

	// 44100 -> 48000: ratio ~1.088, so 100 input should produce ~109 output
	TEST_ASSERT_INT_WITHIN(5, 109, result.frames_written);
	// Should consume most/all input
	TEST_ASSERT_GREATER_THAN(95, result.frames_consumed);
}

void test_upsample_realistic_second_of_audio(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Use larger buffer for this test - upsampling 1000 inputs produces ~1088 outputs
	SND_Frame large_buffer_data[1200];
	AudioRingBuffer large_buffer = {
	    .frames = large_buffer_data,
	    .capacity = 1200,
	    .write_pos = 0,
	    .read_pos = 0,
	};

	// Simulate processing in chunks like real usage
	SND_Frame chunk[1000];
	int total_written = 0;
	int total_consumed = 0;

	// Process 44100 samples in chunks
	for (int batch = 0; batch < 45; batch++) { // ~45 batches of 1000
		int chunk_size = (batch < 44) ? 1000 : 100; // Last chunk smaller

		for (int i = 0; i < chunk_size; i++) {
			int sample_idx = total_consumed + i;
			chunk[i] = (SND_Frame){(int16_t)(sample_idx % 32768), (int16_t)(sample_idx % 32768)};
		}

		// Reset buffer positions for each batch (simulating consumption)
		large_buffer.write_pos = 0;
		large_buffer.read_pos = 0;
		ResampleResult result = AudioResampler_resample(&resampler, &large_buffer, chunk, chunk_size, 1.0f);
		total_written += result.frames_written;
		total_consumed += result.frames_consumed;

		if (total_consumed >= 44100)
			break;
	}

	// Should produce close to 48000 output frames for 44100 input
	TEST_ASSERT_INT_WITHIN(100, 48000, total_written);
}

///////////////////////////////
// Downsampling Tests (48000 -> 44100)
///////////////////////////////

void test_downsample_produces_less_output_than_input(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 48000, 44100);

	SND_Frame input[100];
	for (int i = 0; i < 100; i++) {
		input[i] = (SND_Frame){(int16_t)i, (int16_t)i};
	}

	ResampleResult result = AudioResampler_resample(&resampler, &test_buffer, input, 100, 1.0f);

	// 48000 -> 44100: ratio ~0.919, so 100 input should produce ~92 output
	TEST_ASSERT_INT_WITHIN(5, 92, result.frames_written);
	TEST_ASSERT_EQUAL_INT(100, result.frames_consumed);
}

void test_downsample_extreme_ratio(void) {
	// 96000 -> 22050 (ratio ~0.23)
	AudioResampler resampler;
	AudioResampler_init(&resampler, 96000, 22050);

	SND_Frame input[100];
	for (int i = 0; i < 100; i++) {
		input[i] = (SND_Frame){(int16_t)i, (int16_t)i};
	}

	ResampleResult result = AudioResampler_resample(&resampler, &test_buffer, input, 100, 1.0f);

	// Should produce ~23 output frames
	TEST_ASSERT_INT_WITHIN(5, 23, result.frames_written);
	TEST_ASSERT_EQUAL_INT(100, result.frames_consumed);
}

///////////////////////////////
// Equal Rate Tests (Passthrough)
///////////////////////////////

void test_equal_rates_one_to_one_mapping(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 48000, 48000);

	SND_Frame input[50];
	for (int i = 0; i < 50; i++) {
		input[i] = (SND_Frame){(int16_t)(i * 100), (int16_t)(i * 100)};
	}

	ResampleResult result = AudioResampler_resample(&resampler, &test_buffer, input, 50, 1.0f);

	// Equal rates: 1:1 mapping (approximately, due to interpolation start)
	TEST_ASSERT_INT_WITHIN(2, 50, result.frames_written);
	TEST_ASSERT_INT_WITHIN(2, 50, result.frames_consumed);
}

///////////////////////////////
// Dynamic Rate Adjustment Tests
///////////////////////////////

void test_rate_adjust_above_one_produces_fewer_samples(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Use larger input (500 samples) to make 1% difference detectable
	// 500 * 1.088 = 544 outputs normally, 544 / 1.01 ≈ 539 with 1% speedup
	SND_Frame input[500];
	for (int i = 0; i < 500; i++) {
		input[i] = (SND_Frame){(int16_t)(i % 32768), (int16_t)(i % 32768)};
	}

	// Normal rate
	ResampleResult result_normal = AudioResampler_resample(&resampler, &test_buffer, input, 500, 1.0f);
	int normal_output = result_normal.frames_written;

	// Reset and try with speedup (ratio > 1)
	// Use 1.01f (the max allowed by clamp) for reliable detection
	AudioResampler_reset(&resampler);
	test_buffer.write_pos = 0;

	ResampleResult result_fast = AudioResampler_resample(&resampler, &test_buffer, input, 500, 1.01f);

	// Should produce fewer output samples when sped up
	// With 500 inputs and ~1.088 ratio, we get ~544 outputs normally
	// At 1.01 speedup, should get ~539 outputs (5 fewer)
	TEST_ASSERT_LESS_THAN(normal_output, result_fast.frames_written);
}

void test_rate_adjust_below_one_produces_more_samples(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Use larger input (500 samples) to make 0.5% difference detectable
	SND_Frame input[500];
	for (int i = 0; i < 500; i++) {
		input[i] = (SND_Frame){(int16_t)(i % 32768), (int16_t)(i % 32768)};
	}

	// Normal rate
	ResampleResult result_normal = AudioResampler_resample(&resampler, &test_buffer, input, 500, 1.0f);
	int normal_output = result_normal.frames_written;

	// Reset and try with slowdown (ratio < 1)
	// Use 0.99f (the min allowed by clamp) for reliable detection
	AudioResampler_reset(&resampler);
	test_buffer.write_pos = 0;

	ResampleResult result_slow = AudioResampler_resample(&resampler, &test_buffer, input, 500, 0.99f);

	// Should produce more output samples when slowed down
	// With 500 inputs at ~1.088 ratio, should produce ~549 outputs (5 more)
	TEST_ASSERT_GREATER_THAN(normal_output, result_slow.frames_written);
}

void test_rate_adjust_clamped_to_safe_range(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	SND_Frame input[100];
	for (int i = 0; i < 100; i++) {
		input[i] = (SND_Frame){(int16_t)i, (int16_t)i};
	}

	// Get baseline with normal rate
	ResampleResult result_normal = AudioResampler_resample(&resampler, &test_buffer, input, 100, 1.0f);
	int normal_output = result_normal.frames_written;

	// Reset and test with extreme ratio (10x) - should be clamped to max 1.01
	AudioResampler_reset(&resampler);
	test_buffer.write_pos = 0;
	ResampleResult result_extreme = AudioResampler_resample(&resampler, &test_buffer, input, 100, 10.0f);

	// Should still produce reasonable output (not crash)
	TEST_ASSERT_GREATER_THAN(0, result_extreme.frames_written);
	TEST_ASSERT_GREATER_THAN(0, result_extreme.frames_consumed);

	// Extreme ratio clamped to 1.01 should produce output close to 1.01x baseline
	// Not the 10x that was requested. Allow some tolerance for first-sample behavior.
	int expected_clamped = (int)(normal_output * 0.99f); // 1.01 ratio = fewer outputs
	TEST_ASSERT_INT_WITHIN(10, expected_clamped, result_extreme.frames_written);
}

///////////////////////////////
// Buffer Wrapping Tests
///////////////////////////////

void test_buffer_wraps_at_capacity(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 44100); // 1:1 for simplicity

	// Create small buffer
	SND_Frame small_buffer[5];
	memset(small_buffer, 0, sizeof(small_buffer));
	AudioRingBuffer buffer = {.frames = small_buffer, .capacity = 5, .write_pos = 3};

	SND_Frame input[10];
	for (int i = 0; i < 10; i++) {
		input[i] = (SND_Frame){(int16_t)(i * 10), (int16_t)(i * 10)};
	}

	AudioResampler_resample(&resampler, &buffer, input, 10, 1.0f);

	// Write position should have wrapped
	// With capacity 5 and starting at 3, after 10 writes: (3+10) % 5 = 3
	// But due to interpolation, may vary slightly
	TEST_ASSERT_LESS_THAN(5, buffer.write_pos);
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_empty_input_returns_zero(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	SND_Frame input[1] = {{0, 0}};

	ResampleResult result = AudioResampler_resample(&resampler, &test_buffer, input, 0, 1.0f);

	TEST_ASSERT_EQUAL_INT(0, result.frames_written);
	TEST_ASSERT_EQUAL_INT(0, result.frames_consumed);
}

void test_null_buffer_counts_output_only(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	SND_Frame input[100];
	for (int i = 0; i < 100; i++) {
		input[i] = (SND_Frame){(int16_t)i, (int16_t)i};
	}

	// NULL buffer = dry run
	ResampleResult result = AudioResampler_resample(&resampler, NULL, input, 100, 1.0f);

	// Should still count frames
	TEST_ASSERT_GREATER_THAN(0, result.frames_written);
	TEST_ASSERT_GREATER_THAN(0, result.frames_consumed);
}

void test_estimateOutput_approximates_correctly(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	int estimate = AudioResampler_estimateOutput(&resampler, 44100, 1.0f);

	// Should estimate close to 48000
	TEST_ASSERT_INT_WITHIN(100, 48000, estimate);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Initialization
	RUN_TEST(test_AudioResampler_init_sets_rates);
	RUN_TEST(test_AudioResampler_init_calculates_step);
	RUN_TEST(test_AudioResampler_init_downsampling_step);
	RUN_TEST(test_AudioResampler_reset_clears_state);
	RUN_TEST(test_AudioResampler_isNeeded_returns_true_for_different_rates);
	RUN_TEST(test_AudioResampler_isNeeded_returns_false_for_same_rates);

	// Linear Interpolation
	RUN_TEST(test_linear_interpolation_produces_smooth_output);
	RUN_TEST(test_linear_interpolation_preserves_constant_signal);

	// Upsampling
	RUN_TEST(test_upsample_produces_more_output_than_input);
	RUN_TEST(test_upsample_realistic_second_of_audio);

	// Downsampling
	RUN_TEST(test_downsample_produces_less_output_than_input);
	RUN_TEST(test_downsample_extreme_ratio);

	// Equal rates
	RUN_TEST(test_equal_rates_one_to_one_mapping);

	// Dynamic rate adjustment
	RUN_TEST(test_rate_adjust_above_one_produces_fewer_samples);
	RUN_TEST(test_rate_adjust_below_one_produces_more_samples);
	RUN_TEST(test_rate_adjust_clamped_to_safe_range);

	// Buffer wrapping
	RUN_TEST(test_buffer_wraps_at_capacity);

	// Edge cases
	RUN_TEST(test_empty_input_returns_zero);
	RUN_TEST(test_null_buffer_counts_output_only);
	RUN_TEST(test_estimateOutput_approximates_correctly);

	return UNITY_END();
}
