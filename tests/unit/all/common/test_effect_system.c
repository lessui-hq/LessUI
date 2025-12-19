/**
 * test_effect_system.c - Unit tests for effect state management
 *
 * Tests the visual effect state management system used for scanlines,
 * pixel grids, and CRT effects. All functions are pure state manipulation
 * with no external dependencies.
 *
 * This test includes the effect_system.c source directly to avoid
 * SDL dependencies from api.h. We provide the necessary stubs here.
 *
 * Test coverage:
 * - EFFECT_init - State initialization
 * - EFFECT_setType/setScale/setColor - Pending state setters
 * - EFFECT_applyPending - Pending to current state transfer
 * - EFFECT_needsUpdate - Change detection
 * - EFFECT_markLive - Live state tracking
 * - EFFECT_getOpacity - Opacity calculation
 */

#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Stub out the heavy includes that effect_system.c would normally use
// by defining what it actually needs

// Effect type constants (normally from api.h)
#define EFFECT_NONE 0
#define EFFECT_LINE 1
#define EFFECT_GRID 2
#define EFFECT_GRILLE  3
#define EFFECT_SLOT 4

// Path constant (normally from defines.h)
#define RES_PATH "/tmp/test/.system/test/res"

// Include the effect_system header (it only needs stdint.h)
#include "../../../../workspace/all/common/effect_system.h"

// Now directly include the implementation, providing our own defines
// We need to prevent effect_system.c from including api.h and defines.h
#define __API_H__      // Prevent api.h inclusion
#define __DEFINES_H__  // Prevent defines.h inclusion

// Provide what effect_system.c actually uses from those headers:
// (nothing else beyond what we defined above)

// Include the source directly
#include "../../../../workspace/all/common/effect_system.c"

static EffectState state;

void setUp(void) {
	memset(&state, 0, sizeof(EffectState));
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// EFFECT_init tests
///////////////////////////////

void test_EFFECT_init_sets_type_to_none(void) {
	state.type = EFFECT_LINE;  // Set non-default
	EFFECT_init(&state);
	TEST_ASSERT_EQUAL_INT(EFFECT_NONE, state.type);
}

void test_EFFECT_init_sets_next_type_to_none(void) {
	state.next_type = EFFECT_GRID;
	EFFECT_init(&state);
	TEST_ASSERT_EQUAL_INT(EFFECT_NONE, state.next_type);
}

void test_EFFECT_init_sets_scale_to_1(void) {
	state.scale = 5;
	EFFECT_init(&state);
	TEST_ASSERT_EQUAL_INT(1, state.scale);
}

void test_EFFECT_init_sets_next_scale_to_1(void) {
	state.next_scale = 8;
	EFFECT_init(&state);
	TEST_ASSERT_EQUAL_INT(1, state.next_scale);
}

void test_EFFECT_init_sets_color_to_0(void) {
	state.color = 0xFFFF;
	EFFECT_init(&state);
	TEST_ASSERT_EQUAL_INT(0, state.color);
}

void test_EFFECT_init_sets_live_state_to_defaults(void) {
	state.live_type = EFFECT_LINE;
	state.live_scale = 4;
	state.live_color = 0x1234;
	EFFECT_init(&state);
	TEST_ASSERT_EQUAL_INT(EFFECT_NONE, state.live_type);
	TEST_ASSERT_EQUAL_INT(0, state.live_scale);
	TEST_ASSERT_EQUAL_INT(0, state.live_color);
}

///////////////////////////////
// EFFECT_setType tests
///////////////////////////////

void test_EFFECT_setType_sets_next_type(void) {
	EFFECT_init(&state);
	EFFECT_setType(&state, EFFECT_LINE);
	TEST_ASSERT_EQUAL_INT(EFFECT_LINE, state.next_type);
}

void test_EFFECT_setType_does_not_change_current_type(void) {
	EFFECT_init(&state);
	EFFECT_setType(&state, EFFECT_GRID);
	TEST_ASSERT_EQUAL_INT(EFFECT_NONE, state.type);
}

///////////////////////////////
// EFFECT_setScale tests
///////////////////////////////

void test_EFFECT_setScale_sets_next_scale(void) {
	EFFECT_init(&state);
	EFFECT_setScale(&state, 4);
	TEST_ASSERT_EQUAL_INT(4, state.next_scale);
}

void test_EFFECT_setScale_does_not_change_current_scale(void) {
	EFFECT_init(&state);
	EFFECT_setScale(&state, 6);
	TEST_ASSERT_EQUAL_INT(1, state.scale);
}

///////////////////////////////
// EFFECT_setColor tests
///////////////////////////////

void test_EFFECT_setColor_sets_next_color(void) {
	EFFECT_init(&state);
	EFFECT_setColor(&state, 0x07E0);  // Green in RGB565
	TEST_ASSERT_EQUAL_INT(0x07E0, state.next_color);
}

void test_EFFECT_setColor_does_not_change_current_color(void) {
	EFFECT_init(&state);
	EFFECT_setColor(&state, 0xF800);  // Red in RGB565
	TEST_ASSERT_EQUAL_INT(0, state.color);
}

///////////////////////////////
// EFFECT_applyPending tests
///////////////////////////////

void test_EFFECT_applyPending_copies_type(void) {
	EFFECT_init(&state);
	EFFECT_setType(&state, EFFECT_GRILLE);
	EFFECT_applyPending(&state);
	TEST_ASSERT_EQUAL_INT(EFFECT_GRILLE, state.type);
}

void test_EFFECT_applyPending_copies_scale(void) {
	EFFECT_init(&state);
	EFFECT_setScale(&state, 5);
	EFFECT_applyPending(&state);
	TEST_ASSERT_EQUAL_INT(5, state.scale);
}

void test_EFFECT_applyPending_copies_color(void) {
	EFFECT_init(&state);
	EFFECT_setColor(&state, 0x001F);  // Blue in RGB565
	EFFECT_applyPending(&state);
	TEST_ASSERT_EQUAL_INT(0x001F, state.color);
}

void test_EFFECT_applyPending_copies_all_fields(void) {
	EFFECT_init(&state);
	EFFECT_setType(&state, EFFECT_SLOT);
	EFFECT_setScale(&state, 3);
	EFFECT_setColor(&state, 0x7BEF);
	EFFECT_applyPending(&state);

	TEST_ASSERT_EQUAL_INT(EFFECT_SLOT, state.type);
	TEST_ASSERT_EQUAL_INT(3, state.scale);
	TEST_ASSERT_EQUAL_INT(0x7BEF, state.color);
}

///////////////////////////////
// EFFECT_needsUpdate tests
///////////////////////////////

void test_EFFECT_needsUpdate_returns_0_when_type_is_none(void) {
	EFFECT_init(&state);
	// Even with mismatched live state, EFFECT_NONE means no update needed
	state.live_type = EFFECT_LINE;
	TEST_ASSERT_EQUAL_INT(0, EFFECT_needsUpdate(&state));
}

void test_EFFECT_needsUpdate_returns_1_when_type_differs(void) {
	EFFECT_init(&state);
	state.type = EFFECT_LINE;
	state.live_type = EFFECT_NONE;
	TEST_ASSERT_EQUAL_INT(1, EFFECT_needsUpdate(&state));
}

void test_EFFECT_needsUpdate_returns_1_when_scale_differs(void) {
	EFFECT_init(&state);
	state.type = EFFECT_LINE;
	state.live_type = EFFECT_LINE;
	state.scale = 4;
	state.live_scale = 3;
	TEST_ASSERT_EQUAL_INT(1, EFFECT_needsUpdate(&state));
}

void test_EFFECT_needsUpdate_returns_1_when_color_differs(void) {
	EFFECT_init(&state);
	state.type = EFFECT_GRID;
	state.live_type = EFFECT_GRID;
	state.scale = 2;
	state.live_scale = 2;
	state.color = 0xFFFF;
	state.live_color = 0x0000;
	TEST_ASSERT_EQUAL_INT(1, EFFECT_needsUpdate(&state));
}

void test_EFFECT_needsUpdate_returns_0_when_all_match(void) {
	EFFECT_init(&state);
	state.type = EFFECT_LINE;
	state.scale = 3;
	state.color = 0x1234;
	state.live_type = EFFECT_LINE;
	state.live_scale = 3;
	state.live_color = 0x1234;
	TEST_ASSERT_EQUAL_INT(0, EFFECT_needsUpdate(&state));
}

///////////////////////////////
// EFFECT_markLive tests
///////////////////////////////

void test_EFFECT_markLive_copies_type(void) {
	EFFECT_init(&state);
	state.type = EFFECT_GRILLE;
	EFFECT_markLive(&state);
	TEST_ASSERT_EQUAL_INT(EFFECT_GRILLE, state.live_type);
}

void test_EFFECT_markLive_copies_scale(void) {
	EFFECT_init(&state);
	state.scale = 7;
	EFFECT_markLive(&state);
	TEST_ASSERT_EQUAL_INT(7, state.live_scale);
}

void test_EFFECT_markLive_copies_color(void) {
	EFFECT_init(&state);
	state.color = 0xABCD;
	EFFECT_markLive(&state);
	TEST_ASSERT_EQUAL_INT(0xABCD, state.live_color);
}

void test_EFFECT_markLive_makes_needsUpdate_return_0(void) {
	EFFECT_init(&state);
	state.type = EFFECT_LINE;
	state.scale = 4;
	state.color = 0x5678;
	TEST_ASSERT_EQUAL_INT(1, EFFECT_needsUpdate(&state));

	EFFECT_markLive(&state);
	TEST_ASSERT_EQUAL_INT(0, EFFECT_needsUpdate(&state));
}

///////////////////////////////
// EFFECT_getOpacity tests
///////////////////////////////

void test_EFFECT_getOpacity_increases_with_scale(void) {
	// Test monotonicity: opacity should increase as scale increases
	int prev = EFFECT_getOpacity(1);
	for (int scale = 2; scale <= 10; scale++) {
		int current = EFFECT_getOpacity(scale);
		TEST_ASSERT_GREATER_OR_EQUAL(prev, current);
		prev = current;
	}
}

void test_EFFECT_getOpacity_stays_within_valid_range(void) {
	// All opacity values must be in [0, 255] range for 8-bit alpha
	for (int scale = 0; scale <= 20; scale++) {
		int opacity = EFFECT_getOpacity(scale);
		TEST_ASSERT_GREATER_OR_EQUAL(0, opacity);
		TEST_ASSERT_LESS_OR_EQUAL(255, opacity);
	}
}

void test_EFFECT_getOpacity_clamps_to_255_at_high_scale(void) {
	// High scales should clamp to maximum valid alpha value
	TEST_ASSERT_EQUAL_INT(255, EFFECT_getOpacity(15));
	TEST_ASSERT_EQUAL_INT(255, EFFECT_getOpacity(20));
	TEST_ASSERT_EQUAL_INT(255, EFFECT_getOpacity(100));
}

void test_EFFECT_getOpacity_low_scale_produces_low_opacity(void) {
	// Lower scales should produce lower opacity (for subtlety)
	int opacity_1 = EFFECT_getOpacity(1);
	int opacity_2 = EFFECT_getOpacity(2);
	// Both should be well below maximum
	TEST_ASSERT_LESS_THAN(128, opacity_1);
	TEST_ASSERT_LESS_THAN(128, opacity_2);
}

///////////////////////////////
// Integration test
///////////////////////////////

void test_full_workflow(void) {
	// Simulate a full frame update workflow
	EFFECT_init(&state);

	// Configure effect for next frame
	EFFECT_setType(&state, EFFECT_LINE);
	EFFECT_setScale(&state, 4);
	EFFECT_setColor(&state, 0);

	// Apply pending changes (start of frame)
	EFFECT_applyPending(&state);

	// Check if we need to regenerate texture
	TEST_ASSERT_EQUAL_INT(1, EFFECT_needsUpdate(&state));

	// All effects now use procedural generation
	TEST_ASSERT_EQUAL_INT(1, EFFECT_usesGeneration(state.type));

	// Effects use scale-dependent opacity: 30 + (scale * 20)
	int opacity = EFFECT_getOpacity(state.scale);
	TEST_ASSERT_EQUAL_INT(110, opacity); // scale=4 -> 30 + 80 = 110

	// Mark as live after regeneration
	EFFECT_markLive(&state);

	// Verify no update needed now
	TEST_ASSERT_EQUAL_INT(0, EFFECT_needsUpdate(&state));
}

///////////////////////////////
// Main
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// EFFECT_init
	RUN_TEST(test_EFFECT_init_sets_type_to_none);
	RUN_TEST(test_EFFECT_init_sets_next_type_to_none);
	RUN_TEST(test_EFFECT_init_sets_scale_to_1);
	RUN_TEST(test_EFFECT_init_sets_next_scale_to_1);
	RUN_TEST(test_EFFECT_init_sets_color_to_0);
	RUN_TEST(test_EFFECT_init_sets_live_state_to_defaults);

	// EFFECT_setType
	RUN_TEST(test_EFFECT_setType_sets_next_type);
	RUN_TEST(test_EFFECT_setType_does_not_change_current_type);

	// EFFECT_setScale
	RUN_TEST(test_EFFECT_setScale_sets_next_scale);
	RUN_TEST(test_EFFECT_setScale_does_not_change_current_scale);

	// EFFECT_setColor
	RUN_TEST(test_EFFECT_setColor_sets_next_color);
	RUN_TEST(test_EFFECT_setColor_does_not_change_current_color);

	// EFFECT_applyPending
	RUN_TEST(test_EFFECT_applyPending_copies_type);
	RUN_TEST(test_EFFECT_applyPending_copies_scale);
	RUN_TEST(test_EFFECT_applyPending_copies_color);
	RUN_TEST(test_EFFECT_applyPending_copies_all_fields);

	// EFFECT_needsUpdate
	RUN_TEST(test_EFFECT_needsUpdate_returns_0_when_type_is_none);
	RUN_TEST(test_EFFECT_needsUpdate_returns_1_when_type_differs);
	RUN_TEST(test_EFFECT_needsUpdate_returns_1_when_scale_differs);
	RUN_TEST(test_EFFECT_needsUpdate_returns_1_when_color_differs);
	RUN_TEST(test_EFFECT_needsUpdate_returns_0_when_all_match);

	// EFFECT_markLive
	RUN_TEST(test_EFFECT_markLive_copies_type);
	RUN_TEST(test_EFFECT_markLive_copies_scale);
	RUN_TEST(test_EFFECT_markLive_copies_color);
	RUN_TEST(test_EFFECT_markLive_makes_needsUpdate_return_0);

	// EFFECT_getOpacity
	RUN_TEST(test_EFFECT_getOpacity_increases_with_scale);
	RUN_TEST(test_EFFECT_getOpacity_stays_within_valid_range);
	RUN_TEST(test_EFFECT_getOpacity_clamps_to_255_at_high_scale);
	RUN_TEST(test_EFFECT_getOpacity_low_scale_produces_low_opacity);

	// Integration
	RUN_TEST(test_full_workflow);

	return UNITY_END();
}
