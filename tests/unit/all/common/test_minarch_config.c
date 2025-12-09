/**
 * test_minarch_config.c - Unit tests for MinArch configuration utilities
 *
 * Tests configuration file path generation, option name mapping, and
 * config value parsing. These are pure functions with no external dependencies.
 *
 * Test coverage:
 * - MinArchConfig_getPath - Config file path generation
 * - MinArchConfig_getOptionDisplayName - Option key to display name mapping
 * - MinArchConfig_getValue - Config string parsing with key=value pairs
 */

#include "../../support/unity/unity.h"
#include "minarch_config.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// MinArchConfig_getPath tests
///////////////////////////////

void test_getConfigPath_default_no_device(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/GB", NULL, NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch.cfg", output);
}

void test_getConfigPath_default_with_device(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/GB", NULL, "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch-rg35xx.cfg", output);
}

void test_getConfigPath_game_no_device(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/GB", "Tetris", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris.cfg", output);
}

void test_getConfigPath_game_with_device(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/GB", "Tetris", "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris-rg35xx.cfg", output);
}

void test_getConfigPath_game_with_spaces(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/NES", "Super Mario Bros", "miyoomini");
	TEST_ASSERT_EQUAL_STRING("/userdata/NES/Super Mario Bros-miyoomini.cfg", output);
}

void test_getConfigPath_empty_device_tag(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/GB", "Tetris", "");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris.cfg", output);
}

void test_getConfigPath_empty_game_name(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/GB", "", "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch-rg35xx.cfg", output);
}

void test_getConfigPath_long_directory(void) {
	char output[512];
	MinArchConfig_getPath(output, "/mnt/SDCARD/.userdata/miyoomini/fceumm", "Final Fantasy", "plus");
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/.userdata/miyoomini/fceumm/Final Fantasy-plus.cfg", output);
}

void test_getConfigPath_special_chars_in_game(void) {
	char output[512];
	MinArchConfig_getPath(output, "/userdata/PS1", "Final Fantasy VII (Disc 1)", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/PS1/Final Fantasy VII (Disc 1).cfg", output);
}

void test_getConfigPath_different_platforms(void) {
	char output[512];

	MinArchConfig_getPath(output, "/userdata/GBA", "Pokemon", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GBA/Pokemon.cfg", output);

	MinArchConfig_getPath(output, "/userdata/SNES", "Zelda", "trimuismart");
	TEST_ASSERT_EQUAL_STRING("/userdata/SNES/Zelda-trimuismart.cfg", output);
}

///////////////////////////////
// MinArchConfig_getOptionDisplayName tests
///////////////////////////////

void test_getOptionDisplayName_known_mapping(void) {
	const char* result = MinArchConfig_getOptionDisplayName("pcsx_rearmed_analog_combo", "Default");
	TEST_ASSERT_EQUAL_STRING("DualShock Toggle Combo", result);
}

void test_getOptionDisplayName_unknown_key_returns_default(void) {
	const char* result = MinArchConfig_getOptionDisplayName("unknown_option", "My Default Name");
	TEST_ASSERT_EQUAL_STRING("My Default Name", result);
}

void test_getOptionDisplayName_null_key_returns_default(void) {
	const char* result = MinArchConfig_getOptionDisplayName(NULL, "Fallback");
	TEST_ASSERT_EQUAL_STRING("Fallback", result);
}

void test_getOptionDisplayName_empty_key_returns_default(void) {
	const char* result = MinArchConfig_getOptionDisplayName("", "Empty Key");
	TEST_ASSERT_EQUAL_STRING("Empty Key", result);
}

void test_getOptionDisplayName_similar_but_not_exact(void) {
	// Should not match if not exact
	const char* result = MinArchConfig_getOptionDisplayName("pcsx_rearmed_analog", "Partial");
	TEST_ASSERT_EQUAL_STRING("Partial", result);
}

void test_getOptionDisplayName_case_sensitive(void) {
	// Mapping is case-sensitive
	const char* result = MinArchConfig_getOptionDisplayName("PCSX_REARMED_ANALOG_COMBO", "Uppercase");
	TEST_ASSERT_EQUAL_STRING("Uppercase", result);
}

void test_getOptionDisplayName_preserves_default_pointer(void) {
	const char* default_str = "Test Default";
	const char* result = MinArchConfig_getOptionDisplayName("nonexistent", default_str);
	// Should return the exact same pointer
	TEST_ASSERT_EQUAL_PTR(default_str, result);
}

///////////////////////////////
// Integration tests
///////////////////////////////

void test_config_path_workflow(void) {
	char path[512];

	// Start with default config
	MinArchConfig_getPath(path, "/userdata/GB", NULL, NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch.cfg", path);

	// Override for specific game
	MinArchConfig_getPath(path, "/userdata/GB", "Tetris", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris.cfg", path);

	// Add device-specific override
	MinArchConfig_getPath(path, "/userdata/GB", "Tetris", "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris-rg35xx.cfg", path);
}

void test_option_name_mapping_workflow(void) {
	// Simulate option name resolution
	const char* key1 = "pcsx_rearmed_analog_combo";
	const char* key2 = "some_other_option";

	const char* name1 = MinArchConfig_getOptionDisplayName(key1, key1);
	const char* name2 = MinArchConfig_getOptionDisplayName(key2, key2);

	// First should be mapped
	TEST_ASSERT_EQUAL_STRING("DualShock Toggle Combo", name1);

	// Second should fall back to the key itself
	TEST_ASSERT_EQUAL_STRING("some_other_option", name2);
}

///////////////////////////////
// MinArchConfig_getValue tests
///////////////////////////////

void test_getConfigValue_simple_key_value(void) {
	char value[256];
	int result = MinArchConfig_getValue("scaling = native\n", "scaling", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("native", value);
}

void test_getConfigValue_multiple_keys(void) {
	const char* cfg = "scaling = native\nvsync = on\nfilter = sharp\n";
	char value[256];

	TEST_ASSERT_EQUAL(1, MinArchConfig_getValue(cfg, "scaling", value, NULL));
	TEST_ASSERT_EQUAL_STRING("native", value);

	TEST_ASSERT_EQUAL(1, MinArchConfig_getValue(cfg, "vsync", value, NULL));
	TEST_ASSERT_EQUAL_STRING("on", value);

	TEST_ASSERT_EQUAL(1, MinArchConfig_getValue(cfg, "filter", value, NULL));
	TEST_ASSERT_EQUAL_STRING("sharp", value);
}

void test_getConfigValue_key_not_found(void) {
	char value[256];
	strcpy(value, "unchanged");
	int result = MinArchConfig_getValue("scaling = native\n", "missing", value, NULL);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_STRING("unchanged", value);
}

void test_getConfigValue_locked_value(void) {
	char value[256];
	int lock = 0;
	int result = MinArchConfig_getValue("-vsync = on\n", "vsync", value, &lock);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("on", value);
	TEST_ASSERT_EQUAL(1, lock);
}

void test_getConfigValue_unlocked_value(void) {
	char value[256];
	int lock = 0;
	int result = MinArchConfig_getValue("vsync = on\n", "vsync", value, &lock);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("on", value);
	TEST_ASSERT_EQUAL(0, lock);
}

void test_getConfigValue_lock_null_ignored(void) {
	char value[256];
	// Should not crash when lock is NULL even with locked value
	int result = MinArchConfig_getValue("-vsync = on\n", "vsync", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("on", value);
}

void test_getConfigValue_value_with_spaces(void) {
	char value[256];
	int result = MinArchConfig_getValue("message = Hello World\n", "message", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("Hello World", value);
}

void test_getConfigValue_carriage_return(void) {
	char value[256];
	int result = MinArchConfig_getValue("key = value\r\n", "key", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("value", value);
}

void test_getConfigValue_no_newline_at_end(void) {
	char value[256];
	int result = MinArchConfig_getValue("key = value", "key", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("value", value);
}

void test_getConfigValue_partial_key_match_rejected(void) {
	char value[256];
	// "scale" should not match "scaling = native"
	int result = MinArchConfig_getValue("scaling = native\n", "scale", value, NULL);
	TEST_ASSERT_EQUAL(0, result);
}

void test_getConfigValue_key_substring_in_value(void) {
	// Key appears in value but should find correct match
	char value[256];
	int result = MinArchConfig_getValue("key = key_value\n", "key", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("key_value", value);
}

void test_getConfigValue_null_cfg_returns_zero(void) {
	char value[256];
	int result = MinArchConfig_getValue(NULL, "key", value, NULL);
	TEST_ASSERT_EQUAL(0, result);
}

void test_getConfigValue_null_key_returns_zero(void) {
	char value[256];
	int result = MinArchConfig_getValue("key = value\n", NULL, value, NULL);
	TEST_ASSERT_EQUAL(0, result);
}

void test_getConfigValue_empty_value(void) {
	char value[256];
	strcpy(value, "old");
	int result = MinArchConfig_getValue("key = \n", "key", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("", value);
}

void test_getConfigValue_numeric_value(void) {
	char value[256];
	int result = MinArchConfig_getValue("volume = 75\n", "volume", value, NULL);
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("75", value);
	TEST_ASSERT_EQUAL(75, atoi(value));
}

///////////////////////////////
// MinArchConfig_getStateDesc tests
///////////////////////////////

void test_getConfigStateDesc_none(void) {
	const char* result = MinArchConfig_getStateDesc(MINARCH_CONFIG_NONE);
	TEST_ASSERT_EQUAL_STRING("Using defaults.", result);
}

void test_getConfigStateDesc_console(void) {
	const char* result = MinArchConfig_getStateDesc(MINARCH_CONFIG_CONSOLE);
	TEST_ASSERT_EQUAL_STRING("Using console config.", result);
}

void test_getConfigStateDesc_game(void) {
	const char* result = MinArchConfig_getStateDesc(MINARCH_CONFIG_GAME);
	TEST_ASSERT_EQUAL_STRING("Using game config.", result);
}

void test_getConfigStateDesc_invalid_returns_null(void) {
	const char* result = MinArchConfig_getStateDesc(99);
	TEST_ASSERT_NULL(result);
}

///////////////////////////////
// Main
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// MinArchConfig_getPath
	RUN_TEST(test_getConfigPath_default_no_device);
	RUN_TEST(test_getConfigPath_default_with_device);
	RUN_TEST(test_getConfigPath_game_no_device);
	RUN_TEST(test_getConfigPath_game_with_device);
	RUN_TEST(test_getConfigPath_game_with_spaces);
	RUN_TEST(test_getConfigPath_empty_device_tag);
	RUN_TEST(test_getConfigPath_empty_game_name);
	RUN_TEST(test_getConfigPath_long_directory);
	RUN_TEST(test_getConfigPath_special_chars_in_game);
	RUN_TEST(test_getConfigPath_different_platforms);

	// MinArchConfig_getOptionDisplayName
	RUN_TEST(test_getOptionDisplayName_known_mapping);
	RUN_TEST(test_getOptionDisplayName_unknown_key_returns_default);
	RUN_TEST(test_getOptionDisplayName_null_key_returns_default);
	RUN_TEST(test_getOptionDisplayName_empty_key_returns_default);
	RUN_TEST(test_getOptionDisplayName_similar_but_not_exact);
	RUN_TEST(test_getOptionDisplayName_case_sensitive);
	RUN_TEST(test_getOptionDisplayName_preserves_default_pointer);

	// Integration
	RUN_TEST(test_config_path_workflow);
	RUN_TEST(test_option_name_mapping_workflow);

	// MinArchConfig_getValue
	RUN_TEST(test_getConfigValue_simple_key_value);
	RUN_TEST(test_getConfigValue_multiple_keys);
	RUN_TEST(test_getConfigValue_key_not_found);
	RUN_TEST(test_getConfigValue_locked_value);
	RUN_TEST(test_getConfigValue_unlocked_value);
	RUN_TEST(test_getConfigValue_lock_null_ignored);
	RUN_TEST(test_getConfigValue_value_with_spaces);
	RUN_TEST(test_getConfigValue_carriage_return);
	RUN_TEST(test_getConfigValue_no_newline_at_end);
	RUN_TEST(test_getConfigValue_partial_key_match_rejected);
	RUN_TEST(test_getConfigValue_key_substring_in_value);
	RUN_TEST(test_getConfigValue_null_cfg_returns_zero);
	RUN_TEST(test_getConfigValue_null_key_returns_zero);
	RUN_TEST(test_getConfigValue_empty_value);
	RUN_TEST(test_getConfigValue_numeric_value);

	// MinArchConfig_getStateDesc
	RUN_TEST(test_getConfigStateDesc_none);
	RUN_TEST(test_getConfigStateDesc_console);
	RUN_TEST(test_getConfigStateDesc_game);
	RUN_TEST(test_getConfigStateDesc_invalid_returns_null);

	return UNITY_END();
}
