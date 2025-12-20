/**
 * test_render_common.c - Unit tests for render_common.c
 *
 * Tests the shared rendering utilities:
 * - RGB565 <-> RGB888 color conversion
 * - Hard scale factor calculation
 * - Destination rectangle calculation (aspect ratio, centering)
 */

#include "unity.h"

#include <stdint.h>
#include <string.h>

///////////////////////////////
// Local Type Definitions
///////////////////////////////

// Minimal GFX_Renderer definition for testing (matches api.h structure)
typedef struct GFX_Renderer {
	void* src;
	void* dst;
	void* blit;
	double aspect;
	int scale;
	int visual_scale;
	int true_w;
	int true_h;
	int src_x;
	int src_y;
	int src_w;
	int src_h;
	int src_p;
	int dst_x;
	int dst_y;
	int dst_w;
	int dst_h;
	int dst_p;
} GFX_Renderer;

// Destination rectangle result (matches render_common.h)
typedef struct RenderDestRect {
	int x;
	int y;
	int w;
	int h;
} RenderDestRect;

///////////////////////////////
// Function Declarations (from render_common.h)
///////////////////////////////

RenderDestRect RENDER_calcDestRect(const GFX_Renderer* renderer, int device_w, int device_h);
int RENDER_calcHardScale(int src_w, int src_h, int device_w, int device_h);
void RENDER_rgb565ToRgb888(uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b);
uint16_t RENDER_rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b);

///////////////////////////////
// Test Fixtures
///////////////////////////////

void setUp(void) {
	// No setup needed - all functions are pure
}

void tearDown(void) {
	// No teardown needed
}

///////////////////////////////
// RGB565 to RGB888 Tests
///////////////////////////////

void test_rgb565_to_rgb888_black(void) {
	uint8_t r, g, b;
	RENDER_rgb565ToRgb888(0x0000, &r, &g, &b);

	TEST_ASSERT_EQUAL_UINT8(0, r);
	TEST_ASSERT_EQUAL_UINT8(0, g);
	TEST_ASSERT_EQUAL_UINT8(0, b);
}

void test_rgb565_to_rgb888_white(void) {
	uint8_t r, g, b;
	// RGB565 white: 11111 111111 11111 = 0xFFFF
	RENDER_rgb565ToRgb888(0xFFFF, &r, &g, &b);

	TEST_ASSERT_EQUAL_UINT8(255, r);
	TEST_ASSERT_EQUAL_UINT8(255, g);
	TEST_ASSERT_EQUAL_UINT8(255, b);
}

void test_rgb565_to_rgb888_pure_red(void) {
	uint8_t r, g, b;
	// RGB565 red: 11111 000000 00000 = 0xF800
	RENDER_rgb565ToRgb888(0xF800, &r, &g, &b);

	TEST_ASSERT_EQUAL_UINT8(255, r);
	TEST_ASSERT_EQUAL_UINT8(0, g);
	TEST_ASSERT_EQUAL_UINT8(0, b);
}

void test_rgb565_to_rgb888_pure_green(void) {
	uint8_t r, g, b;
	// RGB565 green: 00000 111111 00000 = 0x07E0
	RENDER_rgb565ToRgb888(0x07E0, &r, &g, &b);

	TEST_ASSERT_EQUAL_UINT8(0, r);
	TEST_ASSERT_EQUAL_UINT8(255, g);
	TEST_ASSERT_EQUAL_UINT8(0, b);
}

void test_rgb565_to_rgb888_pure_blue(void) {
	uint8_t r, g, b;
	// RGB565 blue: 00000 000000 11111 = 0x001F
	RENDER_rgb565ToRgb888(0x001F, &r, &g, &b);

	TEST_ASSERT_EQUAL_UINT8(0, r);
	TEST_ASSERT_EQUAL_UINT8(0, g);
	TEST_ASSERT_EQUAL_UINT8(255, b);
}

void test_rgb565_to_rgb888_mid_gray(void) {
	uint8_t r, g, b;
	// RGB565 mid-gray: 10000 100000 10000 = 0x8410
	RENDER_rgb565ToRgb888(0x8410, &r, &g, &b);

	// 16 in 5-bit -> (16 << 3) | (16 >> 2) = 128 + 4 = 132
	// 32 in 6-bit -> (32 << 2) | (32 >> 4) = 128 + 2 = 130
	TEST_ASSERT_EQUAL_UINT8(132, r);
	TEST_ASSERT_EQUAL_UINT8(130, g);
	TEST_ASSERT_EQUAL_UINT8(132, b);
}

///////////////////////////////
// RGB888 to RGB565 Tests
///////////////////////////////

void test_rgb888_to_rgb565_black(void) {
	uint16_t result = RENDER_rgb888ToRgb565(0, 0, 0);
	TEST_ASSERT_EQUAL_HEX16(0x0000, result);
}

void test_rgb888_to_rgb565_white(void) {
	uint16_t result = RENDER_rgb888ToRgb565(255, 255, 255);
	TEST_ASSERT_EQUAL_HEX16(0xFFFF, result);
}

void test_rgb888_to_rgb565_pure_red(void) {
	uint16_t result = RENDER_rgb888ToRgb565(255, 0, 0);
	TEST_ASSERT_EQUAL_HEX16(0xF800, result);
}

void test_rgb888_to_rgb565_pure_green(void) {
	uint16_t result = RENDER_rgb888ToRgb565(0, 255, 0);
	TEST_ASSERT_EQUAL_HEX16(0x07E0, result);
}

void test_rgb888_to_rgb565_pure_blue(void) {
	uint16_t result = RENDER_rgb888ToRgb565(0, 0, 255);
	TEST_ASSERT_EQUAL_HEX16(0x001F, result);
}

void test_rgb888_to_rgb565_mid_gray(void) {
	// 128 in 8-bit -> 128 >> 3 = 16 for 5-bit
	// 128 in 8-bit -> 128 >> 2 = 32 for 6-bit
	uint16_t result = RENDER_rgb888ToRgb565(128, 128, 128);
	// Expected: (16 << 11) | (32 << 5) | 16 = 0x8410
	TEST_ASSERT_EQUAL_HEX16(0x8410, result);
}

///////////////////////////////
// Roundtrip Conversion Tests
///////////////////////////////

void test_rgb_roundtrip_black(void) {
	uint16_t rgb565 = RENDER_rgb888ToRgb565(0, 0, 0);
	uint8_t r, g, b;
	RENDER_rgb565ToRgb888(rgb565, &r, &g, &b);

	TEST_ASSERT_EQUAL_UINT8(0, r);
	TEST_ASSERT_EQUAL_UINT8(0, g);
	TEST_ASSERT_EQUAL_UINT8(0, b);
}

void test_rgb_roundtrip_white(void) {
	uint16_t rgb565 = RENDER_rgb888ToRgb565(255, 255, 255);
	uint8_t r, g, b;
	RENDER_rgb565ToRgb888(rgb565, &r, &g, &b);

	TEST_ASSERT_EQUAL_UINT8(255, r);
	TEST_ASSERT_EQUAL_UINT8(255, g);
	TEST_ASSERT_EQUAL_UINT8(255, b);
}

void test_rgb_roundtrip_primary_colors(void) {
	uint8_t r, g, b;

	// Red
	RENDER_rgb565ToRgb888(RENDER_rgb888ToRgb565(255, 0, 0), &r, &g, &b);
	TEST_ASSERT_EQUAL_UINT8(255, r);
	TEST_ASSERT_EQUAL_UINT8(0, g);
	TEST_ASSERT_EQUAL_UINT8(0, b);

	// Green
	RENDER_rgb565ToRgb888(RENDER_rgb888ToRgb565(0, 255, 0), &r, &g, &b);
	TEST_ASSERT_EQUAL_UINT8(0, r);
	TEST_ASSERT_EQUAL_UINT8(255, g);
	TEST_ASSERT_EQUAL_UINT8(0, b);

	// Blue
	RENDER_rgb565ToRgb888(RENDER_rgb888ToRgb565(0, 0, 255), &r, &g, &b);
	TEST_ASSERT_EQUAL_UINT8(0, r);
	TEST_ASSERT_EQUAL_UINT8(0, g);
	TEST_ASSERT_EQUAL_UINT8(255, b);
}

///////////////////////////////
// Hard Scale Factor Tests
///////////////////////////////

void test_hard_scale_native_resolution(void) {
	// Source same as device - no upscale needed
	int scale = RENDER_calcHardScale(640, 480, 640, 480);
	TEST_ASSERT_EQUAL(1, scale);
}

void test_hard_scale_larger_than_device(void) {
	// Source larger than device - no upscale needed
	int scale = RENDER_calcHardScale(1920, 1080, 640, 480);
	TEST_ASSERT_EQUAL(1, scale);
}

void test_hard_scale_small_source(void) {
	// Game Boy resolution on 640x480 screen
	int scale = RENDER_calcHardScale(160, 144, 640, 480);
	TEST_ASSERT_EQUAL(4, scale);
}

void test_hard_scale_nes_resolution(void) {
	// NES resolution on 640x480 screen
	int scale = RENDER_calcHardScale(256, 240, 640, 480);
	TEST_ASSERT_EQUAL(4, scale);
}

void test_hard_scale_snes_resolution(void) {
	// SNES resolution on 640x480 screen
	int scale = RENDER_calcHardScale(256, 224, 640, 480);
	TEST_ASSERT_EQUAL(4, scale);
}

void test_hard_scale_width_larger_height_smaller(void) {
	// Only one dimension larger than device
	int scale = RENDER_calcHardScale(800, 400, 640, 480);
	TEST_ASSERT_EQUAL(4, scale);
}

void test_hard_scale_width_smaller_height_larger(void) {
	// Only one dimension larger than device
	int scale = RENDER_calcHardScale(400, 600, 640, 480);
	TEST_ASSERT_EQUAL(4, scale);
}

///////////////////////////////
// Destination Rectangle Tests - Native Scaling (aspect == 0)
///////////////////////////////

void test_dest_rect_native_scaling_centered(void) {
	GFX_Renderer renderer = {
	    .src_w = 160,
	    .src_h = 144,
	    .scale = 2,
	    .visual_scale = 2,
	    .aspect = 0, // Native scaling
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// 160*2 = 320, centered in 640: (640-320)/2 = 160
	// 144*2 = 288, centered in 480: (480-288)/2 = 96
	TEST_ASSERT_EQUAL(160, dst.x);
	TEST_ASSERT_EQUAL(96, dst.y);
	TEST_ASSERT_EQUAL(320, dst.w);
	TEST_ASSERT_EQUAL(288, dst.h);
}

void test_dest_rect_native_scaling_1x(void) {
	GFX_Renderer renderer = {
	    .src_w = 256,
	    .src_h = 240,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 0,
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// 256*1 = 256, centered in 640: (640-256)/2 = 192
	// 240*1 = 240, centered in 480: (480-240)/2 = 120
	TEST_ASSERT_EQUAL(192, dst.x);
	TEST_ASSERT_EQUAL(120, dst.y);
	TEST_ASSERT_EQUAL(256, dst.w);
	TEST_ASSERT_EQUAL(240, dst.h);
}

void test_dest_rect_native_scaling_fills_screen(void) {
	GFX_Renderer renderer = {
	    .src_w = 320,
	    .src_h = 240,
	    .scale = 2,
	    .visual_scale = 2,
	    .aspect = 0,
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// 320*2 = 640, 240*2 = 480 - exactly fills screen
	TEST_ASSERT_EQUAL(0, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(640, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

///////////////////////////////
// Destination Rectangle Tests - Fullscreen Stretch (aspect < 0)
///////////////////////////////

void test_dest_rect_fullscreen_stretch(void) {
	GFX_Renderer renderer = {
	    .src_w = 160,
	    .src_h = 144,
	    .scale = 2,
	    .visual_scale = 2,
	    .aspect = -1, // Fullscreen stretch
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// Fullscreen stretch always fills entire screen
	TEST_ASSERT_EQUAL(0, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(640, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

void test_dest_rect_fullscreen_stretch_any_aspect(void) {
	GFX_Renderer renderer = {
	    .src_w = 256,
	    .src_h = 224,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = -999, // Any negative value
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 854, 480);

	// Should fill 16:9 screen
	TEST_ASSERT_EQUAL(0, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(854, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

///////////////////////////////
// Destination Rectangle Tests - Aspect Ratio Preserving (aspect > 0)
///////////////////////////////

void test_dest_rect_aspect_4_3_on_4_3_screen(void) {
	GFX_Renderer renderer = {
	    .src_w = 320,
	    .src_h = 240,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 4.0 / 3.0, // 4:3 content
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// 4:3 on 4:3 screen should fill it
	TEST_ASSERT_EQUAL(0, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(640, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

void test_dest_rect_aspect_4_3_on_16_9_screen(void) {
	GFX_Renderer renderer = {
	    .src_w = 320,
	    .src_h = 240,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 4.0 / 3.0, // 4:3 content
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 854, 480);

	// 4:3 on 16:9 should pillarbox (black bars on sides)
	// Height fills: 480
	// Width for 4:3: 480 * 4/3 = 640
	// Centered: (854-640)/2 = 107
	TEST_ASSERT_EQUAL(107, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(640, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

void test_dest_rect_aspect_16_9_on_4_3_screen(void) {
	GFX_Renderer renderer = {
	    .src_w = 854,
	    .src_h = 480,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 16.0 / 9.0, // 16:9 content
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// 16:9 on 4:3 should letterbox (black bars top/bottom)
	// Width fills: 640
	// Height for 16:9: 640 * 9/16 = 360
	// Centered: (480-360)/2 = 60
	TEST_ASSERT_EQUAL(0, dst.x);
	TEST_ASSERT_EQUAL(60, dst.y);
	TEST_ASSERT_EQUAL(640, dst.w);
	TEST_ASSERT_EQUAL(360, dst.h);
}

void test_dest_rect_aspect_1_1_square(void) {
	GFX_Renderer renderer = {
	    .src_w = 256,
	    .src_h = 256,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 1.0, // Square content
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// Square on 4:3 should pillarbox
	// Height fills: 480
	// Width for 1:1: 480
	// Centered: (640-480)/2 = 80
	TEST_ASSERT_EQUAL(80, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(480, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

void test_dest_rect_aspect_game_boy(void) {
	GFX_Renderer renderer = {
	    .src_w = 160,
	    .src_h = 144,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 10.0 / 9.0, // Game Boy aspect ratio
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// 10:9 on 4:3
	// Height fills: 480
	// Width for 10:9: 480 * 10/9 = 533
	// Centered: (640-533)/2 = 53
	TEST_ASSERT_EQUAL(53, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(533, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_dest_rect_very_wide_aspect(void) {
	GFX_Renderer renderer = {
	    .src_w = 320,
	    .src_h = 100,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 3.2, // Very wide (3.2:1)
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// Very wide content should letterbox heavily
	// Try fitting height first: 480 * 3.2 = 1536 (too wide!)
	// Fit width instead: 640 / 3.2 = 200
	// Centered: (480-200)/2 = 140
	TEST_ASSERT_EQUAL(0, dst.x);
	TEST_ASSERT_EQUAL(140, dst.y);
	TEST_ASSERT_EQUAL(640, dst.w);
	TEST_ASSERT_EQUAL(200, dst.h);
}

void test_dest_rect_very_tall_aspect(void) {
	GFX_Renderer renderer = {
	    .src_w = 100,
	    .src_h = 320,
	    .scale = 1,
	    .visual_scale = 1,
	    .aspect = 0.3125, // Very tall (1:3.2)
	};

	RenderDestRect dst = RENDER_calcDestRect(&renderer, 640, 480);

	// Very tall content should pillarbox heavily
	// Height fills: 480
	// Width for 0.3125: 480 * 0.3125 = 150
	// Centered: (640-150)/2 = 245
	TEST_ASSERT_EQUAL(245, dst.x);
	TEST_ASSERT_EQUAL(0, dst.y);
	TEST_ASSERT_EQUAL(150, dst.w);
	TEST_ASSERT_EQUAL(480, dst.h);
}

///////////////////////////////
// Main Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// RGB565 to RGB888 tests
	RUN_TEST(test_rgb565_to_rgb888_black);
	RUN_TEST(test_rgb565_to_rgb888_white);
	RUN_TEST(test_rgb565_to_rgb888_pure_red);
	RUN_TEST(test_rgb565_to_rgb888_pure_green);
	RUN_TEST(test_rgb565_to_rgb888_pure_blue);
	RUN_TEST(test_rgb565_to_rgb888_mid_gray);

	// RGB888 to RGB565 tests
	RUN_TEST(test_rgb888_to_rgb565_black);
	RUN_TEST(test_rgb888_to_rgb565_white);
	RUN_TEST(test_rgb888_to_rgb565_pure_red);
	RUN_TEST(test_rgb888_to_rgb565_pure_green);
	RUN_TEST(test_rgb888_to_rgb565_pure_blue);
	RUN_TEST(test_rgb888_to_rgb565_mid_gray);

	// Roundtrip tests
	RUN_TEST(test_rgb_roundtrip_black);
	RUN_TEST(test_rgb_roundtrip_white);
	RUN_TEST(test_rgb_roundtrip_primary_colors);

	// Hard scale factor tests
	RUN_TEST(test_hard_scale_native_resolution);
	RUN_TEST(test_hard_scale_larger_than_device);
	RUN_TEST(test_hard_scale_small_source);
	RUN_TEST(test_hard_scale_nes_resolution);
	RUN_TEST(test_hard_scale_snes_resolution);
	RUN_TEST(test_hard_scale_width_larger_height_smaller);
	RUN_TEST(test_hard_scale_width_smaller_height_larger);

	// Destination rectangle - native scaling
	RUN_TEST(test_dest_rect_native_scaling_centered);
	RUN_TEST(test_dest_rect_native_scaling_1x);
	RUN_TEST(test_dest_rect_native_scaling_fills_screen);

	// Destination rectangle - fullscreen stretch
	RUN_TEST(test_dest_rect_fullscreen_stretch);
	RUN_TEST(test_dest_rect_fullscreen_stretch_any_aspect);

	// Destination rectangle - aspect ratio preserving
	RUN_TEST(test_dest_rect_aspect_4_3_on_4_3_screen);
	RUN_TEST(test_dest_rect_aspect_4_3_on_16_9_screen);
	RUN_TEST(test_dest_rect_aspect_16_9_on_4_3_screen);
	RUN_TEST(test_dest_rect_aspect_1_1_square);
	RUN_TEST(test_dest_rect_aspect_game_boy);

	// Edge cases
	RUN_TEST(test_dest_rect_very_wide_aspect);
	RUN_TEST(test_dest_rect_very_tall_aspect);

	return UNITY_END();
}
