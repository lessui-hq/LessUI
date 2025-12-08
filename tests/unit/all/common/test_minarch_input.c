/**
 * test_minarch_input.c - Unit tests for input handling utilities
 *
 * Tests input state queries, button mapping lookups, and
 * input descriptor processing.
 *
 * 20 tests organized by functionality.
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/minarch_input.h"

#include <string.h>

// Test input state
static MinArchInputState input_state;

// Test button mappings
static MinArchButtonMapping test_mappings[] = {
    {.name = "Up", .retro_id = 4, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0},
    {.name = "Down", .retro_id = 5, .local_id = 1, .modifier = 0, .default_id = 1, .ignore = 0},
    {.name = "Left", .retro_id = 6, .local_id = 2, .modifier = 0, .default_id = 2, .ignore = 0},
    {.name = "Right", .retro_id = 7, .local_id = 3, .modifier = 0, .default_id = 3, .ignore = 0},
    {.name = "A", .retro_id = 8, .local_id = 4, .modifier = 0, .default_id = 4, .ignore = 0},
    {.name = "B", .retro_id = 0, .local_id = 5, .modifier = 0, .default_id = 5, .ignore = 0},
    {.name = NULL, .retro_id = 0, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0}};

// Test input descriptors
static MinArchInputDescriptor test_descriptors[] = {
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
	TEST_ASSERT_EQUAL(0, MinArchInput_getButton(NULL, 0));
}

void test_getButton_returns_pressed_button(void) {
	input_state.buttons = (1 << 8); // Button 8 pressed
	TEST_ASSERT_EQUAL(1, MinArchInput_getButton(&input_state, 8));
}

void test_getButton_returns_zero_for_unpressed(void) {
	input_state.buttons = (1 << 8);
	TEST_ASSERT_EQUAL(0, MinArchInput_getButton(&input_state, 4));
}

void test_getButton_handles_multiple_buttons(void) {
	input_state.buttons = (1 << 4) | (1 << 5) | (1 << 8);
	TEST_ASSERT_EQUAL(1, MinArchInput_getButton(&input_state, 4));
	TEST_ASSERT_EQUAL(1, MinArchInput_getButton(&input_state, 5));
	TEST_ASSERT_EQUAL(0, MinArchInput_getButton(&input_state, 6));
	TEST_ASSERT_EQUAL(1, MinArchInput_getButton(&input_state, 8));
}

void test_getButtonMask_returns_all_buttons(void) {
	input_state.buttons = 0x12345678;
	TEST_ASSERT_EQUAL_HEX32(0x12345678, MinArchInput_getButtonMask(&input_state));
}

void test_getButtonMask_returns_zero_for_null(void) {
	TEST_ASSERT_EQUAL(0, MinArchInput_getButtonMask(NULL));
}

void test_getAnalog_returns_left_stick_x(void) {
	input_state.left.x = 12345;
	TEST_ASSERT_EQUAL(12345, MinArchInput_getAnalog(&input_state, 0, 0));
}

void test_getAnalog_returns_left_stick_y(void) {
	input_state.left.y = -5000;
	TEST_ASSERT_EQUAL(-5000, MinArchInput_getAnalog(&input_state, 0, 1));
}

void test_getAnalog_returns_right_stick(void) {
	input_state.right.x = 32767;
	input_state.right.y = -32768;
	TEST_ASSERT_EQUAL(32767, MinArchInput_getAnalog(&input_state, 1, 0));
	TEST_ASSERT_EQUAL(-32768, MinArchInput_getAnalog(&input_state, 1, 1));
}

void test_getAnalog_returns_zero_for_invalid_index(void) {
	input_state.left.x = 1000;
	TEST_ASSERT_EQUAL(0, MinArchInput_getAnalog(&input_state, 2, 0));
}

///////////////////////////////
// Button Mapping Lookup Tests
///////////////////////////////

void test_findMappingByRetroId_finds_existing(void) {
	const MinArchButtonMapping* mapping = MinArchInput_findMappingByRetroId(test_mappings, 8);
	TEST_ASSERT_NOT_NULL(mapping);
	TEST_ASSERT_EQUAL_STRING("A", mapping->name);
}

void test_findMappingByRetroId_returns_null_for_missing(void) {
	const MinArchButtonMapping* mapping = MinArchInput_findMappingByRetroId(test_mappings, 99);
	TEST_ASSERT_NULL(mapping);
}

void test_findMappingByName_finds_existing(void) {
	const MinArchButtonMapping* mapping = MinArchInput_findMappingByName(test_mappings, "Down");
	TEST_ASSERT_NOT_NULL(mapping);
	TEST_ASSERT_EQUAL(5, mapping->retro_id);
}

void test_findMappingByName_returns_null_for_missing(void) {
	const MinArchButtonMapping* mapping = MinArchInput_findMappingByName(test_mappings, "Select");
	TEST_ASSERT_NULL(mapping);
}

///////////////////////////////
// Input Descriptor Tests
///////////////////////////////

void test_isButtonAvailable_finds_present_button(void) {
	TEST_ASSERT_TRUE(MinArchInput_isButtonAvailable(test_descriptors, 4));
	TEST_ASSERT_TRUE(MinArchInput_isButtonAvailable(test_descriptors, 8));
}

void test_isButtonAvailable_returns_false_for_missing(void) {
	TEST_ASSERT_FALSE(MinArchInput_isButtonAvailable(test_descriptors, 6));
	TEST_ASSERT_FALSE(MinArchInput_isButtonAvailable(test_descriptors, 7));
}

void test_countAvailableButtons_counts_unique(void) {
	// Descriptors have: 4, 5, 8, 0 (but 0 appears twice - should count once)
	int count = MinArchInput_countAvailableButtons(test_descriptors, 15);
	TEST_ASSERT_EQUAL(4, count);
}

void test_getButtonDescription_returns_description(void) {
	const char* desc = MinArchInput_getButtonDescription(test_descriptors, 4);
	TEST_ASSERT_NOT_NULL(desc);
	TEST_ASSERT_EQUAL_STRING("D-Pad Up", desc);
}

void test_getButtonDescription_returns_null_for_missing(void) {
	const char* desc = MinArchInput_getButtonDescription(test_descriptors, 99);
	TEST_ASSERT_NULL(desc);
}

///////////////////////////////
// Button Ignore/Reset Tests
///////////////////////////////

void test_markIgnoredButtons_marks_missing_buttons(void) {
	// Descriptors have: 4, 5, 8, 0
	// Mappings have: 4, 5, 6, 7, 8, 0
	// So 6 and 7 should be marked as ignored
	int ignored = MinArchInput_markIgnoredButtons(test_mappings, test_descriptors);

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

	MinArchInput_resetToDefaults(test_mappings);

	TEST_ASSERT_EQUAL(0, test_mappings[0].local_id);
	TEST_ASSERT_EQUAL(1, test_mappings[1].local_id);
	TEST_ASSERT_EQUAL(0, test_mappings[2].ignore);
}

///////////////////////////////
// Validation Tests
///////////////////////////////

void test_validateMappings_returns_true_for_valid(void) {
	TEST_ASSERT_TRUE(MinArchInput_validateMappings(test_mappings));
}

void test_validateMappings_returns_false_for_duplicates(void) {
	MinArchButtonMapping dupe_mappings[] = {
	    {.name = "A", .retro_id = 8, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0},
	    {.name = "B", .retro_id = 8, .local_id = 1, .modifier = 0, .default_id = 1, .ignore = 0}, // Duplicate!
	    {.name = NULL, .retro_id = 0, .local_id = 0, .modifier = 0, .default_id = 0, .ignore = 0}};

	TEST_ASSERT_FALSE(MinArchInput_validateMappings(dupe_mappings));
}

void test_validateMappings_returns_false_for_null(void) {
	TEST_ASSERT_FALSE(MinArchInput_validateMappings(NULL));
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

	return UNITY_END();
}
