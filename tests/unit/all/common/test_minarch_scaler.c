/**
 * test_minarch_scaler.c - Unit tests for video scaling calculations
 *
 * Tests the pure scaling calculation functions extracted from minarch.c.
 * These tests verify the math for determining how to scale emulator
 * video output to fit various screen sizes and aspect ratios.
 *
 * Test coverage:
 * - MinArchScaler_applyRotation - Dimension swapping for rotated displays
 * - MinArchScaler_calculateAspectDimensions - Aspect ratio calculations
 * - MinArchScaler_clampToBuffer - Buffer bounds checking
 * - MinArchScaler_calculate - Full scaling calculation
 */

#include "../../support/unity/unity.h"
#include "minarch_scaler.h"

#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// MinArchScaler_applyRotation tests
///////////////////////////////

void test_applyRotation_none(void) {
	int w = 320, h = 240;
	MinArchScaler_applyRotation(SCALER_ROTATION_NONE, &w, &h);
	TEST_ASSERT_EQUAL_INT(320, w);
	TEST_ASSERT_EQUAL_INT(240, h);
}

void test_applyRotation_90_swaps_dimensions(void) {
	int w = 320, h = 240;
	MinArchScaler_applyRotation(SCALER_ROTATION_90, &w, &h);
	TEST_ASSERT_EQUAL_INT(240, w);
	TEST_ASSERT_EQUAL_INT(320, h);
}

void test_applyRotation_180_no_swap(void) {
	int w = 320, h = 240;
	MinArchScaler_applyRotation(SCALER_ROTATION_180, &w, &h);
	TEST_ASSERT_EQUAL_INT(320, w);
	TEST_ASSERT_EQUAL_INT(240, h);
}

void test_applyRotation_270_swaps_dimensions(void) {
	int w = 320, h = 240;
	MinArchScaler_applyRotation(SCALER_ROTATION_270, &w, &h);
	TEST_ASSERT_EQUAL_INT(240, w);
	TEST_ASSERT_EQUAL_INT(320, h);
}

void test_applyRotation_square_unchanged(void) {
	int w = 256, h = 256;
	MinArchScaler_applyRotation(SCALER_ROTATION_90, &w, &h);
	TEST_ASSERT_EQUAL_INT(256, w);
	TEST_ASSERT_EQUAL_INT(256, h);
}

///////////////////////////////
// MinArchScaler_calculateAspectDimensions tests
///////////////////////////////

void test_aspectDimensions_4_3_aspect(void) {
	int out_w, out_h;
	MinArchScaler_calculateAspectDimensions(256, 224, 4.0 / 3.0, &out_w, &out_h);
	// 256 / (4/3) = 192, but that's less than 224, so use height-based
	// 224 * (4/3) = 298.67 -> 298 (truncated, then made even stays 298)
	TEST_ASSERT_EQUAL_INT(298, out_w);
	TEST_ASSERT_EQUAL_INT(224, out_h);
}

void test_aspectDimensions_1_1_aspect(void) {
	int out_w, out_h;
	MinArchScaler_calculateAspectDimensions(256, 256, 1.0, &out_w, &out_h);
	TEST_ASSERT_EQUAL_INT(256, out_w);
	TEST_ASSERT_EQUAL_INT(256, out_h);
}

void test_aspectDimensions_16_9_aspect(void) {
	int out_w, out_h;
	MinArchScaler_calculateAspectDimensions(320, 240, 16.0 / 9.0, &out_w, &out_h);
	// 320 / (16/9) = 180, less than 240, so use height-based
	// 240 * (16/9) = 426.67 -> 426 (truncated, then made even stays 426)
	TEST_ASSERT_EQUAL_INT(426, out_w);
	TEST_ASSERT_EQUAL_INT(240, out_h);
}

void test_aspectDimensions_wide_source(void) {
	int out_w, out_h;
	// Wide source (640x200) with 4:3 target
	MinArchScaler_calculateAspectDimensions(640, 200, 4.0 / 3.0, &out_w, &out_h);
	// 640 / (4/3) = 480, which is > 200, so use width-based
	TEST_ASSERT_EQUAL_INT(640, out_w);
	TEST_ASSERT_EQUAL_INT(480, out_h);
}

///////////////////////////////
// MinArchScaler_clampToBuffer tests
///////////////////////////////

void test_clampToBuffer_within_bounds(void) {
	MinArchScalerResult result = {.dst_w = 640, .dst_h = 480, .dst_p = 1280, .dst_x = 0, .dst_y = 0};

	bool clamped = MinArchScaler_clampToBuffer(&result, 960, 720, 2);

	TEST_ASSERT_FALSE(clamped);
	TEST_ASSERT_EQUAL_INT(640, result.dst_w);
	TEST_ASSERT_EQUAL_INT(480, result.dst_h);
}

void test_clampToBuffer_exceeds_width(void) {
	MinArchScalerResult result = {.dst_w = 1200, .dst_h = 480, .dst_p = 2400, .dst_x = 100, .dst_y = 50};

	bool clamped = MinArchScaler_clampToBuffer(&result, 960, 720, 2);

	TEST_ASSERT_TRUE(clamped);
	TEST_ASSERT_EQUAL_INT(960, result.dst_w);
	// Height scaled proportionally: 480 * (960/1200) = 384
	TEST_ASSERT_EQUAL_INT(384, result.dst_h);
	// Pitch updated
	TEST_ASSERT_EQUAL_INT(1920, result.dst_p);
}

void test_clampToBuffer_exceeds_height(void) {
	MinArchScalerResult result = {.dst_w = 640, .dst_h = 900, .dst_p = 1280, .dst_x = 0, .dst_y = 0};

	bool clamped = MinArchScaler_clampToBuffer(&result, 960, 720, 2);

	TEST_ASSERT_TRUE(clamped);
	// Width scaled proportionally: 640 * (720/900) = 512
	TEST_ASSERT_EQUAL_INT(512, result.dst_w);
	TEST_ASSERT_EQUAL_INT(720, result.dst_h);
}

void test_clampToBuffer_adjusts_offsets(void) {
	MinArchScalerResult result = {.dst_w = 1920, .dst_h = 1080, .dst_p = 3840, .dst_x = 100, .dst_y = 80};

	bool clamped = MinArchScaler_clampToBuffer(&result, 960, 540, 2);

	TEST_ASSERT_TRUE(clamped);
	// Scale factor is 0.5 (both dimensions halved)
	TEST_ASSERT_EQUAL_INT(960, result.dst_w);
	TEST_ASSERT_EQUAL_INT(540, result.dst_h);
	// Offsets should be scaled proportionally
	TEST_ASSERT_EQUAL_INT(50, result.dst_x);
	TEST_ASSERT_EQUAL_INT(40, result.dst_y);
}

///////////////////////////////
// MinArchScaler_calculate - Native mode tests
///////////////////////////////

void test_calculate_native_2x_scale(void) {
	MinArchScalerInput input = {.src_w = 256,
	                            .src_h = 224,
	                            .src_p = 512,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_NATIVE,
	                            .device_w = 640,
	                            .device_h = 480,
	                            .device_p = 1280,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 960,
	                            .buffer_h = 720,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// 640/256 = 2.5, 480/224 = 2.14 -> scale = 2
	TEST_ASSERT_EQUAL_INT(2, result.scale);
	TEST_ASSERT_EQUAL_INT(256, result.src_w);
	TEST_ASSERT_EQUAL_INT(224, result.src_h);
	// Centered: (640 - 512) / 2 = 64
	TEST_ASSERT_EQUAL_INT(64, result.dst_x);
	// Centered: (480 - 448) / 2 = 16
	TEST_ASSERT_EQUAL_INT(16, result.dst_y);
}

void test_calculate_native_1x_scale(void) {
	MinArchScalerInput input = {.src_w = 320,
	                            .src_h = 240,
	                            .src_p = 640,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_NATIVE,
	                            .device_w = 320,
	                            .device_h = 240,
	                            .device_p = 640,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 480,
	                            .buffer_h = 360,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	TEST_ASSERT_EQUAL_INT(1, result.scale);
	TEST_ASSERT_EQUAL_INT(0, result.dst_x);
	TEST_ASSERT_EQUAL_INT(0, result.dst_y);
}

///////////////////////////////
// MinArchScaler_calculate - Aspect mode tests
///////////////////////////////

void test_calculate_aspect_fit_mode(void) {
	MinArchScalerInput input = {.src_w = 256,
	                            .src_h = 224,
	                            .src_p = 512,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_ASPECT,
	                            .device_w = 640,
	                            .device_h = 480,
	                            .device_p = 1280,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 960,
	                            .buffer_h = 720,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Should use nearest neighbor scaling
	TEST_ASSERT_EQUAL_INT(-1, result.scale);
	// Aspect ratio preserved
	TEST_ASSERT_TRUE(result.aspect > 1.0);
}

void test_calculate_fullscreen_fit_mode(void) {
	MinArchScalerInput input = {.src_w = 256,
	                            .src_h = 224,
	                            .src_p = 512,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_FULLSCREEN,
	                            .device_w = 640,
	                            .device_h = 480,
	                            .device_p = 1280,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 960,
	                            .buffer_h = 720,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Fullscreen fills entire device
	TEST_ASSERT_EQUAL_INT(640, result.dst_w);
	TEST_ASSERT_EQUAL_INT(480, result.dst_h);
	TEST_ASSERT_EQUAL_INT(0, result.dst_x);
	TEST_ASSERT_EQUAL_INT(0, result.dst_y);
	TEST_ASSERT_EQUAL_INT(-1, (int)result.aspect);
}

///////////////////////////////
// MinArchScaler_calculate - Rotation tests
///////////////////////////////

void test_calculate_rotation_90(void) {
	MinArchScalerInput input = {.src_w = 320,
	                            .src_h = 240,
	                            .src_p = 640,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_90,
	                            .mode = SCALER_MODE_NATIVE,
	                            .device_w = 480,
	                            .device_h = 640,
	                            .device_p = 960,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 720,
	                            .buffer_h = 960,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Dimensions should be swapped
	TEST_ASSERT_EQUAL_INT(240, result.true_w);
	TEST_ASSERT_EQUAL_INT(320, result.true_h);
}

void test_calculate_rotation_270(void) {
	MinArchScalerInput input = {.src_w = 320,
	                            .src_h = 240,
	                            .src_p = 640,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_270,
	                            .mode = SCALER_MODE_NATIVE,
	                            .device_w = 480,
	                            .device_h = 640,
	                            .device_p = 960,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 720,
	                            .buffer_h = 960,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Dimensions should be swapped
	TEST_ASSERT_EQUAL_INT(240, result.true_w);
	TEST_ASSERT_EQUAL_INT(320, result.true_h);
}

///////////////////////////////
// MinArchScaler_calculate - Cropped mode tests
///////////////////////////////

void test_calculate_cropped_mode(void) {
	MinArchScalerInput input = {.src_w = 320,
	                            .src_h = 240,
	                            .src_p = 640,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_CROPPED,
	                            .device_w = 640,
	                            .device_h = 480,
	                            .device_p = 1280,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 960,
	                            .buffer_h = 720,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Cropped mode fills the screen
	TEST_ASSERT_EQUAL_INT(640, result.dst_w);
	TEST_ASSERT_EQUAL_INT(480, result.dst_h);
	TEST_ASSERT_EQUAL_STRING("cropped", result.scaler_name);
}

void test_calculate_cropped_on_hdmi_becomes_native(void) {
	MinArchScalerInput input = {.src_w = 320,
	                            .src_h = 240,
	                            .src_p = 640,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_CROPPED,
	                            .device_w = 1280, // HDMI width
	                            .device_h = 720,
	                            .device_p = 2560,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 1920,
	                            .buffer_h = 1080,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Should use native/integer mode instead of cropped
	TEST_ASSERT_EQUAL_STRING("integer", result.scaler_name);
}

///////////////////////////////
// MinArchScaler_calculate - Forced crop tests
///////////////////////////////

void test_calculate_forced_crop_large_source(void) {
	MinArchScalerInput input = {.src_w = 800,
	                            .src_h = 600,
	                            .src_p = 1600,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_NATIVE,
	                            .device_w = 640,
	                            .device_h = 480,
	                            .device_p = 1280,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 960,
	                            .buffer_h = 720,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Source larger than device - forced crop
	TEST_ASSERT_EQUAL_STRING("forced crop", result.scaler_name);
	// Source should be cropped
	TEST_ASSERT_GREATER_THAN(0, result.src_x);
	TEST_ASSERT_GREATER_THAN(0, result.src_y);
}

///////////////////////////////
// MinArchScaler_calculate - Oversized device tests
///////////////////////////////

void test_calculate_oversized_device_fullscreen(void) {
	MinArchScalerInput input = {.src_w = 256,
	                            .src_h = 224,
	                            .src_p = 512,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_FULLSCREEN,
	                            .device_w = 1920,
	                            .device_h = 1080,
	                            .device_p = 3840,
	                            .bpp = 2,
	                            .fit = false, // Oversized device
	                            .buffer_w = 2880,
	                            .buffer_h = 1620,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Should be scaled up significantly
	TEST_ASSERT_GREATER_THAN(256, result.dst_w);
	TEST_ASSERT_GREATER_THAN(224, result.dst_h);
	TEST_ASSERT_GREATER_THAN(1, result.scale);
}

void test_calculate_oversized_device_aspect(void) {
	MinArchScalerInput input = {.src_w = 256,
	                            .src_h = 224,
	                            .src_p = 512,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_ASPECT,
	                            .device_w = 1920,
	                            .device_h = 1080,
	                            .device_p = 3840,
	                            .bpp = 2,
	                            .fit = false,
	                            .buffer_w = 2880,
	                            .buffer_h = 1620,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Aspect should be preserved (letterbox/pillarbox)
	TEST_ASSERT_TRUE(result.aspect > 1.0);
}

///////////////////////////////
// MinArchScaler_calculate - Buffer clamping tests
///////////////////////////////

void test_calculate_clamps_to_buffer(void) {
	MinArchScalerInput input = {.src_w = 256,
	                            .src_h = 224,
	                            .src_p = 512,
	                            .aspect_ratio = 4.0 / 3.0,
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_FULLSCREEN,
	                            .device_w = 1920,
	                            .device_h = 1080,
	                            .device_p = 3840,
	                            .bpp = 2,
	                            .fit = false,
	                            .buffer_w = 800, // Small buffer
	                            .buffer_h = 600,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Should be clamped to buffer size
	TEST_ASSERT_LESS_OR_EQUAL(800, result.dst_w);
	TEST_ASSERT_LESS_OR_EQUAL(600, result.dst_h);
}

///////////////////////////////
// MinArchScaler_calculate - Zero aspect ratio tests
///////////////////////////////

void test_calculate_zero_aspect_uses_source(void) {
	MinArchScalerInput input = {.src_w = 320,
	                            .src_h = 240,
	                            .src_p = 640,
	                            .aspect_ratio = 0, // No aspect ratio specified
	                            .rotation = SCALER_ROTATION_NONE,
	                            .mode = SCALER_MODE_ASPECT,
	                            .device_w = 640,
	                            .device_h = 480,
	                            .device_p = 1280,
	                            .bpp = 2,
	                            .fit = true,
	                            .buffer_w = 960,
	                            .buffer_h = 720,
	                            .hdmi_width = 1280};

	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Should use source aspect ratio (320/240 = 4/3 = 1.33)
	// Use integer comparison with tolerance (multiply by 100 for 2 decimal places)
	int aspect_x100 = (int)(result.aspect * 100);
	TEST_ASSERT_INT_WITHIN(5, 133, aspect_x100); // 1.33 +/- 0.05
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// applyRotation tests
	RUN_TEST(test_applyRotation_none);
	RUN_TEST(test_applyRotation_90_swaps_dimensions);
	RUN_TEST(test_applyRotation_180_no_swap);
	RUN_TEST(test_applyRotation_270_swaps_dimensions);
	RUN_TEST(test_applyRotation_square_unchanged);

	// calculateAspectDimensions tests
	RUN_TEST(test_aspectDimensions_4_3_aspect);
	RUN_TEST(test_aspectDimensions_1_1_aspect);
	RUN_TEST(test_aspectDimensions_16_9_aspect);
	RUN_TEST(test_aspectDimensions_wide_source);

	// clampToBuffer tests
	RUN_TEST(test_clampToBuffer_within_bounds);
	RUN_TEST(test_clampToBuffer_exceeds_width);
	RUN_TEST(test_clampToBuffer_exceeds_height);
	RUN_TEST(test_clampToBuffer_adjusts_offsets);

	// calculate - Native mode tests
	RUN_TEST(test_calculate_native_2x_scale);
	RUN_TEST(test_calculate_native_1x_scale);

	// calculate - Aspect mode tests
	RUN_TEST(test_calculate_aspect_fit_mode);
	RUN_TEST(test_calculate_fullscreen_fit_mode);

	// calculate - Rotation tests
	RUN_TEST(test_calculate_rotation_90);
	RUN_TEST(test_calculate_rotation_270);

	// calculate - Cropped mode tests
	RUN_TEST(test_calculate_cropped_mode);
	RUN_TEST(test_calculate_cropped_on_hdmi_becomes_native);

	// calculate - Forced crop tests
	RUN_TEST(test_calculate_forced_crop_large_source);

	// calculate - Oversized device tests
	RUN_TEST(test_calculate_oversized_device_fullscreen);
	RUN_TEST(test_calculate_oversized_device_aspect);

	// calculate - Buffer clamping tests
	RUN_TEST(test_calculate_clamps_to_buffer);

	// calculate - Zero aspect ratio tests
	RUN_TEST(test_calculate_zero_aspect_uses_source);

	return UNITY_END();
}
