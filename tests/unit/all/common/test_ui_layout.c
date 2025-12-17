/**
 * test_ui_layout.c - Unit tests for Display Points (DP) UI layout calculations
 *
 * STATUS: TESTS DISABLED - Implementation extraction needed
 *
 * These tests previously contained a COPIED implementation of UI_initLayout
 * that had drifted significantly from the real code in api.c. Key differences:
 * - Real code uses ppi/120, copy used ppi/160
 * - Real code works in pixel space, copy worked in DP space
 * - Real code has SCALE_MODIFIER, EDGE_PADDING support
 * - Real code has even-pixel preference logic
 *
 * TODO: To properly test UI_initLayout:
 * 1. Extract UI_initLayout from api.c to ui_layout.c
 * 2. Create ui_layout.h with UI_Layout struct and function declaration
 * 3. Have api.c include ui_layout.c
 * 4. Update this test to include/link ui_layout.c directly
 *
 * Until then, these tests are disabled to avoid false confidence from
 * tests that don't actually verify the production code.
 */

#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_placeholder_ui_layout_needs_extraction(void) {
	// This test exists to remind us that UI_initLayout needs proper testing.
	// The previous tests used a COPIED implementation that drifted from reality.
	// See file header comment for refactoring plan.
	TEST_IGNORE_MESSAGE("UI_initLayout tests disabled - needs extraction from api.c");
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_placeholder_ui_layout_needs_extraction);

	return UNITY_END();
}
