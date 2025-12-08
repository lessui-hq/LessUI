/**
 * test_minarch_config.c - Unit tests for MinArch configuration utilities
 *
 * Tests configuration file path generation and option name mapping.
 * These are pure functions with no external dependencies.
 *
 * Test coverage:
 * - MinArch_getConfigPath - Config file path generation
 * - MinArch_getOptionDisplayName - Option key to display name mapping
 */

#include "../../support/unity/unity.h"
#include "../../../../workspace/all/common/minarch_config.h"
#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// MinArch_getConfigPath tests
///////////////////////////////

void test_getConfigPath_default_no_device(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/GB", NULL, NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch.cfg", output);
}

void test_getConfigPath_default_with_device(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/GB", NULL, "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch-rg35xx.cfg", output);
}

void test_getConfigPath_game_no_device(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/GB", "Tetris", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris.cfg", output);
}

void test_getConfigPath_game_with_device(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/GB", "Tetris", "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris-rg35xx.cfg", output);
}

void test_getConfigPath_game_with_spaces(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/NES", "Super Mario Bros", "miyoomini");
	TEST_ASSERT_EQUAL_STRING("/userdata/NES/Super Mario Bros-miyoomini.cfg", output);
}

void test_getConfigPath_empty_device_tag(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/GB", "Tetris", "");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris.cfg", output);
}

void test_getConfigPath_empty_game_name(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/GB", "", "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch-rg35xx.cfg", output);
}

void test_getConfigPath_long_directory(void) {
	char output[512];
	MinArch_getConfigPath(output, "/mnt/SDCARD/.userdata/miyoomini/fceumm", "Final Fantasy", "plus");
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/.userdata/miyoomini/fceumm/Final Fantasy-plus.cfg", output);
}

void test_getConfigPath_special_chars_in_game(void) {
	char output[512];
	MinArch_getConfigPath(output, "/userdata/PS1", "Final Fantasy VII (Disc 1)", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/PS1/Final Fantasy VII (Disc 1).cfg", output);
}

void test_getConfigPath_different_platforms(void) {
	char output[512];

	MinArch_getConfigPath(output, "/userdata/GBA", "Pokemon", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GBA/Pokemon.cfg", output);

	MinArch_getConfigPath(output, "/userdata/SNES", "Zelda", "trimuismart");
	TEST_ASSERT_EQUAL_STRING("/userdata/SNES/Zelda-trimuismart.cfg", output);
}

///////////////////////////////
// MinArch_getOptionDisplayName tests
///////////////////////////////

void test_getOptionDisplayName_known_mapping(void) {
	const char* result = MinArch_getOptionDisplayName("pcsx_rearmed_analog_combo", "Default");
	TEST_ASSERT_EQUAL_STRING("DualShock Toggle Combo", result);
}

void test_getOptionDisplayName_unknown_key_returns_default(void) {
	const char* result = MinArch_getOptionDisplayName("unknown_option", "My Default Name");
	TEST_ASSERT_EQUAL_STRING("My Default Name", result);
}

void test_getOptionDisplayName_null_key_returns_default(void) {
	const char* result = MinArch_getOptionDisplayName(NULL, "Fallback");
	TEST_ASSERT_EQUAL_STRING("Fallback", result);
}

void test_getOptionDisplayName_empty_key_returns_default(void) {
	const char* result = MinArch_getOptionDisplayName("", "Empty Key");
	TEST_ASSERT_EQUAL_STRING("Empty Key", result);
}

void test_getOptionDisplayName_similar_but_not_exact(void) {
	// Should not match if not exact
	const char* result = MinArch_getOptionDisplayName("pcsx_rearmed_analog", "Partial");
	TEST_ASSERT_EQUAL_STRING("Partial", result);
}

void test_getOptionDisplayName_case_sensitive(void) {
	// Mapping is case-sensitive
	const char* result = MinArch_getOptionDisplayName("PCSX_REARMED_ANALOG_COMBO", "Uppercase");
	TEST_ASSERT_EQUAL_STRING("Uppercase", result);
}

void test_getOptionDisplayName_preserves_default_pointer(void) {
	const char* default_str = "Test Default";
	const char* result = MinArch_getOptionDisplayName("nonexistent", default_str);
	// Should return the exact same pointer
	TEST_ASSERT_EQUAL_PTR(default_str, result);
}

///////////////////////////////
// Integration tests
///////////////////////////////

void test_config_path_workflow(void) {
	char path[512];

	// Start with default config
	MinArch_getConfigPath(path, "/userdata/GB", NULL, NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/minarch.cfg", path);

	// Override for specific game
	MinArch_getConfigPath(path, "/userdata/GB", "Tetris", NULL);
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris.cfg", path);

	// Add device-specific override
	MinArch_getConfigPath(path, "/userdata/GB", "Tetris", "rg35xx");
	TEST_ASSERT_EQUAL_STRING("/userdata/GB/Tetris-rg35xx.cfg", path);
}

void test_option_name_mapping_workflow(void) {
	// Simulate option name resolution
	const char* key1 = "pcsx_rearmed_analog_combo";
	const char* key2 = "some_other_option";

	const char* name1 = MinArch_getOptionDisplayName(key1, key1);
	const char* name2 = MinArch_getOptionDisplayName(key2, key2);

	// First should be mapped
	TEST_ASSERT_EQUAL_STRING("DualShock Toggle Combo", name1);

	// Second should fall back to the key itself
	TEST_ASSERT_EQUAL_STRING("some_other_option", name2);
}

///////////////////////////////
// Main
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// MinArch_getConfigPath
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

	// MinArch_getOptionDisplayName
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

	return UNITY_END();
}
