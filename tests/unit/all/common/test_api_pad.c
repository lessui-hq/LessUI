/**
 * test_api_pad.c - Unit tests for PAD (input) functions
 *
 * Tests the button state management and analog stick conversion logic.
 * These functions are pure state manipulation using bitwise operations,
 * making them ideal candidates for unit testing.
 *
 * Test coverage:
 * - PAD_reset() - Clearing all button states
 * - PAD_setAnalog() - Analog stick to digital button conversion
 * - PAD_anyJustPressed/anyPressed/anyJustReleased() - Query functions
 * - PAD_justPressed/isPressed/justReleased/justRepeated() - Button-specific queries
 * - PAD_tappedMenu() - Menu tap detection with timing
 */

#include "../../support/unity/unity.h"
#include "../../../../workspace/all/common/pad.h"
#include <string.h>

// Test helpers
static void resetPadState(void) {
	pad.just_pressed = BTN_NONE;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;
	memset(pad.repeat_at, 0, sizeof(pad.repeat_at));
}

void setUp(void) {
	resetPadState();
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// PAD_reset tests
///////////////////////////////

void test_PAD_reset_clears_all_button_states(void) {
	// Set up some button state
	pad.just_pressed = BTN_A | BTN_B;
	pad.is_pressed = BTN_START | BTN_SELECT;
	pad.just_released = BTN_X;
	pad.just_repeated = BTN_DPAD_UP;

	// Reset should clear everything
	PAD_reset();

	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.just_pressed);
	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.is_pressed);
	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.just_released);
	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.just_repeated);
}

///////////////////////////////
// PAD_setAnalog tests
///////////////////////////////

void test_PAD_setAnalog_positive_direction_from_neutral(void) {
	// Simulate right analog stick moving right
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 20000, 1000);

	// Should set positive direction (right)
	TEST_ASSERT_TRUE(pad.is_pressed & BTN_ANALOG_RIGHT);
	TEST_ASSERT_TRUE(pad.just_pressed & BTN_ANALOG_RIGHT);
	TEST_ASSERT_TRUE(pad.just_repeated & BTN_ANALOG_RIGHT);
	TEST_ASSERT_EQUAL_UINT32(1000, pad.repeat_at[BTN_ID_ANALOG_RIGHT]);

	// Should NOT set negative direction (left)
	TEST_ASSERT_FALSE(pad.is_pressed & BTN_ANALOG_LEFT);
}

void test_PAD_setAnalog_negative_direction_from_neutral(void) {
	// Simulate analog stick moving left
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, -20000, 2000);

	// Should set negative direction (left)
	TEST_ASSERT_TRUE(pad.is_pressed & BTN_ANALOG_LEFT);
	TEST_ASSERT_TRUE(pad.just_pressed & BTN_ANALOG_LEFT);
	TEST_ASSERT_TRUE(pad.just_repeated & BTN_ANALOG_LEFT);
	TEST_ASSERT_EQUAL_UINT32(2000, pad.repeat_at[BTN_ID_ANALOG_LEFT]);

	// Should NOT set positive direction (right)
	TEST_ASSERT_FALSE(pad.is_pressed & BTN_ANALOG_RIGHT);
}

void test_PAD_setAnalog_deadzone_prevents_activation(void) {
	// Values within deadzone (< 0x4000 = 16384) should not activate
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 10000, 1000);

	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.is_pressed);
	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.just_pressed);

	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, -10000, 1000);

	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.is_pressed);
	TEST_ASSERT_EQUAL_INT(BTN_NONE, pad.just_pressed);
}

void test_PAD_setAnalog_opposite_direction_cancels_previous(void) {
	// First press right
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 20000, 1000);
	TEST_ASSERT_TRUE(pad.is_pressed & BTN_ANALOG_RIGHT);

	// Clear transient state (simulating next frame)
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	// Now press left - should cancel right
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, -20000, 2000);

	TEST_ASSERT_TRUE(pad.is_pressed & BTN_ANALOG_LEFT);
	TEST_ASSERT_FALSE(pad.is_pressed & BTN_ANALOG_RIGHT);
	TEST_ASSERT_TRUE(pad.just_released & BTN_ANALOG_RIGHT);
}

void test_PAD_setAnalog_return_to_neutral_releases_both(void) {
	// Press right
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 20000, 1000);
	TEST_ASSERT_TRUE(pad.is_pressed & BTN_ANALOG_RIGHT);

	// Clear transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	// Return to neutral (within deadzone)
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 0, 2000);

	TEST_ASSERT_FALSE(pad.is_pressed & BTN_ANALOG_RIGHT);
	TEST_ASSERT_FALSE(pad.is_pressed & BTN_ANALOG_LEFT);
	TEST_ASSERT_TRUE(pad.just_released & BTN_ANALOG_RIGHT);
}

void test_PAD_setAnalog_already_pressed_doesnt_trigger_just_pressed(void) {
	// Press right
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 20000, 1000);
	TEST_ASSERT_TRUE(pad.just_pressed & BTN_ANALOG_RIGHT);

	// Clear transient state
	pad.just_pressed = BTN_NONE;

	// Still holding right - should not trigger just_pressed again
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 20000, 1100);

	TEST_ASSERT_TRUE(pad.is_pressed & BTN_ANALOG_RIGHT);
	TEST_ASSERT_FALSE(pad.just_pressed & BTN_ANALOG_RIGHT);
}

void test_PAD_setAnalog_release_clears_just_repeated(void) {
	// Press right analog
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 20000, 1000);
	TEST_ASSERT_TRUE(pad.just_repeated & BTN_ANALOG_RIGHT);

	// Clear transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	// Manually set just_repeated (simulating button held for repeat)
	pad.just_repeated = BTN_ANALOG_RIGHT | BTN_A;

	// Return analog to neutral - should clear ONLY ANALOG_RIGHT from just_repeated
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 0, 2000);

	// just_repeated should have ANALOG_RIGHT cleared, but BTN_A should remain
	TEST_ASSERT_FALSE(pad.just_repeated & BTN_ANALOG_RIGHT);
	TEST_ASSERT_TRUE(pad.just_repeated & BTN_A);
}

void test_PAD_setAnalog_release_with_multiple_buttons_repeated(void) {
	// Setup: Multiple buttons are in just_repeated state
	pad.is_pressed = BTN_ANALOG_LEFT | BTN_ANALOG_RIGHT;
	pad.just_repeated = BTN_ANALOG_LEFT | BTN_ANALOG_RIGHT | BTN_B | BTN_SELECT;

	// Release right analog (return to neutral)
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, 0, 1000);

	// Verify: ONLY the released buttons cleared from just_repeated
	TEST_ASSERT_FALSE(pad.just_repeated & BTN_ANALOG_RIGHT);
	TEST_ASSERT_FALSE(pad.just_repeated & BTN_ANALOG_LEFT);

	// Other buttons in just_repeated should remain untouched
	TEST_ASSERT_TRUE(pad.just_repeated & BTN_B);
	TEST_ASSERT_TRUE(pad.just_repeated & BTN_SELECT);
}

///////////////////////////////
// PAD query function tests
///////////////////////////////

void test_PAD_anyJustPressed_returns_true_when_button_just_pressed(void) {
	pad.just_pressed = BTN_A;
	TEST_ASSERT_TRUE(PAD_anyJustPressed());
}

void test_PAD_anyJustPressed_returns_false_when_no_buttons_just_pressed(void) {
	pad.just_pressed = BTN_NONE;
	TEST_ASSERT_FALSE(PAD_anyJustPressed());
}

void test_PAD_anyPressed_returns_true_when_button_is_held(void) {
	pad.is_pressed = BTN_START;
	TEST_ASSERT_TRUE(PAD_anyPressed());
}

void test_PAD_anyPressed_returns_false_when_no_buttons_held(void) {
	pad.is_pressed = BTN_NONE;
	TEST_ASSERT_FALSE(PAD_anyPressed());
}

void test_PAD_anyJustReleased_returns_true_when_button_just_released(void) {
	pad.just_released = BTN_B;
	TEST_ASSERT_TRUE(PAD_anyJustReleased());
}

void test_PAD_anyJustReleased_returns_false_when_no_buttons_just_released(void) {
	pad.just_released = BTN_NONE;
	TEST_ASSERT_FALSE(PAD_anyJustReleased());
}

void test_PAD_justPressed_returns_true_for_specific_button(void) {
	pad.just_pressed = BTN_A | BTN_B;
	TEST_ASSERT_TRUE(PAD_justPressed(BTN_A));
	TEST_ASSERT_TRUE(PAD_justPressed(BTN_B));
	TEST_ASSERT_FALSE(PAD_justPressed(BTN_X));
}

void test_PAD_isPressed_returns_true_for_held_button(void) {
	pad.is_pressed = BTN_START | BTN_SELECT;
	TEST_ASSERT_TRUE(PAD_isPressed(BTN_START));
	TEST_ASSERT_TRUE(PAD_isPressed(BTN_SELECT));
	TEST_ASSERT_FALSE(PAD_isPressed(BTN_MENU));
}

void test_PAD_justReleased_returns_true_for_released_button(void) {
	pad.just_released = BTN_L1 | BTN_R1;
	TEST_ASSERT_TRUE(PAD_justReleased(BTN_L1));
	TEST_ASSERT_TRUE(PAD_justReleased(BTN_R1));
	TEST_ASSERT_FALSE(PAD_justReleased(BTN_L2));
}

void test_PAD_justRepeated_returns_true_for_repeating_button(void) {
	pad.just_repeated = BTN_DPAD_UP;
	TEST_ASSERT_TRUE(PAD_justRepeated(BTN_DPAD_UP));
	TEST_ASSERT_FALSE(PAD_justRepeated(BTN_DPAD_DOWN));
}

///////////////////////////////
// PAD_tappedMenu tests
///////////////////////////////

void test_PAD_tappedMenu_detects_quick_menu_tap(void) {
	uint32_t start_time = 1000;

	// Simulate menu button press
	pad.just_pressed = BTN_MENU;
	pad.is_pressed = BTN_MENU;
	PAD_tappedMenu(start_time);

	// Clear just_pressed (simulating next frame)
	pad.just_pressed = BTN_NONE;

	// Release menu button quickly (within 250ms)
	uint32_t release_time = start_time + 200;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_MENU;

	TEST_ASSERT_TRUE(PAD_tappedMenu(release_time));
}

void test_PAD_tappedMenu_ignores_held_menu_button(void) {
	uint32_t start_time = 1000;

	// Simulate menu button press
	pad.just_pressed = BTN_MENU;
	pad.is_pressed = BTN_MENU;
	PAD_tappedMenu(start_time);

	// Clear just_pressed
	pad.just_pressed = BTN_NONE;

	// Release menu button after too long (> 250ms)
	uint32_t release_time = start_time + 300;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_MENU;

	TEST_ASSERT_FALSE(PAD_tappedMenu(release_time));
}

void test_PAD_tappedMenu_ignores_brightness_adjustment(void) {
	// This test only applies on platforms where BTN_MOD_BRIGHTNESS == BTN_MENU
	// Skip the test if BTN_MOD_BRIGHTNESS is BTN_NONE (test environment)
	if (BTN_MOD_BRIGHTNESS == BTN_NONE) {
		TEST_PASS();
		return;
	}

	uint32_t start_time = 1000;

	// Simulate menu button press
	pad.just_pressed = BTN_MENU;
	pad.is_pressed = BTN_MENU;
	PAD_tappedMenu(start_time);

	// Clear just_pressed
	pad.just_pressed = BTN_NONE;

	// Now press PLUS while holding MENU (brightness adjustment)
	// Note: This only applies if BTN_MOD_BRIGHTNESS == BTN_MENU
	if (BTN_MOD_BRIGHTNESS == BTN_MENU) {
		pad.just_pressed = BTN_MOD_PLUS;
		PAD_tappedMenu(start_time + 100);

		// Clear just_pressed
		pad.just_pressed = BTN_NONE;

		// Release menu quickly - should be ignored due to brightness adjustment
		uint32_t release_time = start_time + 200;
		pad.is_pressed = BTN_NONE;
		pad.just_released = BTN_MENU;

		TEST_ASSERT_FALSE(PAD_tappedMenu(release_time));
	}
}

void test_PAD_tappedMenu_returns_false_when_menu_still_held(void) {
	uint32_t start_time = 1000;

	// Simulate menu button press
	pad.just_pressed = BTN_MENU;
	pad.is_pressed = BTN_MENU;
	PAD_tappedMenu(start_time);

	// Clear just_pressed
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;

	// Menu is still held (not released)
	TEST_ASSERT_FALSE(PAD_tappedMenu(start_time + 100));
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// PAD_reset tests
	RUN_TEST(test_PAD_reset_clears_all_button_states);

	// PAD_setAnalog tests
	RUN_TEST(test_PAD_setAnalog_positive_direction_from_neutral);
	RUN_TEST(test_PAD_setAnalog_negative_direction_from_neutral);
	RUN_TEST(test_PAD_setAnalog_deadzone_prevents_activation);
	RUN_TEST(test_PAD_setAnalog_opposite_direction_cancels_previous);
	RUN_TEST(test_PAD_setAnalog_return_to_neutral_releases_both);
	RUN_TEST(test_PAD_setAnalog_already_pressed_doesnt_trigger_just_pressed);
	RUN_TEST(test_PAD_setAnalog_release_clears_just_repeated);
	RUN_TEST(test_PAD_setAnalog_release_with_multiple_buttons_repeated);

	// PAD query function tests
	RUN_TEST(test_PAD_anyJustPressed_returns_true_when_button_just_pressed);
	RUN_TEST(test_PAD_anyJustPressed_returns_false_when_no_buttons_just_pressed);
	RUN_TEST(test_PAD_anyPressed_returns_true_when_button_is_held);
	RUN_TEST(test_PAD_anyPressed_returns_false_when_no_buttons_held);
	RUN_TEST(test_PAD_anyJustReleased_returns_true_when_button_just_released);
	RUN_TEST(test_PAD_anyJustReleased_returns_false_when_no_buttons_just_released);
	RUN_TEST(test_PAD_justPressed_returns_true_for_specific_button);
	RUN_TEST(test_PAD_isPressed_returns_true_for_held_button);
	RUN_TEST(test_PAD_justReleased_returns_true_for_released_button);
	RUN_TEST(test_PAD_justRepeated_returns_true_for_repeating_button);

	// PAD_tappedMenu tests
	RUN_TEST(test_PAD_tappedMenu_detects_quick_menu_tap);
	RUN_TEST(test_PAD_tappedMenu_ignores_held_menu_button);
	RUN_TEST(test_PAD_tappedMenu_ignores_brightness_adjustment);
	RUN_TEST(test_PAD_tappedMenu_returns_false_when_menu_still_held);

	return UNITY_END();
}
