/**
 * test_ui_layout.c - Unit tests for Display Points (DP) UI layout calculations
 *
 * Tests the UI_initLayout() function which calculates:
 * - dp_scale from screen PPI
 * - Optimal pill height to fill screen
 * - Row count (6-8)
 * - Derived button sizes
 */

#include "unity.h"

#include <math.h>

// Define minimal platform stubs (we don't need full platform.h)
#define PLATFORM "test"

// UI Layout types - copy from api.h to avoid SDL dependencies
typedef struct UI_Layout {
	int pill_height;
	int row_count;
	int padding;
	int text_baseline;
	int button_size;
	int button_margin;
	int button_padding;
} UI_Layout;

// External globals and function (implemented in api.c, but we'll redefine for testing)
float gfx_dp_scale = 2.0f;
UI_Layout ui = {
    .pill_height = 30,
    .row_count = 6,
    .padding = 10,
    .text_baseline = 6,
    .button_size = 20,
    .button_margin = 5,
    .button_padding = 12,
};

// DP macro for testing
#define DP(x) ((int)((x) *gfx_dp_scale + 0.5f))

// Copy UI_initLayout implementation for testing
void UI_initLayout(int screen_width, int screen_height, float diagonal_inches) {
	// Calculate PPI and dp_scale
	float diagonal_px = sqrtf((float)(screen_width * screen_width + screen_height * screen_height));
	float ppi = diagonal_px / diagonal_inches;
	gfx_dp_scale = ppi / 160.0f;

	// Bounds for layout calculation
	const int MIN_PILL = 28;
	const int MAX_PILL = 32;
	const int MIN_ROWS = 6;
	const int MAX_ROWS = 8;
	const int DEFAULT_PADDING = 10;

	// Calculate available height in dp
	int screen_height_dp = (int)(screen_height / gfx_dp_scale + 0.5f);
	int available_dp = screen_height_dp - (DEFAULT_PADDING * 2);

	// Find best row count where pill height fits in acceptable range
	int best_rows = MIN_ROWS;
	int best_pill = 30;

	// Try to fit as many rows as possible
	for (int rows = MAX_ROWS; rows >= MIN_ROWS; rows--) {
		int total_rows = rows + 2; // +1 header spacing, +1 footer
		int pill = available_dp / total_rows;

		if (pill >= MIN_PILL && pill <= MAX_PILL) {
			// Found a good fit - use it
			best_rows = rows;
			best_pill = pill;
			break;
		} else if (pill < MIN_PILL && rows > MIN_ROWS) {
			// Too many rows, not enough space - try fewer rows
			continue;
		} else if (rows == MIN_ROWS) {
			// Reached minimum rows - use closest valid pill size
			if (pill < MIN_PILL) {
				best_pill = MIN_PILL;
			} else if (pill > MAX_PILL) {
				best_pill = MAX_PILL;
			} else {
				best_pill = pill;
			}
			best_rows = rows;
		}
	}

	// Store calculated values
	ui.pill_height = best_pill;
	ui.row_count = best_rows;
	ui.padding = DEFAULT_PADDING;

	// Derived proportional sizes
	ui.button_size = (best_pill * 2) / 3;
	ui.button_margin = (best_pill - ui.button_size) / 2;
	ui.button_padding = (best_pill * 2) / 5;
	ui.text_baseline = (best_pill * 2) / 10;
}

void setUp(void) {
	// Reset to defaults before each test
	gfx_dp_scale = 2.0f;
	ui.pill_height = 30;
	ui.row_count = 6;
	ui.padding = 10;
	ui.button_size = 20;
	ui.button_margin = 5;
	ui.button_padding = 12;
	ui.text_baseline = 6;
}

void tearDown(void) {
}

///////////////////////////////
// DP Scale Calculation Tests
///////////////////////////////

void test_dp_scale_miyoomini_480p(void) {
	// Miyoo Mini: 640x480 @ 2.8" → PPI = 286 → dp_scale ≈ 1.79
	UI_initLayout(640, 480, 2.8f);

	float expected_ppi = sqrtf(640 * 640 + 480 * 480) / 2.8f; // ≈ 286
	float expected_dp_scale = expected_ppi / 160.0f;           // ≈ 1.79

	TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_dp_scale, gfx_dp_scale);
}

void test_dp_scale_miyoomini_560p(void) {
	// Miyoo Mini 560p: 752x560 @ 2.8" → PPI = 334 → dp_scale ≈ 2.09
	UI_initLayout(752, 560, 2.8f);

	float expected_ppi = sqrtf(752 * 752 + 560 * 560) / 2.8f;
	float expected_dp_scale = expected_ppi / 160.0f;

	TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_dp_scale, gfx_dp_scale);
}

void test_dp_scale_trimuismart(void) {
	// Trimui Smart: 320x240 @ 2.4" → PPI = 167 → dp_scale ≈ 1.04
	UI_initLayout(320, 240, 2.4f);

	float expected_ppi = sqrtf(320 * 320 + 240 * 240) / 2.4f;
	float expected_dp_scale = expected_ppi / 160.0f;

	TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_dp_scale, gfx_dp_scale);
}

void test_dp_scale_tg5040_brick(void) {
	// TG5040 Brick: 1024x768 @ 3.2" → PPI = 400 → dp_scale ≈ 2.50
	UI_initLayout(1024, 768, 3.2f);

	float expected_ppi = sqrtf(1024 * 1024 + 768 * 768) / 3.2f;
	float expected_dp_scale = expected_ppi / 160.0f;

	TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_dp_scale, gfx_dp_scale);
}

void test_dp_scale_rg35xx(void) {
	// RG35XX: 640x480 @ 3.5" → PPI = 229 → dp_scale ≈ 1.43
	UI_initLayout(640, 480, 3.5f);

	float expected_ppi = sqrtf(640 * 640 + 480 * 480) / 3.5f;
	float expected_dp_scale = expected_ppi / 160.0f;

	TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_dp_scale, gfx_dp_scale);
}

///////////////////////////////
// Row Count Calculation Tests
///////////////////////////////

void test_row_count_small_screen(void) {
	// Small screen (240px logical height) should get 6 rows
	UI_initLayout(320, 240, 2.4f);

	TEST_ASSERT_EQUAL_INT(6, ui.row_count);
}

void test_row_count_medium_screen(void) {
	// Medium screen (240px logical at 2x = 480px physical) should get 6 rows
	UI_initLayout(640, 480, 2.8f);

	// At dp_scale ≈ 1.79, 480px / 1.79 ≈ 268 logical pixels
	// Should fit 6-8 rows depending on pill calculation
	TEST_ASSERT_TRUE(ui.row_count >= 6 && ui.row_count <= 8);
}

void test_row_count_tall_screen(void) {
	// Tall screen (560p @ 2.8") - logical ≈ 268dp, fits 6 rows optimally
	UI_initLayout(752, 560, 2.8f);

	// 268dp - 20 (padding) = 248dp available
	// 8 rows would need 24.8dp pills (too small)
	// 6 rows needs 31dp pills (good fit)
	TEST_ASSERT_EQUAL_INT(6, ui.row_count);
}

void test_row_count_large_screen(void) {
	// Large screen (720px @ 4.95") - logical ≈ 387dp
	UI_initLayout(1280, 720, 4.95f);

	// 387dp - 20 (padding) = 367dp available
	// 8 rows would need 36.7dp pills (too big, caps at 32)
	// 6 rows needs 45.9dp pills (way too big, caps at 32)
	// Algorithm will use 6 rows with 32dp max pill
	TEST_ASSERT_EQUAL_INT(6, ui.row_count);
	TEST_ASSERT_EQUAL_INT(32, ui.pill_height);
}

///////////////////////////////
// Pill Height Calculation Tests
///////////////////////////////

void test_pill_height_within_bounds(void) {
	// All platforms should have pill height 28-32dp
	UI_initLayout(640, 480, 2.8f);

	TEST_ASSERT_TRUE(ui.pill_height >= 28);
	TEST_ASSERT_TRUE(ui.pill_height <= 32);
}

void test_pill_height_fills_screen(void) {
	// Pill height should be calculated to fill available space
	UI_initLayout(640, 480, 2.8f);

	// Calculate logical height
	int logical_height = (int)(480 / gfx_dp_scale + 0.5f);
	int available = logical_height - (ui.padding * 2);
	int total_rows = ui.row_count + 2; // content + header + footer

	// Pill height should approximately fill the space
	int expected_pill = available / total_rows;
	TEST_ASSERT_INT_WITHIN(2, expected_pill, ui.pill_height);
}

///////////////////////////////
// Derived Sizes Tests
///////////////////////////////

void test_button_size_proportional(void) {
	UI_initLayout(640, 480, 2.8f);

	// button_size should be ~2/3 of pill_height
	int expected_button = (ui.pill_height * 2) / 3;
	TEST_ASSERT_EQUAL_INT(expected_button, ui.button_size);
}

void test_button_margin_centers_button(void) {
	UI_initLayout(640, 480, 2.8f);

	// button_margin should center button in pill
	int expected_margin = (ui.pill_height - ui.button_size) / 2;
	TEST_ASSERT_EQUAL_INT(expected_margin, ui.button_margin);
}

void test_button_padding_proportional(void) {
	UI_initLayout(640, 480, 2.8f);

	// button_padding should be ~2/5 of pill_height (12 for 30dp)
	int expected_padding = (ui.pill_height * 2) / 5;
	TEST_ASSERT_EQUAL_INT(expected_padding, ui.button_padding);
}

void test_text_baseline_proportional(void) {
	UI_initLayout(640, 480, 2.8f);

	// text_baseline should scale proportionally (~4 for 30dp)
	int expected_baseline = (4 * ui.pill_height + 15) / 30;
	TEST_ASSERT_EQUAL_INT(expected_baseline, ui.text_baseline);
}

///////////////////////////////
// Padding Tests
///////////////////////////////

void test_padding_consistent(void) {
	// Padding should be consistent across all screen sizes
	UI_initLayout(320, 240, 2.4f);
	TEST_ASSERT_EQUAL_INT(10, ui.padding);

	UI_initLayout(640, 480, 2.8f);
	TEST_ASSERT_EQUAL_INT(10, ui.padding);

	UI_initLayout(1280, 720, 4.95f);
	TEST_ASSERT_EQUAL_INT(10, ui.padding);
}

///////////////////////////////
// DP Macro Tests
///////////////////////////////

void test_DP_macro_rounds_correctly(void) {
	gfx_dp_scale = 1.79f;

	// DP(30) = (int)(30 * 1.79 + 0.5) = (int)(53.7 + 0.5) = 54
	TEST_ASSERT_EQUAL_INT(54, DP(30));

	// DP(10) = (int)(10 * 1.79 + 0.5) = (int)(17.9 + 0.5) = 18
	TEST_ASSERT_EQUAL_INT(18, DP(10));
}

void test_DP_macro_handles_fractions(void) {
	gfx_dp_scale = 2.5f;

	// DP(30) = (int)(30 * 2.5 + 0.5) = 75
	TEST_ASSERT_EQUAL_INT(75, DP(30));

	// DP(31) = (int)(31 * 2.5 + 0.5) = 78
	TEST_ASSERT_EQUAL_INT(78, DP(31));
}

///////////////////////////////
// Edge Case Tests
///////////////////////////////

void test_extremely_small_screen(void) {
	// Tiny screen should still work, even if it can't fit 6 rows perfectly
	UI_initLayout(240, 160, 2.0f);

	TEST_ASSERT_TRUE(ui.row_count >= 6);
	TEST_ASSERT_TRUE(ui.pill_height >= 28);
}

void test_extremely_large_screen(void) {
	// Large screen should cap at 8 rows with max pill size
	UI_initLayout(1920, 1080, 7.0f);

	TEST_ASSERT_TRUE(ui.row_count <= 8);
	TEST_ASSERT_TRUE(ui.pill_height <= 32);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// DP scale calculations
	RUN_TEST(test_dp_scale_miyoomini_480p);
	RUN_TEST(test_dp_scale_miyoomini_560p);
	RUN_TEST(test_dp_scale_trimuismart);
	RUN_TEST(test_dp_scale_tg5040_brick);
	RUN_TEST(test_dp_scale_rg35xx);

	// Row count calculations
	RUN_TEST(test_row_count_small_screen);
	RUN_TEST(test_row_count_medium_screen);
	RUN_TEST(test_row_count_tall_screen);
	RUN_TEST(test_row_count_large_screen);

	// Pill height calculations
	RUN_TEST(test_pill_height_within_bounds);
	RUN_TEST(test_pill_height_fills_screen);

	// Derived sizes
	RUN_TEST(test_button_size_proportional);
	RUN_TEST(test_button_margin_centers_button);
	RUN_TEST(test_button_padding_proportional);
	RUN_TEST(test_text_baseline_proportional);

	// Padding
	RUN_TEST(test_padding_consistent);

	// DP macro
	RUN_TEST(test_DP_macro_rounds_correctly);
	RUN_TEST(test_DP_macro_handles_fractions);

	// Edge cases
	RUN_TEST(test_extremely_small_screen);
	RUN_TEST(test_extremely_large_screen);

	return UNITY_END();
}
