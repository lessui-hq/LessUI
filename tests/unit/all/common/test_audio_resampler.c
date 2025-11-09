/**
 * test_audio_resampler.c - Unit tests for audio resampling algorithm
 *
 * Tests the Bresenham-like nearest-neighbor resampling extracted from api.c.
 * This algorithm handles sample rate conversion for audio playback.
 *
 * Test coverage:
 * - Initialization and reset
 * - Upsampling (e.g., 44100 -> 48000) - frame duplication
 * - Downsampling (e.g., 48000 -> 44100) - frame skipping
 * - Equal rates (passthrough)
 * - Extreme ratios
 * - Buffer wrapping behavior
 * - State accumulation over many frames
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/audio_resampler.h"

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
	TEST_ASSERT_EQUAL_INT(0, resampler.diff);
}

void test_AudioResampler_reset_clears_diff(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Accumulate some diff
	resampler.diff = 12345;

	AudioResampler_reset(&resampler);

	TEST_ASSERT_EQUAL_INT(0, resampler.diff);
	// Rates should be unchanged
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
// Upsampling Tests (44100 -> 48000)
///////////////////////////////

void test_upsample_first_frame_written_not_consumed(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	SND_Frame frame = {.left = 100, .right = 200};
	ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

	// First frame: diff starts at 0, which is < 48000
	// So write frame, diff becomes 44100
	// 44100 < 48000, so consumed = 0
	TEST_ASSERT_EQUAL_INT(1, result.wrote_frame);
	TEST_ASSERT_EQUAL_INT(0, result.consumed);
	TEST_ASSERT_EQUAL_INT(44100, resampler.diff);

	// Verify frame was written to buffer
	TEST_ASSERT_EQUAL_INT(100, test_buffer.frames[0].left);
	TEST_ASSERT_EQUAL_INT(200, test_buffer.frames[0].right);
	TEST_ASSERT_EQUAL_INT(1, test_buffer.write_pos);
}

void test_upsample_second_frame_written_and_consumed(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	SND_Frame frame = {.left = 100, .right = 200};

	// First call
	AudioResampler_processFrame(&resampler, &test_buffer, frame);

	// Second call with same frame (since consumed=0 from first)
	ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

	// diff starts at 44100, which is < 48000
	// Write frame, diff becomes 88200
	// 88200 >= 48000, so consumed = 1, diff = 40200
	TEST_ASSERT_EQUAL_INT(1, result.wrote_frame);
	TEST_ASSERT_EQUAL_INT(1, result.consumed);
	TEST_ASSERT_EQUAL_INT(40200, resampler.diff);

	// Verify both frames written
	TEST_ASSERT_EQUAL_INT(2, test_buffer.write_pos);
}

void test_upsample_pattern_over_many_frames(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	int writes = 0;
	int consumes = 0;
	int input_frame_id = 0;

	// Process 100 input frames
	for (int i = 0; i < 200; i++) { // Up to 200 calls (may duplicate)
		if (consumes >= 100)
			break; // Processed all 100 input frames

		SND_Frame frame = {.left = (int16_t)input_frame_id, .right = (int16_t)input_frame_id};
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (result.wrote_frame)
			writes++;
		if (result.consumed)
			consumes++;

		// Advance input only if consumed
		if (result.consumed)
			input_frame_id++;
	}

	// For 44100 -> 48000, ratio is 48000/44100 ≈ 1.0884
	// So 100 input frames should produce ~109 output frames
	TEST_ASSERT_INT_WITHIN(2, 109, writes);
	TEST_ASSERT_EQUAL_INT(100, consumes);
}

///////////////////////////////
// Downsampling Tests (48000 -> 44100)
///////////////////////////////

void test_downsample_first_frame_written_and_consumed(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 48000, 44100);

	SND_Frame frame = {.left = 100, .right = 200};
	ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

	// diff starts at 0, which is < 44100
	// Write frame, diff becomes 48000
	// 48000 >= 44100, so consumed = 1, diff = 3900
	TEST_ASSERT_EQUAL_INT(1, result.wrote_frame);
	TEST_ASSERT_EQUAL_INT(1, result.consumed);
	TEST_ASSERT_EQUAL_INT(3900, resampler.diff);
}

void test_downsample_pattern_over_many_frames(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 48000, 44100);

	int writes = 0;
	int consumes = 0;

	// Process 100 input frames
	for (int i = 0; i < 100; i++) {
		SND_Frame frame = {.left = (int16_t)i, .right = (int16_t)i};
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (result.wrote_frame)
			writes++;
		if (result.consumed)
			consumes++;
	}

	// For 48000 -> 44100, ratio is 44100/48000 ≈ 0.91875
	// So 100 input frames should produce ~92 output frames
	TEST_ASSERT_INT_WITHIN(2, 92, writes);
	TEST_ASSERT_EQUAL_INT(100, consumes);
}

void test_downsample_extreme_skips_frames(void) {
	// Extreme downsample: 96000 -> 22050 (ratio ~0.23)
	AudioResampler resampler;
	AudioResampler_init(&resampler, 96000, 22050);

	int writes = 0;
	int consumes = 0;

	// Process 100 input frames
	for (int i = 0; i < 100; i++) {
		SND_Frame frame = {.left = (int16_t)i, .right = (int16_t)i};
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (result.wrote_frame)
			writes++;
		if (result.consumed)
			consumes++;
	}

	// Should skip most frames (~77%)
	TEST_ASSERT_INT_WITHIN(5, 23, writes);
	TEST_ASSERT_EQUAL_INT(100, consumes);
}

///////////////////////////////
// Equal Rate Tests (Passthrough)
///////////////////////////////

void test_equal_rates_writes_and_consumes_every_frame(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 48000, 48000);

	int writes = 0;
	int consumes = 0;

	// Process 50 frames
	for (int i = 0; i < 50; i++) {
		SND_Frame frame = {.left = (int16_t)i, .right = (int16_t)i};
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (result.wrote_frame)
			writes++;
		if (result.consumed)
			consumes++;
	}

	// Equal rates: every frame written and consumed (1:1)
	TEST_ASSERT_EQUAL_INT(50, writes);
	TEST_ASSERT_EQUAL_INT(50, consumes);
}

///////////////////////////////
// Buffer Wrapping Tests
///////////////////////////////

void test_buffer_wraps_at_capacity(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 44100); // No resampling for simplicity

	// Create small buffer
	SND_Frame small_buffer[5];
	AudioRingBuffer buffer = {.frames = small_buffer, .capacity = 5, .write_pos = 0};

	// Write 10 frames (should wrap twice)
	for (int i = 0; i < 10; i++) {
		SND_Frame frame = {.left = (int16_t)i, .right = (int16_t)i};
		AudioResampler_processFrame(&resampler, &buffer, frame);
	}

	// Write position should have wrapped
	TEST_ASSERT_EQUAL_INT(0, buffer.write_pos); // Wrapped back to 0

	// Last write should be frame 9 at position 4 (then wrapped to 0)
	TEST_ASSERT_EQUAL_INT(9, small_buffer[4].left);
}

///////////////////////////////
// State Accumulation Tests
///////////////////////////////

void test_resampler_accumulates_diff_correctly(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	SND_Frame frame = {0, 0};

	// Call 1: diff = 0 -> write, diff = 44100, consumed = 0
	AudioResampler_processFrame(&resampler, &test_buffer, frame);
	TEST_ASSERT_EQUAL_INT(44100, resampler.diff);

	// Call 2: diff = 44100 -> write, diff = 88200, consumed = 1, diff = 40200
	AudioResampler_processFrame(&resampler, &test_buffer, frame);
	TEST_ASSERT_EQUAL_INT(40200, resampler.diff);

	// Call 3: diff = 40200 -> write, diff = 84300, consumed = 1, diff = 36300
	AudioResampler_processFrame(&resampler, &test_buffer, frame);
	TEST_ASSERT_EQUAL_INT(36300, resampler.diff);
}

void test_downsample_eventually_skips_write(void) {
	// Use ratio where diff will exceed rate_out quickly
	AudioResampler resampler;
	AudioResampler_init(&resampler, 96000, 44100);

	SND_Frame frame = {100, 200};

	// Process frames until we see a skip
	int found_skip = 0;
	for (int i = 0; i < 20; i++) {
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (!result.wrote_frame && result.consumed) {
			found_skip = 1;
			break;
		}
	}

	TEST_ASSERT_TRUE(found_skip);
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_upsample_extreme_ratio(void) {
	// 22050 -> 48000 (ratio ~2.18)
	AudioResampler resampler;
	AudioResampler_init(&resampler, 22050, 48000);

	int writes = 0;
	int consumes = 0;

	for (int i = 0; i < 100; i++) {
		SND_Frame frame = {.left = (int16_t)i, .right = (int16_t)i};
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (result.wrote_frame)
			writes++;
		if (result.consumed)
			consumes++;

		if (consumes >= 100)
			break;
	}

	// Should write more than 2x the input (~218 frames from 100 input)
	TEST_ASSERT_GREATER_THAN(200, writes);
	TEST_ASSERT_EQUAL_INT(100, consumes);
}

void test_reset_clears_accumulated_state(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Accumulate state
	SND_Frame frame = {0, 0};
	AudioResampler_processFrame(&resampler, &test_buffer, frame);
	AudioResampler_processFrame(&resampler, &test_buffer, frame);

	TEST_ASSERT_NOT_EQUAL(0, resampler.diff);

	// Reset
	AudioResampler_reset(&resampler);

	// Should behave like first frame again
	ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);
	TEST_ASSERT_EQUAL_INT(1, result.wrote_frame);
	TEST_ASSERT_EQUAL_INT(0, result.consumed);
	TEST_ASSERT_EQUAL_INT(44100, resampler.diff);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_upsample_44100_to_48000_realistic_scenario(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Simulate 1 second of audio (44100 input frames)
	int writes = 0;
	int consumes = 0;
	int input_pos = 0;

	while (consumes < 44100) {
		SND_Frame frame = {.left = (int16_t)input_pos, .right = (int16_t)input_pos};
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (result.wrote_frame)
			writes++;
		if (result.consumed)
			input_pos++;

		consumes += result.consumed;

		// Safety limit
		if (writes > 50000)
			break;
	}

	// Should produce close to 48000 output frames
	TEST_ASSERT_INT_WITHIN(10, 48000, writes);
	TEST_ASSERT_EQUAL_INT(44100, consumes);
}

void test_downsample_48000_to_44100_realistic_scenario(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 48000, 44100);

	// Simulate 1 second of audio (48000 input frames)
	int writes = 0;
	int consumes = 0;

	for (int i = 0; i < 48000; i++) {
		SND_Frame frame = {.left = (int16_t)i, .right = (int16_t)i};
		ResampleResult result = AudioResampler_processFrame(&resampler, &test_buffer, frame);

		if (result.wrote_frame)
			writes++;
		if (result.consumed)
			consumes++;
	}

	// Should produce close to 44100 output frames
	TEST_ASSERT_INT_WITHIN(10, 44100, writes);
	TEST_ASSERT_EQUAL_INT(48000, consumes);
}

void test_buffer_contains_correct_frame_data(void) {
	AudioResampler resampler;
	AudioResampler_init(&resampler, 44100, 48000);

	// Write a few distinct frames
	SND_Frame f1 = {.left = 111, .right = 222};
	SND_Frame f2 = {.left = 333, .right = 444};

	AudioResampler_processFrame(&resampler, &test_buffer, f1); // Write f1
	AudioResampler_processFrame(&resampler, &test_buffer, f1); // Write f1 again (duplicate)
	AudioResampler_processFrame(&resampler, &test_buffer, f2); // Write f2

	// Check buffer contents
	TEST_ASSERT_EQUAL_INT(111, test_buffer.frames[0].left);
	TEST_ASSERT_EQUAL_INT(222, test_buffer.frames[0].right);
	TEST_ASSERT_EQUAL_INT(111, test_buffer.frames[1].left); // Duplicated
	TEST_ASSERT_EQUAL_INT(333, test_buffer.frames[2].left);
	TEST_ASSERT_EQUAL_INT(444, test_buffer.frames[2].right);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Initialization
	RUN_TEST(test_AudioResampler_init_sets_rates);
	RUN_TEST(test_AudioResampler_reset_clears_diff);
	RUN_TEST(test_AudioResampler_isNeeded_returns_true_for_different_rates);
	RUN_TEST(test_AudioResampler_isNeeded_returns_false_for_same_rates);

	// Upsampling
	RUN_TEST(test_upsample_first_frame_written_not_consumed);
	RUN_TEST(test_upsample_second_frame_written_and_consumed);
	RUN_TEST(test_upsample_pattern_over_many_frames);

	// Downsampling
	RUN_TEST(test_downsample_first_frame_written_and_consumed);
	RUN_TEST(test_downsample_pattern_over_many_frames);
	RUN_TEST(test_downsample_extreme_skips_frames);

	// Equal rates
	RUN_TEST(test_equal_rates_writes_and_consumes_every_frame);

	// Buffer wrapping
	RUN_TEST(test_buffer_wraps_at_capacity);

	// State accumulation
	RUN_TEST(test_resampler_accumulates_diff_correctly);
	RUN_TEST(test_downsample_eventually_skips_write);
	RUN_TEST(test_reset_clears_accumulated_state);

	// Integration
	RUN_TEST(test_upsample_44100_to_48000_realistic_scenario);
	RUN_TEST(test_downsample_48000_to_44100_realistic_scenario);
	RUN_TEST(test_buffer_contains_correct_frame_data);

	return UNITY_END();
}
