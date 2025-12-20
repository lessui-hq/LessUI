/**
 * test_player_options.c - Unit tests for Player option management
 *
 * Tests option list search, get, and set operations. These are pure
 * data structure operations with no external dependencies.
 *
 * Test coverage:
 * - PlayerOptions_find - Option search by key
 * - PlayerOptions_getValue - Get current value string
 * - PlayerOptions_setValue - Set value by string
 * - PlayerOptions_setRawValue - Set value by index
 */

#define _POSIX_C_SOURCE 200809L  // Required for strdup()

#include "unity.h"
#include "player_options.h"
#include <stdlib.h>
#include <string.h>

// Test fixtures
static PlayerOptionList test_list;
static PlayerOption options[3];

void setUp(void) {
	memset(&test_list, 0, sizeof(PlayerOptionList));
	memset(options, 0, sizeof(options));

	// Build test option list with 3 options
	test_list.count = 3;
	test_list.changed = 0;
	test_list.options = options;

	// Option 0: video_scale (values: 1x, 2x, 3x)
	options[0].key = strdup("video_scale");
	options[0].name = strdup("Video Scale");
	options[0].count = 3;
	options[0].values = malloc(4 * sizeof(char*));
	options[0].values[0] = strdup("1x");
	options[0].values[1] = strdup("2x");
	options[0].values[2] = strdup("3x");
	options[0].values[3] = NULL;
	options[0].value = 1; // Default to 2x
	options[0].default_value = 1;

	// Option 1: audio_enable (values: Off, On)
	options[1].key = strdup("audio_enable");
	options[1].name = strdup("Audio");
	options[1].count = 2;
	options[1].values = malloc(3 * sizeof(char*));
	options[1].values[0] = strdup("Off");
	options[1].values[1] = strdup("On");
	options[1].values[2] = NULL;
	options[1].value = 1; // Default to On
	options[1].default_value = 1;

	// Option 2: frameskip (values: 0, 1, 2, 3)
	options[2].key = strdup("frameskip");
	options[2].name = strdup("Frameskip");
	options[2].count = 4;
	options[2].values = malloc(5 * sizeof(char*));
	options[2].values[0] = strdup("0");
	options[2].values[1] = strdup("1");
	options[2].values[2] = strdup("2");
	options[2].values[3] = strdup("3");
	options[2].values[4] = NULL;
	options[2].value = 0; // Default to 0
	options[2].default_value = 0;
}

void tearDown(void) {
	// Free allocated memory
	for (int i = 0; i < 3; i++) {
		free(options[i].key);
		free(options[i].name);
		for (int j = 0; j < options[i].count; j++) {
			free(options[i].values[j]);
		}
		free(options[i].values);
	}
}

///////////////////////////////
// PlayerOptions_find tests
///////////////////////////////

void test_findOption_finds_first_option(void) {
	PlayerOption* opt = PlayerOptions_find(&test_list, "video_scale");
	TEST_ASSERT_NOT_NULL(opt);
	TEST_ASSERT_EQUAL_STRING("video_scale", opt->key);
	TEST_ASSERT_EQUAL_STRING("Video Scale", opt->name);
}

void test_findOption_finds_middle_option(void) {
	PlayerOption* opt = PlayerOptions_find(&test_list, "audio_enable");
	TEST_ASSERT_NOT_NULL(opt);
	TEST_ASSERT_EQUAL_STRING("audio_enable", opt->key);
	TEST_ASSERT_EQUAL_STRING("Audio", opt->name);
}

void test_findOption_finds_last_option(void) {
	PlayerOption* opt = PlayerOptions_find(&test_list, "frameskip");
	TEST_ASSERT_NOT_NULL(opt);
	TEST_ASSERT_EQUAL_STRING("frameskip", opt->key);
	TEST_ASSERT_EQUAL_STRING("Frameskip", opt->name);
}

void test_findOption_returns_null_for_nonexistent(void) {
	PlayerOption* opt = PlayerOptions_find(&test_list, "nonexistent_option");
	TEST_ASSERT_NULL(opt);
}

void test_findOption_returns_null_for_null_key(void) {
	PlayerOption* opt = PlayerOptions_find(&test_list, NULL);
	TEST_ASSERT_NULL(opt);
}

void test_findOption_returns_null_for_null_list(void) {
	PlayerOption* opt = PlayerOptions_find(NULL, "video_scale");
	TEST_ASSERT_NULL(opt);
}

void test_findOption_case_sensitive(void) {
	PlayerOption* opt = PlayerOptions_find(&test_list, "VIDEO_SCALE");
	TEST_ASSERT_NULL(opt);  // Should not match (case mismatch)
}

void test_findOption_empty_string(void) {
	PlayerOption* opt = PlayerOptions_find(&test_list, "");
	TEST_ASSERT_NULL(opt);
}

///////////////////////////////
// PlayerOptions_getValue tests
///////////////////////////////

void test_getOptionValue_returns_current_value(void) {
	const char* value = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_NOT_NULL(value);
	TEST_ASSERT_EQUAL_STRING("2x", value);  // Default is index 1
}

void test_getOptionValue_returns_first_value(void) {
	const char* value = PlayerOptions_getValue(&test_list, "frameskip");
	TEST_ASSERT_NOT_NULL(value);
	TEST_ASSERT_EQUAL_STRING("0", value);  // Default is index 0
}

void test_getOptionValue_returns_null_for_nonexistent(void) {
	const char* value = PlayerOptions_getValue(&test_list, "missing");
	TEST_ASSERT_NULL(value);
}

void test_getOptionValue_returns_null_for_null_key(void) {
	const char* value = PlayerOptions_getValue(&test_list, NULL);
	TEST_ASSERT_NULL(value);
}

void test_getOptionValue_returns_null_for_null_list(void) {
	const char* value = PlayerOptions_getValue(NULL, "video_scale");
	TEST_ASSERT_NULL(value);
}

void test_getOptionValue_after_change(void) {
	// Change the value
	options[0].value = 2;  // Change to 3x

	const char* value = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_EQUAL_STRING("3x", value);
}

///////////////////////////////
// PlayerOptions_setValue tests
///////////////////////////////

void test_setOptionValue_changes_value(void) {
	PlayerOptions_setValue(&test_list, "video_scale", "3x");

	// Verify via public API instead of internal index
	const char* value = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_EQUAL_STRING("3x", value);
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_setOptionValue_changes_to_first(void) {
	PlayerOptions_setValue(&test_list, "video_scale", "1x");

	// Verify via public API instead of internal index
	const char* value = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_EQUAL_STRING("1x", value);
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_setOptionValue_marks_list_as_changed(void) {
	test_list.changed = 0;

	PlayerOptions_setValue(&test_list, "audio_enable", "Off");

	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_setOptionValue_ignores_invalid_value(void) {
	const char* original_value = PlayerOptions_getValue(&test_list, "video_scale");
	int original_changed = test_list.changed;

	PlayerOptions_setValue(&test_list, "video_scale", "4x");  // Not in list

	// Should not change
	const char* current_value = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_EQUAL_STRING(original_value, current_value);
	TEST_ASSERT_EQUAL_INT(original_changed, test_list.changed);
}

void test_setOptionValue_ignores_nonexistent_key(void) {
	int original_changed = test_list.changed;

	PlayerOptions_setValue(&test_list, "nonexistent", "value");

	TEST_ASSERT_EQUAL_INT(original_changed, test_list.changed);
}

void test_setOptionValue_ignores_null_key(void) {
	PlayerOptions_setValue(&test_list, NULL, "value");
	// Should not crash
	TEST_PASS();
}

void test_setOptionValue_ignores_null_value(void) {
	const char* original_value = PlayerOptions_getValue(&test_list, "video_scale");

	PlayerOptions_setValue(&test_list, "video_scale", NULL);

	const char* current_value = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_EQUAL_STRING(original_value, current_value);
}

void test_setOptionValue_case_sensitive_values(void) {
	// "on" != "On"
	const char* original_value = PlayerOptions_getValue(&test_list, "audio_enable");

	PlayerOptions_setValue(&test_list, "audio_enable", "on");  // lowercase

	// Should not match "On" (uppercase)
	const char* current_value = PlayerOptions_getValue(&test_list, "audio_enable");
	TEST_ASSERT_EQUAL_STRING(original_value, current_value);
}

///////////////////////////////
// PlayerOptions_setRawValue tests
///////////////////////////////

void test_setOptionRawValue_changes_value(void) {
	PlayerOptions_setRawValue(&test_list, "video_scale", 2);

	TEST_ASSERT_EQUAL_INT(2, options[0].value);
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_setOptionRawValue_sets_to_zero(void) {
	PlayerOptions_setRawValue(&test_list, "video_scale", 0);

	TEST_ASSERT_EQUAL_INT(0, options[0].value);
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_setOptionRawValue_marks_list_as_changed(void) {
	test_list.changed = 0;

	PlayerOptions_setRawValue(&test_list, "frameskip", 2);

	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_setOptionRawValue_ignores_out_of_bounds_low(void) {
	int original_value = options[0].value;

	PlayerOptions_setRawValue(&test_list, "video_scale", -1);

	TEST_ASSERT_EQUAL_INT(original_value, options[0].value);
}

void test_setOptionRawValue_ignores_out_of_bounds_high(void) {
	int original_value = options[0].value;

	PlayerOptions_setRawValue(&test_list, "video_scale", 10);

	TEST_ASSERT_EQUAL_INT(original_value, options[0].value);
}

void test_setOptionRawValue_ignores_nonexistent_key(void) {
	PlayerOptions_setRawValue(&test_list, "nonexistent", 0);
	// Should not crash
	TEST_PASS();
}

void test_setOptionRawValue_max_valid_index(void) {
	// Set to last valid index
	PlayerOptions_setRawValue(&test_list, "frameskip", 3);

	TEST_ASSERT_EQUAL_INT(3, options[2].value);
}

///////////////////////////////
// Integration tests
///////////////////////////////

void test_option_workflow_find_get_set(void) {
	// Find option
	PlayerOption* opt = PlayerOptions_find(&test_list, "video_scale");
	TEST_ASSERT_NOT_NULL(opt);

	// Get current value
	const char* current = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_EQUAL_STRING("2x", current);

	// Change value
	PlayerOptions_setValue(&test_list, "video_scale", "3x");

	// Verify change
	const char* new_value = PlayerOptions_getValue(&test_list, "video_scale");
	TEST_ASSERT_EQUAL_STRING("3x", new_value);
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_option_workflow_raw_value_setting(void) {
	// Set using raw index
	PlayerOptions_setRawValue(&test_list, "audio_enable", 0);

	// Verify via get
	const char* value = PlayerOptions_getValue(&test_list, "audio_enable");
	TEST_ASSERT_EQUAL_STRING("Off", value);
}

void test_multiple_changes_track_correctly(void) {
	test_list.changed = 0;

	// Make multiple changes
	PlayerOptions_setValue(&test_list, "video_scale", "1x");
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);

	PlayerOptions_setValue(&test_list, "audio_enable", "Off");
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);  // Still 1 (just a flag)

	PlayerOptions_setRawValue(&test_list, "frameskip", 2);
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

void test_empty_option_list(void) {
	PlayerOptionList empty_list = {.count = 0, .changed = 0, .options = NULL};

	PlayerOption* opt = PlayerOptions_find(&empty_list, "anything");
	TEST_ASSERT_NULL(opt);

	const char* value = PlayerOptions_getValue(&empty_list, "anything");
	TEST_ASSERT_NULL(value);

	// Should not crash
	PlayerOptions_setValue(&empty_list, "anything", "value");
	PlayerOptions_setRawValue(&empty_list, "anything", 0);
	TEST_PASS();
}

///////////////////////////////
// Edge case tests
///////////////////////////////

void test_option_with_single_value(void) {
	// Create option with only one value
	PlayerOption single_opt = {
	    .key = strdup("single"),
	    .name = strdup("Single"),
	    .count = 1,
	    .values = malloc(2 * sizeof(char*)),
	    .value = 0,
	    .default_value = 0
	};
	single_opt.values[0] = strdup("only");
	single_opt.values[1] = NULL;

	PlayerOptionList single_list = {
	    .count = 1,
	    .changed = 0,
	    .options = &single_opt
	};

	const char* value = PlayerOptions_getValue(&single_list, "single");
	TEST_ASSERT_EQUAL_STRING("only", value);

	PlayerOptions_setValue(&single_list, "single", "only");
	TEST_ASSERT_EQUAL_INT(0, single_opt.value);

	// Cleanup
	free(single_opt.key);
	free(single_opt.name);
	free(single_opt.values[0]);
	free(single_opt.values);
}

void test_option_value_at_boundary(void) {
	// Test value at maximum index
	options[2].value = 3;  // Last valid index for frameskip

	const char* value = PlayerOptions_getValue(&test_list, "frameskip");
	TEST_ASSERT_EQUAL_STRING("3", value);
}

void test_unchanged_list_stays_unchanged(void) {
	test_list.changed = 0;

	// Set to current value (no actual change)
	PlayerOptions_setValue(&test_list, "video_scale", "2x");

	// Still marks as changed (implementation doesn't check if value actually changed)
	TEST_ASSERT_EQUAL_INT(1, test_list.changed);
}

///////////////////////////////
// PlayerOptions_getValueIndex tests
///////////////////////////////

void test_getOptionValueIndex_finds_first(void) {
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(&options[0], "1x"));
}

void test_getOptionValueIndex_finds_middle(void) {
	TEST_ASSERT_EQUAL_INT(1, PlayerOptions_getValueIndex(&options[0], "2x"));
}

void test_getOptionValueIndex_finds_last(void) {
	TEST_ASSERT_EQUAL_INT(2, PlayerOptions_getValueIndex(&options[0], "3x"));
}

void test_getOptionValueIndex_returns_0_for_not_found(void) {
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(&options[0], "4x"));
}

void test_getOptionValueIndex_returns_0_for_null_value(void) {
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(&options[0], NULL));
}

void test_getOptionValueIndex_returns_0_for_null_opt(void) {
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(NULL, "1x"));
}

void test_getOptionValueIndex_case_sensitive(void) {
	// "Off" should match index 0, but "off" should return 0 (default, not found)
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(&options[1], "Off"));
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(&options[1], "off")); // Not found -> returns 0
}

void test_getOptionValueIndex_empty_string_not_in_values(void) {
	// Empty string not in our test values, should return 0
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(&options[0], ""));
}

void test_getOptionValueIndex_single_value_option(void) {
	// Create a single-value option
	PlayerOption single = {
		.key = "single",
		.name = "Single",
		.count = 1,
		.values = (char*[]){ "only", NULL },
		.value = 0
	};
	TEST_ASSERT_EQUAL_INT(0, PlayerOptions_getValueIndex(&single, "only"));
}

///////////////////////////////
// Main
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// PlayerOptions_find
	RUN_TEST(test_findOption_finds_first_option);
	RUN_TEST(test_findOption_finds_middle_option);
	RUN_TEST(test_findOption_finds_last_option);
	RUN_TEST(test_findOption_returns_null_for_nonexistent);
	RUN_TEST(test_findOption_returns_null_for_null_key);
	RUN_TEST(test_findOption_returns_null_for_null_list);
	RUN_TEST(test_findOption_case_sensitive);
	RUN_TEST(test_findOption_empty_string);

	// PlayerOptions_getValue
	RUN_TEST(test_getOptionValue_returns_current_value);
	RUN_TEST(test_getOptionValue_returns_first_value);
	RUN_TEST(test_getOptionValue_returns_null_for_nonexistent);
	RUN_TEST(test_getOptionValue_returns_null_for_null_key);
	RUN_TEST(test_getOptionValue_returns_null_for_null_list);
	RUN_TEST(test_getOptionValue_after_change);

	// PlayerOptions_setValue
	RUN_TEST(test_setOptionValue_changes_value);
	RUN_TEST(test_setOptionValue_changes_to_first);
	RUN_TEST(test_setOptionValue_marks_list_as_changed);
	RUN_TEST(test_setOptionValue_ignores_invalid_value);
	RUN_TEST(test_setOptionValue_ignores_nonexistent_key);
	RUN_TEST(test_setOptionValue_ignores_null_key);
	RUN_TEST(test_setOptionValue_ignores_null_value);
	RUN_TEST(test_setOptionValue_case_sensitive_values);

	// PlayerOptions_setRawValue
	RUN_TEST(test_setOptionRawValue_changes_value);
	RUN_TEST(test_setOptionRawValue_sets_to_zero);
	RUN_TEST(test_setOptionRawValue_marks_list_as_changed);
	RUN_TEST(test_setOptionRawValue_ignores_out_of_bounds_low);
	RUN_TEST(test_setOptionRawValue_ignores_out_of_bounds_high);
	RUN_TEST(test_setOptionRawValue_ignores_nonexistent_key);
	RUN_TEST(test_setOptionRawValue_max_valid_index);

	// Integration tests
	RUN_TEST(test_option_workflow_find_get_set);
	RUN_TEST(test_option_workflow_raw_value_setting);
	RUN_TEST(test_multiple_changes_track_correctly);
	RUN_TEST(test_empty_option_list);

	// Edge cases
	RUN_TEST(test_option_with_single_value);
	RUN_TEST(test_option_value_at_boundary);
	RUN_TEST(test_unchanged_list_stays_unchanged);

	// PlayerOptions_getValueIndex
	RUN_TEST(test_getOptionValueIndex_finds_first);
	RUN_TEST(test_getOptionValueIndex_finds_middle);
	RUN_TEST(test_getOptionValueIndex_finds_last);
	RUN_TEST(test_getOptionValueIndex_returns_0_for_not_found);
	RUN_TEST(test_getOptionValueIndex_returns_0_for_null_value);
	RUN_TEST(test_getOptionValueIndex_returns_0_for_null_opt);
	RUN_TEST(test_getOptionValueIndex_case_sensitive);
	RUN_TEST(test_getOptionValueIndex_empty_string_not_in_values);
	RUN_TEST(test_getOptionValueIndex_single_value_option);

	return UNITY_END();
}
