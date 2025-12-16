/**
 * test_player_input.c - Unit tests for input handling utilities
 *
 * Tests input state queries, button mapping lookups, and
 * input descriptor processing.
 *
 * 20 tests organized by functionality.
 */

#include "../../../support/unity/unity.h"
#include "player_input.h"

#include <string.h>

// Test input state
static PlayerInputState input_state;

// Test button mappings
static PlayerButtonMapping test_mappings[] = {
    {.name = "Up", .retro_id = 4, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0},
    {.name = "Down", .retro_id = 5, .local_id = 1, .modifier = 0, .default_id = 1, .ignore = 0},
    {.name = "Left", .retro_id = 6, .local_id = 2, .modifier = 0, .default_id = 2, .ignore = 0},
    {.name = "Right", .retro_id = 7, .local_id = 3, .modifier = 0, .default_id = 3, .ignore = 0},
    {.name = "A", .retro_id = 8, .local_id = 4, .modifier = 0, .default_id = 4, .ignore = 0},
    {.name = "B", .retro_id = 0, .local_id = 5, .modifier = 0, .default_id = 5, .ignore = 0},
    {.name = NULL, .retro_id = 0, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0}};

// Test input descriptors
static PlayerInputDescriptor test_descriptors[] = {
    {.port = 0, .device = 1, .index = 0, .id = 4, .description = "D-Pad Up"},
    {.port = 0, .device = 1, .index = 0, .id = 5, .description = "D-Pad Down"},
    {.port = 0, .device = 1, .index = 0, .id = 8, .description = "A Button"},
    {.port = 0, .device = 1, .index = 0, .id = 0, .description = "B Button"},
    {.port = 0, .device = 1, .index = 0, .id = 0, .description = NULL} // Terminator
};

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	memset(&input_state, 0, sizeof(input_state));

	// Reset test mappings
	for (int i = 0; test_mappings[i].name != NULL; i++) {
		test_mappings[i].ignore = 0;
		test_mappings[i].local_id = test_mappings[i].default_id;
	}
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Input State Query Tests
///////////////////////////////

void test_getButton_returns_zero_for_null_state(void) {
	TEST_ASSERT_EQUAL(0, PlayerInput_getButton(NULL, 0));
}

void test_getButton_returns_pressed_button(void) {
	input_state.buttons = (1 << 8); // Button 8 pressed
	TEST_ASSERT_EQUAL(1, PlayerInput_getButton(&input_state, 8));
}

void test_getButton_returns_zero_for_unpressed(void) {
	input_state.buttons = (1 << 8);
	TEST_ASSERT_EQUAL(0, PlayerInput_getButton(&input_state, 4));
}

void test_getButton_handles_multiple_buttons(void) {
	input_state.buttons = (1 << 4) | (1 << 5) | (1 << 8);
	TEST_ASSERT_EQUAL(1, PlayerInput_getButton(&input_state, 4));
	TEST_ASSERT_EQUAL(1, PlayerInput_getButton(&input_state, 5));
	TEST_ASSERT_EQUAL(0, PlayerInput_getButton(&input_state, 6));
	TEST_ASSERT_EQUAL(1, PlayerInput_getButton(&input_state, 8));
}

void test_getButtonMask_returns_all_buttons(void) {
	input_state.buttons = 0x12345678;
	TEST_ASSERT_EQUAL_HEX32(0x12345678, PlayerInput_getButtonMask(&input_state));
}

void test_getButtonMask_returns_zero_for_null(void) {
	TEST_ASSERT_EQUAL(0, PlayerInput_getButtonMask(NULL));
}

void test_getAnalog_returns_left_stick_x(void) {
	input_state.left.x = 12345;
	TEST_ASSERT_EQUAL(12345, PlayerInput_getAnalog(&input_state, 0, 0));
}

void test_getAnalog_returns_left_stick_y(void) {
	input_state.left.y = -5000;
	TEST_ASSERT_EQUAL(-5000, PlayerInput_getAnalog(&input_state, 0, 1));
}

void test_getAnalog_returns_right_stick(void) {
	input_state.right.x = 32767;
	input_state.right.y = -32768;
	TEST_ASSERT_EQUAL(32767, PlayerInput_getAnalog(&input_state, 1, 0));
	TEST_ASSERT_EQUAL(-32768, PlayerInput_getAnalog(&input_state, 1, 1));
}

void test_getAnalog_returns_zero_for_invalid_index(void) {
	input_state.left.x = 1000;
	TEST_ASSERT_EQUAL(0, PlayerInput_getAnalog(&input_state, 2, 0));
}

///////////////////////////////
// Button Mapping Lookup Tests
///////////////////////////////

void test_findMappingByRetroId_finds_existing(void) {
	const PlayerButtonMapping* mapping = PlayerInput_findMappingByRetroId(test_mappings, 8);
	TEST_ASSERT_NOT_NULL(mapping);
	TEST_ASSERT_EQUAL_STRING("A", mapping->name);
}

void test_findMappingByRetroId_returns_null_for_missing(void) {
	const PlayerButtonMapping* mapping = PlayerInput_findMappingByRetroId(test_mappings, 99);
	TEST_ASSERT_NULL(mapping);
}

void test_findMappingByName_finds_existing(void) {
	const PlayerButtonMapping* mapping = PlayerInput_findMappingByName(test_mappings, "Down");
	TEST_ASSERT_NOT_NULL(mapping);
	TEST_ASSERT_EQUAL(5, mapping->retro_id);
}

void test_findMappingByName_returns_null_for_missing(void) {
	const PlayerButtonMapping* mapping = PlayerInput_findMappingByName(test_mappings, "Select");
	TEST_ASSERT_NULL(mapping);
}

///////////////////////////////
// Input Descriptor Tests
///////////////////////////////

void test_isButtonAvailable_finds_present_button(void) {
	TEST_ASSERT_TRUE(PlayerInput_isButtonAvailable(test_descriptors, 4));
	TEST_ASSERT_TRUE(PlayerInput_isButtonAvailable(test_descriptors, 8));
}

void test_isButtonAvailable_returns_false_for_missing(void) {
	TEST_ASSERT_FALSE(PlayerInput_isButtonAvailable(test_descriptors, 6));
	TEST_ASSERT_FALSE(PlayerInput_isButtonAvailable(test_descriptors, 7));
}

void test_countAvailableButtons_counts_unique(void) {
	// Descriptors have: 4, 5, 8, 0 (but 0 appears twice - should count once)
	int count = PlayerInput_countAvailableButtons(test_descriptors, 15);
	TEST_ASSERT_EQUAL(4, count);
}

void test_getButtonDescription_returns_description(void) {
	const char* desc = PlayerInput_getButtonDescription(test_descriptors, 4);
	TEST_ASSERT_NOT_NULL(desc);
	TEST_ASSERT_EQUAL_STRING("D-Pad Up", desc);
}

void test_getButtonDescription_returns_null_for_missing(void) {
	const char* desc = PlayerInput_getButtonDescription(test_descriptors, 99);
	TEST_ASSERT_NULL(desc);
}

///////////////////////////////
// Button Ignore/Reset Tests
///////////////////////////////

void test_markIgnoredButtons_marks_missing_buttons(void) {
	// Descriptors have: 4, 5, 8, 0
	// Mappings have: 4, 5, 6, 7, 8, 0
	// So 6 and 7 should be marked as ignored
	int ignored = PlayerInput_markIgnoredButtons(test_mappings, test_descriptors);

	TEST_ASSERT_EQUAL(2, ignored);
	TEST_ASSERT_EQUAL(0, test_mappings[0].ignore); // Up (4) - present
	TEST_ASSERT_EQUAL(0, test_mappings[1].ignore); // Down (5) - present
	TEST_ASSERT_EQUAL(1, test_mappings[2].ignore); // Left (6) - missing
	TEST_ASSERT_EQUAL(1, test_mappings[3].ignore); // Right (7) - missing
	TEST_ASSERT_EQUAL(0, test_mappings[4].ignore); // A (8) - present
	TEST_ASSERT_EQUAL(0, test_mappings[5].ignore); // B (0) - present
}

void test_resetToDefaults_restores_default_ids(void) {
	// Modify local_ids
	test_mappings[0].local_id = 99;
	test_mappings[1].local_id = 88;
	test_mappings[2].ignore = 1;

	PlayerInput_resetToDefaults(test_mappings);

	TEST_ASSERT_EQUAL(0, test_mappings[0].local_id);
	TEST_ASSERT_EQUAL(1, test_mappings[1].local_id);
	TEST_ASSERT_EQUAL(0, test_mappings[2].ignore);
}

///////////////////////////////
// Validation Tests
///////////////////////////////

void test_validateMappings_returns_true_for_valid(void) {
	TEST_ASSERT_TRUE(PlayerInput_validateMappings(test_mappings));
}

void test_validateMappings_returns_false_for_duplicates(void) {
	PlayerButtonMapping dupe_mappings[] = {
	    {.name = "A", .retro_id = 8, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0},
	    {.name = "B", .retro_id = 8, .local_id = 1, .modifier = 0, .default_id = 1, .ignore = 0}, // Duplicate!
	    {.name = NULL, .retro_id = 0, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0}};

	TEST_ASSERT_FALSE(PlayerInput_validateMappings(dupe_mappings));
}

void test_validateMappings_returns_false_for_null(void) {
	TEST_ASSERT_FALSE(PlayerInput_validateMappings(NULL));
}

///////////////////////////////
// Button Collection Tests
///////////////////////////////

// Define button bits for testing (matching typical platform layout)
#define TEST_BTN_UP (1 << 4)
#define TEST_BTN_DOWN (1 << 5)
#define TEST_BTN_LEFT (1 << 6)
#define TEST_BTN_RIGHT (1 << 7)
#define TEST_BTN_A (1 << 8)
#define TEST_BTN_B (1 << 9)
#define TEST_BTN_DPAD_UP (1 << 16)
#define TEST_BTN_DPAD_DOWN (1 << 17)

// Simple control mappings for collectButtons tests
static PlayerButtonMapping collect_controls[] = {
    {.name = "A", .retro_id = 8, .local_id = 8, .modifier = 0, .default_id = 8, .ignore = 0},
    {.name = "B", .retro_id = 0, .local_id = 9, .modifier = 0, .default_id = 9, .ignore = 0},
    {.name = "Up", .retro_id = 4, .local_id = 4, .modifier = 0, .default_id = 4, .ignore = 0},
    {.name = NULL}};

static PlayerButtonMapping modifier_controls[] = {
    {.name = "A", .retro_id = 8, .local_id = 8, .modifier = 0, .default_id = 8, .ignore = 0},
    {.name = "Turbo", .retro_id = 9, .local_id = 10, .modifier = 1, .default_id = 10, .ignore = 0},
    {.name = NULL}};

static PlayerDpadRemap test_dpad_remaps[] = {{.from_btn = TEST_BTN_DPAD_UP, .to_btn = TEST_BTN_UP},
                                              {.from_btn = TEST_BTN_DPAD_DOWN, .to_btn = TEST_BTN_DOWN},
                                              {.from_btn = 0, .to_btn = 0}};

void test_collectButtons_returns_zero_for_null_controls(void) {
	uint32_t result = PlayerInput_collectButtons(NULL, TEST_BTN_A, 0, 0, NULL, NULL);
	TEST_ASSERT_EQUAL_UINT32(0, result);
}

void test_collectButtons_single_button_pressed(void) {
	uint32_t result = PlayerInput_collectButtons(collect_controls, TEST_BTN_A, 0, 1, NULL, NULL);
	TEST_ASSERT_EQUAL_UINT32(1 << 8, result); // retro_id 8 for A button
}

void test_collectButtons_multiple_buttons_pressed(void) {
	uint32_t pressed = TEST_BTN_A | TEST_BTN_B;
	uint32_t result = PlayerInput_collectButtons(collect_controls, pressed, 0, 1, NULL, NULL);
	uint32_t expected = (1 << 8) | (1 << 0); // A=8, B=0
	TEST_ASSERT_EQUAL_UINT32(expected, result);
}

void test_collectButtons_no_buttons_pressed(void) {
	uint32_t result = PlayerInput_collectButtons(collect_controls, 0, 0, 1, NULL, NULL);
	TEST_ASSERT_EQUAL_UINT32(0, result);
}

void test_collectButtons_unbound_button_ignored(void) {
	// Control with local_id=0 is considered unbound (btn = 1 << 0 = 1 = BTN_NONE)
	PlayerButtonMapping unbound[] = {
	    {.name = "Unbound", .retro_id = 5, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0},
	    {.name = NULL}};
	uint32_t result = PlayerInput_collectButtons(unbound, 0xFFFFFFFF, 0, 1, NULL, NULL);
	TEST_ASSERT_EQUAL_UINT32(0, result);
}

void test_collectButtons_modifier_not_activated_without_menu(void) {
	uint32_t pressed = 1 << 10; // Turbo button local_id
	uint32_t result = PlayerInput_collectButtons(modifier_controls, pressed, 0, 1, NULL, NULL);
	TEST_ASSERT_EQUAL_UINT32(0, result); // Should not trigger without MENU
}

void test_collectButtons_modifier_activated_with_menu(void) {
	uint32_t pressed = 1 << 10; // Turbo button local_id
	int used_modifier = 0;
	uint32_t result =
	    PlayerInput_collectButtons(modifier_controls, pressed, 1, 1, NULL, &used_modifier);
	TEST_ASSERT_EQUAL_UINT32(1 << 9, result); // retro_id 9 for Turbo
	TEST_ASSERT_EQUAL_INT(1, used_modifier);
}

void test_collectButtons_reports_used_modifier(void) {
	uint32_t pressed = (1 << 8) | (1 << 10); // A (no mod) + Turbo (mod)
	int used_modifier = 0;
	uint32_t result =
	    PlayerInput_collectButtons(modifier_controls, pressed, 1, 1, NULL, &used_modifier);
	TEST_ASSERT_EQUAL_INT(1, used_modifier);
	TEST_ASSERT_EQUAL_UINT32((1 << 8) | (1 << 9), result);
}

void test_collectButtons_no_modifier_when_menu_not_pressed(void) {
	uint32_t pressed = (1 << 8) | (1 << 10); // A (no mod) + Turbo (mod)
	int used_modifier = 0;
	uint32_t result =
	    PlayerInput_collectButtons(modifier_controls, pressed, 0, 1, NULL, &used_modifier);
	TEST_ASSERT_EQUAL_INT(0, used_modifier);
	TEST_ASSERT_EQUAL_UINT32(1 << 8, result); // Only A, not Turbo
}

void test_collectButtons_dpad_remapping_standard_gamepad(void) {
	// D-pad buttons in standard gamepad mode should remap to arrow keys.
	// In gamepad_type=0, the control is mapped to DPAD_UP (local_id=16),
	// but hardware sends arrow keys, so we remap to check BTN_UP instead.
	PlayerButtonMapping dpad_controls[] = {
	    {.name = "Up", .retro_id = 4, .local_id = 16, .modifier = 0, .default_id = 16, .ignore = 0},
	    {.name = NULL}};
	// The control mapping says local_id=16 (DPAD_UP), but hardware sends BTN_UP
	// when gamepad_type=0. So we pass BTN_UP as pressed (what hardware provides).
	uint32_t result = PlayerInput_collectButtons(dpad_controls, TEST_BTN_UP, 0, 0, test_dpad_remaps,
	                                              NULL);
	TEST_ASSERT_EQUAL_UINT32(1 << 4, result);
}

void test_collectButtons_dpad_no_remapping_analog_gamepad(void) {
	// In analog gamepad mode (type=1), d-pad should NOT be remapped
	PlayerButtonMapping dpad_controls[] = {
	    {.name = "Up", .retro_id = 4, .local_id = 16, .modifier = 0, .default_id = 16, .ignore = 0},
	    {.name = NULL}};
	uint32_t result = PlayerInput_collectButtons(dpad_controls, TEST_BTN_DPAD_UP, 0, 1,
	                                              test_dpad_remaps, NULL);
	TEST_ASSERT_EQUAL_UINT32(1 << 4, result); // Direct mapping, no remap
}

void test_collectButtons_null_modifier_output_ok(void) {
	uint32_t result = PlayerInput_collectButtons(collect_controls, TEST_BTN_A, 0, 1, NULL, NULL);
	TEST_ASSERT_EQUAL_UINT32(1 << 8, result);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Input state queries
	RUN_TEST(test_getButton_returns_zero_for_null_state);
	RUN_TEST(test_getButton_returns_pressed_button);
	RUN_TEST(test_getButton_returns_zero_for_unpressed);
	RUN_TEST(test_getButton_handles_multiple_buttons);
	RUN_TEST(test_getButtonMask_returns_all_buttons);
	RUN_TEST(test_getButtonMask_returns_zero_for_null);
	RUN_TEST(test_getAnalog_returns_left_stick_x);
	RUN_TEST(test_getAnalog_returns_left_stick_y);
	RUN_TEST(test_getAnalog_returns_right_stick);
	RUN_TEST(test_getAnalog_returns_zero_for_invalid_index);

	// Button mapping lookups
	RUN_TEST(test_findMappingByRetroId_finds_existing);
	RUN_TEST(test_findMappingByRetroId_returns_null_for_missing);
	RUN_TEST(test_findMappingByName_finds_existing);
	RUN_TEST(test_findMappingByName_returns_null_for_missing);

	// Input descriptors
	RUN_TEST(test_isButtonAvailable_finds_present_button);
	RUN_TEST(test_isButtonAvailable_returns_false_for_missing);
	RUN_TEST(test_countAvailableButtons_counts_unique);
	RUN_TEST(test_getButtonDescription_returns_description);
	RUN_TEST(test_getButtonDescription_returns_null_for_missing);

	// Ignore/Reset
	RUN_TEST(test_markIgnoredButtons_marks_missing_buttons);
	RUN_TEST(test_resetToDefaults_restores_default_ids);

	// Validation
	RUN_TEST(test_validateMappings_returns_true_for_valid);
	RUN_TEST(test_validateMappings_returns_false_for_duplicates);
	RUN_TEST(test_validateMappings_returns_false_for_null);

	// Button collection
	RUN_TEST(test_collectButtons_returns_zero_for_null_controls);
	RUN_TEST(test_collectButtons_single_button_pressed);
	RUN_TEST(test_collectButtons_multiple_buttons_pressed);
	RUN_TEST(test_collectButtons_no_buttons_pressed);
	RUN_TEST(test_collectButtons_unbound_button_ignored);
	RUN_TEST(test_collectButtons_modifier_not_activated_without_menu);
	RUN_TEST(test_collectButtons_modifier_activated_with_menu);
	RUN_TEST(test_collectButtons_reports_used_modifier);
	RUN_TEST(test_collectButtons_no_modifier_when_menu_not_pressed);
	RUN_TEST(test_collectButtons_dpad_remapping_standard_gamepad);
	RUN_TEST(test_collectButtons_dpad_no_remapping_analog_gamepad);
	RUN_TEST(test_collectButtons_null_modifier_output_ok);

	return UNITY_END();
}
