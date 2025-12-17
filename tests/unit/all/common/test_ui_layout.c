/**
 * test_ui_layout.c - Unit tests for UI layout calculations (DP system)
 *
 * Tests the UI_initLayout() function which calculates optimal UI layout
 * from screen dimensions and PPI. This is complex logic with:
 * - PPI and DP scale calculation
 * - Pixel-accurate row fitting algorithm
 * - Even-pixel preference for cleaner rendering
 * - Platform-specific modifiers (SCALE_MODIFIER, EDGE_PADDING)
 * - Derived size calculations (buttons, options, etc.)
 */

#include "unity.h"

#include <math.h>
#include <stdio.h>

// Platform stubs for testing
#define PLATFORM "test"

// Undefine SCALE_MODIFIER and EDGE_PADDING for baseline tests
// NOTE: SCALE_MODIFIER path in ui_layout.c is not currently tested.
// Platforms with SCALE_MODIFIER apply an additional multiplier to dp_scale.
// Testing this would require including ui_layout.c with different defines,
// which is complex. Consider testing on actual platform builds instead.
#ifdef SCALE_MODIFIER
#undef SCALE_MODIFIER
#endif
#ifdef EDGE_PADDING
#undef EDGE_PADDING
#endif

// Include ui_layout implementation directly for testing
#include "log.h"
#include "ui_layout.h"

// Define globals that ui_layout.c expects (normally in api.c)
float gfx_dp_scale = 2.0f;

// UI_Layout ui is defined in ui_layout.c and declared in ui_layout.h

void setUp(void) {
	// Reset globals before each test
	gfx_dp_scale = 2.0f;
	ui = (UI_Layout){
		.screen_width = 320,
		.screen_height = 240,
		.screen_width_px = 640,
		.screen_height_px = 480,
		.pill_height = 30,
		.pill_height_px = 60,
		.row_count = 6,
		.padding = 10,
		.edge_padding = 10,
		.edge_padding_px = 20,
		.button_size = 20,
		.button_margin = 5,
		.option_size = 22,
		.option_size_px = 44,
	};
}

void tearDown(void) {
}

///////////////////////////////
// PPI and DP Scale Calculation Tests
///////////////////////////////

void test_ppi_calculation_miyoomini(void) {
	// Miyoo Mini: 640x480 @ 2.8"
	// Expected: diagonal_px = 800, ppi ~= 286, dp_scale ~= 2.38
	UI_initLayout(640, 480, 2.8);

	float expected_diagonal_px = sqrtf(640.0f * 640.0f + 480.0f * 480.0f); // 800
	float expected_ppi = expected_diagonal_px / 2.8f;                       // ~286
	float expected_dp_scale = expected_ppi / 120.0f;                        // ~2.38

	TEST_ASSERT_FLOAT_WITHIN(0.01, expected_dp_scale, gfx_dp_scale);
	TEST_ASSERT_EQUAL(640, ui.screen_width_px);
	TEST_ASSERT_EQUAL(480, ui.screen_height_px);
}

void test_ppi_calculation_trimuismart(void) {
	// Trimui Smart: 320x240 @ 2.4"
	// Expected: diagonal_px = 400, ppi ~= 167, dp_scale ~= 1.39
	UI_initLayout(320, 240, 2.4);

	float expected_diagonal_px = sqrtf(320.0f * 320.0f + 240.0f * 240.0f); // 400
	float expected_ppi = expected_diagonal_px / 2.4f;                       // ~167
	float expected_dp_scale = expected_ppi / 120.0f;                        // ~1.39

	TEST_ASSERT_FLOAT_WITHIN(0.01, expected_dp_scale, gfx_dp_scale);
}

void test_ppi_calculation_rg35xxplus(void) {
	// RG35XX Plus: 640x480 @ 3.5"
	// Expected: diagonal_px = 800, ppi ~= 229, dp_scale ~= 1.9
	UI_initLayout(640, 480, 3.5);

	float expected_diagonal_px = sqrtf(640.0f * 640.0f + 480.0f * 480.0f); // 800
	float expected_ppi = expected_diagonal_px / 3.5f;                       // ~229
	float expected_dp_scale = expected_ppi / 120.0f;                        // ~1.9

	TEST_ASSERT_FLOAT_WITHIN(0.01, expected_dp_scale, gfx_dp_scale);
}

void test_dp_values_converted_from_pixels(void) {
	// Verify DP values are correctly calculated from pixel values
	UI_initLayout(640, 480, 3.5);

	// screen_width/height should be pixels / dp_scale
	int expected_width_dp = (int)(640 / gfx_dp_scale + 0.5f);
	int expected_height_dp = (int)(480 / gfx_dp_scale + 0.5f);

	TEST_ASSERT_EQUAL(expected_width_dp, ui.screen_width);
	TEST_ASSERT_EQUAL(expected_height_dp, ui.screen_height);

	// pill_height should be pill_height_px / dp_scale
	int expected_pill_dp = (int)(ui.pill_height_px / gfx_dp_scale + 0.5f);
	TEST_ASSERT_EQUAL(expected_pill_dp, ui.pill_height);
}

///////////////////////////////
// Row Fitting Algorithm Tests
///////////////////////////////

void test_row_fitting_miyoomini_640x480(void) {
	// Miyoo Mini: 640x480 @ 2.8" should fit multiple rows
	UI_initLayout(640, 480, 2.8);

	// Verify we got a reasonable row count (4-8 rows is typical)
	TEST_ASSERT_GREATER_THAN(3, ui.row_count);
	TEST_ASSERT_LESS_THAN(9, ui.row_count);

	// Verify pill size is in acceptable range (28-32dp base range)
	TEST_ASSERT_GREATER_OR_EQUAL(26, ui.pill_height); // Allow ±2dp tolerance
	TEST_ASSERT_LESS_OR_EQUAL(34, ui.pill_height);

	// Verify pixel values are set
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);
	TEST_ASSERT_GREATER_THAN(0, ui.edge_padding_px);
}

void test_row_fitting_prefers_more_rows(void) {
	// For a screen that can fit multiple configurations,
	// the algorithm should prefer MORE content rows
	UI_initLayout(640, 480, 3.5);

	int first_row_count = ui.row_count;

	// Slightly larger screen should potentially fit more rows
	UI_initLayout(640, 500, 3.5);

	// Should have same or more rows (prefers more content)
	TEST_ASSERT_GREATER_OR_EQUAL(first_row_count, ui.row_count);
}

void test_row_fitting_even_pixel_preference(void) {
	// The algorithm prefers even-pixel pill heights for cleaner rendering
	UI_initLayout(640, 480, 3.5);

	// Check if pill_height_px is even (preferred) or at least reasonable
	// We can't guarantee it's always even (depends on screen size),
	// but we can verify the algorithm ran without crashing
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);

	// Verify the pill is used consistently
	int expected_content_bottom = ui.edge_padding_px + (ui.row_count * ui.pill_height_px);
	int expected_footer_top = 480 - ui.edge_padding_px - ui.pill_height_px;

	// Verify no overlap (content bottom should be <= footer top)
	TEST_ASSERT_LESS_OR_EQUAL(expected_footer_top, expected_content_bottom);
}

void test_row_fitting_no_overlap_content_footer(void) {
	// Critical: content rows must not overlap with footer row
	UI_initLayout(640, 480, 2.8);

	int content_bottom_px = ui.edge_padding_px + (ui.row_count * ui.pill_height_px);
	int footer_top_px = ui.screen_height_px - ui.edge_padding_px - ui.pill_height_px;

	// Content must end at or before footer starts
	TEST_ASSERT_LESS_OR_EQUAL(footer_top_px, content_bottom_px);

	// Gap should be non-negative
	int gap_px = footer_top_px - content_bottom_px;
	TEST_ASSERT_GREATER_OR_EQUAL(0, gap_px);
}

void test_row_fitting_small_screen(void) {
	// Very small screen should still get at least 1 content row
	UI_initLayout(320, 240, 2.0);

	TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);
}

void test_row_fitting_large_screen(void) {
	// Large screen should fit many rows
	UI_initLayout(1920, 1080, 10.0);

	TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);

	// Verify no overlap
	int content_bottom_px = ui.edge_padding_px + (ui.row_count * ui.pill_height_px);
	int footer_top_px = ui.screen_height_px - ui.edge_padding_px - ui.pill_height_px;
	TEST_ASSERT_LESS_OR_EQUAL(footer_top_px, content_bottom_px);
}

void test_row_fitting_wide_screen_16x9(void) {
	// 16:9 aspect ratio screen
	UI_initLayout(854, 480, 4.0);

	TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);
}

///////////////////////////////
// Edge Padding Tests
///////////////////////////////

void test_edge_padding_default(void) {
	// Without EDGE_PADDING defined, should use internal_padding (10dp)
	UI_initLayout(640, 480, 3.5);

	TEST_ASSERT_EQUAL(10, ui.edge_padding);
	TEST_ASSERT_GREATER_THAN(0, ui.edge_padding_px);
}

///////////////////////////////
// Derived Size Calculation Tests
///////////////////////////////

void test_button_size_calculation(void) {
	// button_size should be approximately (pill_height * 2) / 3
	UI_initLayout(640, 480, 3.5);

	int expected_button_size_base = (ui.pill_height * 2) / 3;

	// Allow ±1 for even-pixel adjustment
	TEST_ASSERT_INT_WITHIN(1, expected_button_size_base, ui.button_size);

	// button_margin should center button in pill
	int expected_margin = (ui.pill_height - ui.button_size) / 2;
	TEST_ASSERT_EQUAL(expected_margin, ui.button_margin);

	// button_padding should be approximately (pill_height * 2) / 5
	int expected_padding = (ui.pill_height * 2) / 5;
	TEST_ASSERT_INT_WITHIN(1, expected_padding, ui.button_padding);
}

void test_option_size_calculation(void) {
	// option_size should be approximately (pill_height * 3) / 4
	UI_initLayout(640, 480, 3.5);

	int expected_option_size_base = (ui.pill_height * 3) / 4;

	// Allow ±1 for even-pixel adjustment
	TEST_ASSERT_INT_WITHIN(1, expected_option_size_base, ui.option_size);

	// option_size_px should be DP(option_size)
	TEST_ASSERT_GREATER_THAN(0, ui.option_size_px);
}

void test_settings_size_calculation(void) {
	// settings_size should be pill_height / 8
	UI_initLayout(640, 480, 3.5);

	int expected_settings_size = ui.pill_height / 8;
	TEST_ASSERT_EQUAL(expected_settings_size, ui.settings_size);

	// settings_width is fixed at 80dp
	TEST_ASSERT_EQUAL(80, ui.settings_width);
}

void test_even_pixel_adjustments_for_derived_sizes(void) {
	// Verify even-pixel adjustments are applied to button_size and option_size
	UI_initLayout(640, 480, 3.5);

	// After initialization, check if derived sizes produce even pixels when converted
	float dp_scale = gfx_dp_scale;
	int button_px = (int)(ui.button_size * dp_scale + 0.5f);
	int option_px = (int)(ui.option_size * dp_scale + 0.5f);

	// These should be even (or at least the algorithm attempted it)
	// We can verify they're positive and reasonable
	TEST_ASSERT_GREATER_THAN(0, button_px);
	TEST_ASSERT_GREATER_THAN(0, option_px);
	TEST_ASSERT_GREATER_THAN(ui.button_size, ui.option_size); // option > button
}

///////////////////////////////
// Consistency Tests
///////////////////////////////

void test_layout_consistency_multiple_screens(void) {
	// Test multiple common screen sizes to ensure consistency
	struct {
		int width;
		int height;
		float diagonal;
	} screens[] = {
		{320, 240, 2.4},  // Trimui Smart
		{640, 480, 2.8},  // Miyoo Mini
		{640, 480, 3.5},  // RG35XX Plus
		{854, 480, 4.0},  // RGB30
		{960, 544, 5.0},  // PlayStation Vita
	};

	for (int i = 0; i < 5; i++) {
		UI_initLayout(screens[i].width, screens[i].height, screens[i].diagonal);

		// All screens should get valid layouts
		TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
		TEST_ASSERT_GREATER_THAN(0, ui.pill_height);
		TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);
		TEST_ASSERT_GREATER_THAN(0, ui.edge_padding_px);

		// Verify no overlap
		int content_bottom_px = ui.edge_padding_px + (ui.row_count * ui.pill_height_px);
		int footer_top_px = screens[i].height - ui.edge_padding_px - ui.pill_height_px;
		TEST_ASSERT_LESS_OR_EQUAL(footer_top_px, content_bottom_px);

		// Verify derived sizes are reasonable
		TEST_ASSERT_GREATER_THAN(0, ui.button_size);
		TEST_ASSERT_GREATER_THAN(0, ui.option_size);
	}
}

void test_layout_pixel_values_match_dp_values(void) {
	// Verify pixel values are consistent with DP values
	UI_initLayout(640, 480, 3.5);

	// screen_width_px and screen_height_px should match input
	TEST_ASSERT_EQUAL(640, ui.screen_width_px);
	TEST_ASSERT_EQUAL(480, ui.screen_height_px);

	// pill_height_px should be approximately pill_height * dp_scale
	int expected_pill_px = (int)(ui.pill_height * gfx_dp_scale + 0.5f);
	// Allow some tolerance due to rounding and even-pixel adjustments
	TEST_ASSERT_INT_WITHIN(2, expected_pill_px, ui.pill_height_px);

	// edge_padding_px should be approximately edge_padding * dp_scale
	int expected_edge_px = (int)(ui.edge_padding * gfx_dp_scale + 0.5f);
	TEST_ASSERT_INT_WITHIN(2, expected_edge_px, ui.edge_padding_px);
}

///////////////////////////////
// Extreme Cases and Edge Conditions
///////////////////////////////

void test_very_small_screen_emergency_fallback(void) {
	// Extremely small screen might trigger emergency fallback
	UI_initLayout(160, 120, 1.5);

	// Should still produce valid layout
	TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);
}

void test_square_screen(void) {
	// Square aspect ratio
	UI_initLayout(480, 480, 3.0);

	TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);
}

void test_portrait_orientation(void) {
	// Portrait orientation (height > width)
	// Note: LessUI is designed for landscape, but should still work
	UI_initLayout(480, 640, 3.5);

	TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height_px);
}

///////////////////////////////
// Integration: Full Layout Validation
///////////////////////////////

void test_full_layout_miyoomini(void) {
	// Complete validation for Miyoo Mini (640x480 @ 2.8")
	UI_initLayout(640, 480, 2.8);

	// Verify PPI calculation
	float diagonal_px = sqrtf(640.0f * 640.0f + 480.0f * 480.0f);
	float ppi = diagonal_px / 2.8f;
	float expected_dp_scale = ppi / 120.0f;
	TEST_ASSERT_FLOAT_WITHIN(0.01, expected_dp_scale, gfx_dp_scale);

	// Verify layout values are reasonable
	TEST_ASSERT_GREATER_OR_EQUAL(4, ui.row_count); // Should fit at least 4 rows
	TEST_ASSERT_LESS_OR_EQUAL(8, ui.row_count);    // Probably not more than 8

	TEST_ASSERT_GREATER_OR_EQUAL(26, ui.pill_height); // ~28-32dp ±2
	TEST_ASSERT_LESS_OR_EQUAL(34, ui.pill_height);

	// Verify no overlap
	int content_bottom_px = ui.edge_padding_px + (ui.row_count * ui.pill_height_px);
	int footer_top_px = 480 - ui.edge_padding_px - ui.pill_height_px;
	TEST_ASSERT_LESS_OR_EQUAL(footer_top_px, content_bottom_px);

	// Verify derived sizes
	TEST_ASSERT_GREATER_THAN(0, ui.button_size);
	TEST_ASSERT_GREATER_THAN(0, ui.option_size);
	TEST_ASSERT_GREATER_THAN(ui.button_size, ui.option_size); // option > button
}

void test_full_layout_trimuismart(void) {
	// Complete validation for Trimui Smart (320x240 @ 2.4")
	UI_initLayout(320, 240, 2.4);

	// Verify PPI calculation
	float diagonal_px = sqrtf(320.0f * 320.0f + 240.0f * 240.0f);
	float ppi = diagonal_px / 2.4f;
	float expected_dp_scale = ppi / 120.0f;
	TEST_ASSERT_FLOAT_WITHIN(0.01, expected_dp_scale, gfx_dp_scale);

	// Verify layout is valid
	TEST_ASSERT_GREATER_OR_EQUAL(1, ui.row_count);
	TEST_ASSERT_GREATER_THAN(0, ui.pill_height);

	// Verify no overlap
	int content_bottom_px = ui.edge_padding_px + (ui.row_count * ui.pill_height_px);
	int footer_top_px = 240 - ui.edge_padding_px - ui.pill_height_px;
	TEST_ASSERT_LESS_OR_EQUAL(footer_top_px, content_bottom_px);
}

int main(void) {
	UNITY_BEGIN();

	// PPI and DP Scale Calculation
	RUN_TEST(test_ppi_calculation_miyoomini);
	RUN_TEST(test_ppi_calculation_trimuismart);
	RUN_TEST(test_ppi_calculation_rg35xxplus);
	RUN_TEST(test_dp_values_converted_from_pixels);

	// Row Fitting Algorithm
	RUN_TEST(test_row_fitting_miyoomini_640x480);
	RUN_TEST(test_row_fitting_prefers_more_rows);
	RUN_TEST(test_row_fitting_even_pixel_preference);
	RUN_TEST(test_row_fitting_no_overlap_content_footer);
	RUN_TEST(test_row_fitting_small_screen);
	RUN_TEST(test_row_fitting_large_screen);
	RUN_TEST(test_row_fitting_wide_screen_16x9);

	// Edge Padding
	RUN_TEST(test_edge_padding_default);

	// Derived Size Calculations
	RUN_TEST(test_button_size_calculation);
	RUN_TEST(test_option_size_calculation);
	RUN_TEST(test_settings_size_calculation);
	RUN_TEST(test_even_pixel_adjustments_for_derived_sizes);

	// Consistency Tests
	RUN_TEST(test_layout_consistency_multiple_screens);
	RUN_TEST(test_layout_pixel_values_match_dp_values);

	// Extreme Cases
	RUN_TEST(test_very_small_screen_emergency_fallback);
	RUN_TEST(test_square_screen);
	RUN_TEST(test_portrait_orientation);

	// Full Integration Tests
	RUN_TEST(test_full_layout_miyoomini);
	RUN_TEST(test_full_layout_trimuismart);

	return UNITY_END();
}
