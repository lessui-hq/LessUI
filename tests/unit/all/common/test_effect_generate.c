/**
 * test_effect_generate.c - Unit tests for procedural effect pattern generation
 *
 * Tests the pattern generation functions that create CRT/LCD overlay effects.
 * All functions write directly to pixel buffers with no external dependencies.
 *
 * Test coverage:
 * - EFFECT_generateCRT - CRT aperture grille pattern
 * - EFFECT_generateLine - Simple scanline pattern
 * - EFFECT_generateGrid - LCD pixel grid pattern
 * - EFFECT_generateGridWithColor - Grid with color tinting
 * - EFFECT_generateSlot - Staggered slot mask pattern
 */

#include "unity.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Include the source directly to avoid build dependencies
#include "effect_generate.c"

// Test buffer helper
static uint32_t* create_buffer(int width, int height, int* pitch) {
	*pitch = width * 4;
	uint32_t* buf = calloc(width * height, sizeof(uint32_t));
	return buf;
}

// Extract alpha from ARGB8888 pixel
static uint8_t get_alpha(uint32_t pixel) {
	return (pixel >> 24) & 0xFF;
}

// Extract RGB from ARGB8888 pixel
static void get_rgb(uint32_t pixel, uint8_t* r, uint8_t* g, uint8_t* b) {
	*r = (pixel >> 16) & 0xFF;
	*g = (pixel >> 8) & 0xFF;
	*b = pixel & 0xFF;
}

void setUp(void) {}
void tearDown(void) {}

///////////////////////////////
// EFFECT_generateLine tests
///////////////////////////////

void test_generateLine_null_buffer_returns_safely(void) {
	// Should not crash
	EFFECT_generateLine(NULL, 10, 10, 40, 2);
}

void test_generateLine_invalid_dimensions_returns_safely(void) {
	int pitch;
	uint32_t* buf = create_buffer(10, 10, &pitch);
	// Should not crash with invalid dimensions
	EFFECT_generateLine(buf, 0, 10, pitch, 2);
	EFFECT_generateLine(buf, 10, 0, pitch, 2);
	EFFECT_generateLine(buf, 10, 10, pitch, 0);
	free(buf);
}

void test_generateLine_scale2_has_symmetric_pattern(void) {
	int pitch;
	uint32_t* buf = create_buffer(4, 4, &pitch);
	EFFECT_generateLine(buf, 4, 4, pitch, 2);

	// Scale 2: rows 0,1 are one content pixel, rows 2,3 are the next
	// Pattern should be symmetric: dark edges, bright center
	// Row 0 (top of content pixel) should be darker
	// Row 1 (bottom of content pixel) should be brighter or equal
	uint8_t alpha_row0 = get_alpha(buf[0]);
	uint8_t alpha_row1 = get_alpha(buf[4]);

	// With scale 2, tile_row for y=0 is 0 (dark), y=1 is 1 or 2
	// The exact mapping depends on (pos_in_pixel * 3) / scale
	// For scale 2: y=0 -> (0*3)/2 = 0 (dark), y=1 -> (1*3)/2 = 1 (bright)
	TEST_ASSERT_GREATER_THAN(alpha_row1, alpha_row0); // Row 0 darker than row 1

	free(buf);
}

void test_generateLine_scale3_has_three_distinct_zones(void) {
	int pitch;
	uint32_t* buf = create_buffer(3, 6, &pitch);
	EFFECT_generateLine(buf, 3, 6, pitch, 3);

	// Scale 3: each content pixel spans 3 screen rows
	// y=0: tile_row = (0*3)/3 = 0 (dark edge)
	// y=1: tile_row = (1*3)/3 = 1 (bright center)
	// y=2: tile_row = (2*3)/3 = 2 (dark edge)
	uint8_t alpha_y0 = get_alpha(buf[0 * 3]);     // row 0
	uint8_t alpha_y1 = get_alpha(buf[1 * 3]);     // row 1
	uint8_t alpha_y2 = get_alpha(buf[2 * 3]);     // row 2

	// Dark at edges (0 and 2), bright at center (1)
	TEST_ASSERT_EQUAL_UINT8(alpha_y0, alpha_y2); // Symmetric
	TEST_ASSERT_LESS_THAN(alpha_y0, alpha_y1);   // Center brighter (lower alpha)

	free(buf);
}

void test_generateLine_all_pixels_same_in_row(void) {
	int pitch;
	uint32_t* buf = create_buffer(10, 3, &pitch);
	EFFECT_generateLine(buf, 10, 3, pitch, 3);

	// LINE has no horizontal variation, all pixels in a row should be identical
	for (int y = 0; y < 3; y++) {
		uint32_t first_pixel = buf[y * 10];
		for (int x = 1; x < 10; x++) {
			TEST_ASSERT_EQUAL_UINT32(first_pixel, buf[y * 10 + x]);
		}
	}

	free(buf);
}

///////////////////////////////
// EFFECT_generateCRT tests
///////////////////////////////

void test_generateCRT_null_buffer_returns_safely(void) {
	EFFECT_generateCRT(NULL, 10, 10, 40, 2);
}

void test_generateCRT_scale3_has_horizontal_variation(void) {
	int pitch;
	uint32_t* buf = create_buffer(9, 3, &pitch);
	EFFECT_generateCRT(buf, 9, 3, pitch, 3);

	// CRT has RGB phosphor variation horizontally
	// At row 1 (bright center), columns should have different colors
	uint32_t pixel_col0 = buf[1 * 9 + 0];
	uint32_t pixel_col1 = buf[1 * 9 + 1];
	uint32_t pixel_col2 = buf[1 * 9 + 2];

	// The phosphors should have different RGB values (cyan, blue, red)
	uint8_t r0, g0, b0, r1, g1, b1, r2, g2, b2;
	get_rgb(pixel_col0, &r0, &g0, &b0);
	get_rgb(pixel_col1, &r1, &g1, &b1);
	get_rgb(pixel_col2, &r2, &g2, &b2);

	// Each phosphor should emphasize different channels
	// Col 0: cyan-ish (high G, high B)
	// Col 1: blue (high B)
	// Col 2: red (high R)
	TEST_ASSERT_GREATER_THAN(r0, r2); // Red phosphor (r2) has more red than cyan (r0)
	TEST_ASSERT_GREATER_THAN(r1, b1); // Blue phosphor (b1) has more blue than red (r1)

	free(buf);
}

void test_generateCRT_symmetric_scanlines(void) {
	int pitch;
	uint32_t* buf = create_buffer(3, 6, &pitch);
	EFFECT_generateCRT(buf, 3, 6, pitch, 3);

	// Rows 0 and 2 should have same alpha (both are dark scanline edges)
	uint8_t alpha_y0 = get_alpha(buf[0 * 3]);
	uint8_t alpha_y1 = get_alpha(buf[1 * 3]);
	uint8_t alpha_y2 = get_alpha(buf[2 * 3]);

	TEST_ASSERT_EQUAL_UINT8(alpha_y0, alpha_y2); // Symmetric edges
	TEST_ASSERT_LESS_THAN(alpha_y0, alpha_y1);   // Center brighter

	free(buf);
}

///////////////////////////////
// EFFECT_generateGrid tests
///////////////////////////////

void test_generateGrid_null_buffer_returns_safely(void) {
	EFFECT_generateGrid(NULL, 10, 10, 40, 2);
}

void test_generateGrid_scale2_interior_is_transparent(void) {
	int pitch;
	uint32_t* buf = create_buffer(4, 4, &pitch);
	EFFECT_generateGrid(buf, 4, 4, pitch, 2);

	// Scale 2: cell (1,1) is interior, should be transparent (alpha 0)
	// Position (1,1) is bottom-right of first content pixel
	uint8_t alpha_interior = get_alpha(buf[1 * 4 + 1]);
	TEST_ASSERT_EQUAL_UINT8(0, alpha_interior);

	free(buf);
}

void test_generateGrid_scale2_edges_have_alpha(void) {
	int pitch;
	uint32_t* buf = create_buffer(4, 4, &pitch);
	EFFECT_generateGrid(buf, 4, 4, pitch, 2);

	// Scale 2: left column and top row should have alpha 64
	uint8_t alpha_left = get_alpha(buf[0 * 4 + 0]); // (0,0)
	uint8_t alpha_top = get_alpha(buf[0 * 4 + 1]);  // (0,1)

	TEST_ASSERT_EQUAL_UINT8(64, alpha_left);
	TEST_ASSERT_EQUAL_UINT8(64, alpha_top);

	free(buf);
}

void test_generateGrid_scale3_has_graduated_alpha(void) {
	int pitch;
	uint32_t* buf = create_buffer(6, 6, &pitch);
	EFFECT_generateGrid(buf, 6, 6, pitch, 3);

	// Scale 3+: edges have alpha 102, corners have alpha 153
	// Check bottom-left corner of first content pixel (y=2, x=0)
	uint8_t alpha_corner = get_alpha(buf[2 * 6 + 0]); // bottom-left corner
	uint8_t alpha_edge = get_alpha(buf[2 * 6 + 1]);   // bottom edge, not corner
	uint8_t alpha_interior = get_alpha(buf[1 * 6 + 1]); // interior

	TEST_ASSERT_EQUAL_UINT8(153, alpha_corner);
	TEST_ASSERT_EQUAL_UINT8(102, alpha_edge);
	TEST_ASSERT_EQUAL_UINT8(0, alpha_interior);

	free(buf);
}

///////////////////////////////
// EFFECT_generateGridWithColor tests
///////////////////////////////

void test_generateGridWithColor_black_same_as_grid(void) {
	int pitch;
	uint32_t* buf1 = create_buffer(4, 4, &pitch);
	uint32_t* buf2 = create_buffer(4, 4, &pitch);

	EFFECT_generateGrid(buf1, 4, 4, pitch, 2);
	EFFECT_generateGridWithColor(buf2, 4, 4, pitch, 2, 0);

	// With color=0, should be identical to Grid
	for (int i = 0; i < 16; i++) {
		TEST_ASSERT_EQUAL_UINT32(buf1[i], buf2[i]);
	}

	free(buf1);
	free(buf2);
}

void test_generateGridWithColor_rgb565_conversion(void) {
	int pitch;
	uint32_t* buf = create_buffer(4, 4, &pitch);

	// Pure red in RGB565: 0xF800 (11111 000000 00000)
	EFFECT_generateGridWithColor(buf, 4, 4, pitch, 2, 0xF800);

	// Find a pixel with alpha > 0 to check color
	uint8_t r, g, b;
	get_rgb(buf[0], &r, &g, &b);

	// Should be close to pure red
	TEST_ASSERT_GREATER_THAN(200, r);
	TEST_ASSERT_LESS_THAN(10, g);
	TEST_ASSERT_LESS_THAN(10, b);

	free(buf);
}

void test_generateGridWithColor_green_conversion(void) {
	int pitch;
	uint32_t* buf = create_buffer(4, 4, &pitch);

	// Pure green in RGB565: 0x07E0 (00000 111111 00000)
	EFFECT_generateGridWithColor(buf, 4, 4, pitch, 2, 0x07E0);

	uint8_t r, g, b;
	get_rgb(buf[0], &r, &g, &b);

	// Should be close to pure green
	TEST_ASSERT_LESS_THAN(10, r);
	TEST_ASSERT_GREATER_THAN(200, g);
	TEST_ASSERT_LESS_THAN(10, b);

	free(buf);
}

///////////////////////////////
// EFFECT_generateSlot tests
///////////////////////////////

void test_generateSlot_null_buffer_returns_safely(void) {
	EFFECT_generateSlot(NULL, 10, 10, 40, 2);
}

void test_generateSlot_horizontal_border_at_content_pixel_top(void) {
	int pitch;
	uint32_t* buf = create_buffer(6, 6, &pitch);
	EFFECT_generateSlot(buf, 6, 6, pitch, 3);

	// Row 0 should be horizontal border (all have alpha)
	for (int x = 0; x < 6; x++) {
		uint8_t alpha = get_alpha(buf[0 * 6 + x]);
		TEST_ASSERT_GREATER_THAN(0, alpha);
	}

	// Row 3 (top of second content pixel) should also be border
	for (int x = 0; x < 6; x++) {
		uint8_t alpha = get_alpha(buf[3 * 6 + x]);
		TEST_ASSERT_GREATER_THAN(0, alpha);
	}

	free(buf);
}

void test_generateSlot_vertical_borders_alternate(void) {
	int pitch;
	uint32_t* buf = create_buffer(6, 6, &pitch);
	EFFECT_generateSlot(buf, 6, 6, pitch, 3);

	// Content row 0 (y=0,1,2): left border at x=0
	// Content row 1 (y=3,4,5): right border at x=2 (scale-1)

	// Check row 2 (last row of content row 0, after glow): left border
	uint8_t alpha_left = get_alpha(buf[2 * 6 + 0]);
	uint8_t alpha_mid = get_alpha(buf[2 * 6 + 1]);
	TEST_ASSERT_GREATER_THAN(0, alpha_left);   // Left has border
	TEST_ASSERT_EQUAL_UINT8(0, alpha_mid);     // Middle is clear (interior)

	// Check row 5 (last row of content row 1, after glow): right border
	uint8_t alpha_left2 = get_alpha(buf[5 * 6 + 0]);
	uint8_t alpha_right2 = get_alpha(buf[5 * 6 + 2]);
	TEST_ASSERT_EQUAL_UINT8(0, alpha_left2);   // Left is clear (interior)
	TEST_ASSERT_GREATER_THAN(0, alpha_right2); // Right has border

	free(buf);
}

void test_generateSlot_glow_row_at_scale3(void) {
	int pitch;
	uint32_t* buf = create_buffer(6, 6, &pitch);
	EFFECT_generateSlot(buf, 6, 6, pitch, 3);

	// Row 1 (pos_in_pixel = 1) should have glow (alpha 60) for non-border pixels
	// At scale 3, interior position (1,1) should be glow
	uint8_t alpha_glow = get_alpha(buf[1 * 6 + 1]);
	TEST_ASSERT_EQUAL_UINT8(60, alpha_glow);

	free(buf);
}

void test_generateSlot_no_glow_at_scale2(void) {
	int pitch;
	uint32_t* buf = create_buffer(4, 4, &pitch);
	EFFECT_generateSlot(buf, 4, 4, pitch, 2);

	// At scale 2, there's no glow row (only scale >= 3)
	// Interior pixels should be transparent
	uint8_t alpha_interior = get_alpha(buf[1 * 4 + 1]);
	TEST_ASSERT_EQUAL_UINT8(0, alpha_interior);

	free(buf);
}

void test_generateSlot_graduated_alpha_matches_grid(void) {
	int pitch;
	uint32_t* buf = create_buffer(6, 6, &pitch);
	EFFECT_generateSlot(buf, 6, 6, pitch, 3);

	// Edge alpha should be 102 (same as GRID)
	// Check horizontal border at interior position
	uint8_t alpha_edge = get_alpha(buf[0 * 6 + 1]); // horizontal border, not corner
	TEST_ASSERT_EQUAL_UINT8(102, alpha_edge);

	// Corner alpha should be 153 (same as GRID)
	uint8_t alpha_corner = get_alpha(buf[0 * 6 + 0]); // corner
	TEST_ASSERT_EQUAL_UINT8(153, alpha_corner);

	free(buf);
}

///////////////////////////////
// Edge case tests
///////////////////////////////

void test_all_functions_handle_1x1_buffer(void) {
	int pitch;
	uint32_t* buf = create_buffer(1, 1, &pitch);

	// Should not crash
	EFFECT_generateLine(buf, 1, 1, pitch, 2);
	EFFECT_generateCRT(buf, 1, 1, pitch, 2);
	EFFECT_generateGrid(buf, 1, 1, pitch, 2);
	EFFECT_generateSlot(buf, 1, 1, pitch, 2);

	free(buf);
}

void test_all_functions_handle_scale1(void) {
	int pitch;
	uint32_t* buf = create_buffer(4, 4, &pitch);

	// Scale 1 should work (though unusual)
	EFFECT_generateLine(buf, 4, 4, pitch, 1);
	EFFECT_generateCRT(buf, 4, 4, pitch, 1);
	EFFECT_generateGrid(buf, 4, 4, pitch, 1);
	EFFECT_generateSlot(buf, 4, 4, pitch, 1);

	free(buf);
}

void test_all_functions_handle_large_scale(void) {
	int pitch;
	uint32_t* buf = create_buffer(20, 20, &pitch);

	// Large scale should work
	EFFECT_generateLine(buf, 20, 20, pitch, 10);
	EFFECT_generateCRT(buf, 20, 20, pitch, 10);
	EFFECT_generateGrid(buf, 20, 20, pitch, 10);
	EFFECT_generateSlot(buf, 20, 20, pitch, 10);

	free(buf);
}

int main(void) {
	UNITY_BEGIN();

	// EFFECT_generateLine
	RUN_TEST(test_generateLine_null_buffer_returns_safely);
	RUN_TEST(test_generateLine_invalid_dimensions_returns_safely);
	RUN_TEST(test_generateLine_scale2_has_symmetric_pattern);
	RUN_TEST(test_generateLine_scale3_has_three_distinct_zones);
	RUN_TEST(test_generateLine_all_pixels_same_in_row);

	// EFFECT_generateCRT
	RUN_TEST(test_generateCRT_null_buffer_returns_safely);
	RUN_TEST(test_generateCRT_scale3_has_horizontal_variation);
	RUN_TEST(test_generateCRT_symmetric_scanlines);

	// EFFECT_generateGrid
	RUN_TEST(test_generateGrid_null_buffer_returns_safely);
	RUN_TEST(test_generateGrid_scale2_interior_is_transparent);
	RUN_TEST(test_generateGrid_scale2_edges_have_alpha);
	RUN_TEST(test_generateGrid_scale3_has_graduated_alpha);

	// EFFECT_generateGridWithColor
	RUN_TEST(test_generateGridWithColor_black_same_as_grid);
	RUN_TEST(test_generateGridWithColor_rgb565_conversion);
	RUN_TEST(test_generateGridWithColor_green_conversion);

	// EFFECT_generateSlot
	RUN_TEST(test_generateSlot_null_buffer_returns_safely);
	RUN_TEST(test_generateSlot_horizontal_border_at_content_pixel_top);
	RUN_TEST(test_generateSlot_vertical_borders_alternate);
	RUN_TEST(test_generateSlot_glow_row_at_scale3);
	RUN_TEST(test_generateSlot_no_glow_at_scale2);
	RUN_TEST(test_generateSlot_graduated_alpha_matches_grid);

	// Edge cases
	RUN_TEST(test_all_functions_handle_1x1_buffer);
	RUN_TEST(test_all_functions_handle_scale1);
	RUN_TEST(test_all_functions_handle_large_scale);

	return UNITY_END();
}
