/**
 * test_minui_utils.c - Unit tests for MinUI launcher utilities
 *
 * Tests helper functions extracted from minui.c.
 * These are pure string logic with no file system dependencies.
 *
 * Test coverage:
 * - MinUI_getIndexChar() - Alphabetical index for quick navigation
 * - MinUI_isConsoleDir() - Console directory classification
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/minui_utils.h"

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// getIndexChar() Tests
///////////////////////////////

void test_getIndexChar_lowercase_letters(void) {
	TEST_ASSERT_EQUAL_INT(1, MinUI_getIndexChar("apple"));
	TEST_ASSERT_EQUAL_INT(2, MinUI_getIndexChar("banana"));
	TEST_ASSERT_EQUAL_INT(13, MinUI_getIndexChar("mario"));
	TEST_ASSERT_EQUAL_INT(26, MinUI_getIndexChar("zelda"));
}

void test_getIndexChar_uppercase_letters(void) {
	TEST_ASSERT_EQUAL_INT(1, MinUI_getIndexChar("Apple"));
	TEST_ASSERT_EQUAL_INT(2, MinUI_getIndexChar("BANANA"));
	TEST_ASSERT_EQUAL_INT(13, MinUI_getIndexChar("Mario"));
	TEST_ASSERT_EQUAL_INT(26, MinUI_getIndexChar("ZELDA"));
}

void test_getIndexChar_mixed_case(void) {
	TEST_ASSERT_EQUAL_INT(1, MinUI_getIndexChar("ApPlE"));
	TEST_ASSERT_EQUAL_INT(26, MinUI_getIndexChar("zElDa"));
}

void test_getIndexChar_numbers_return_zero(void) {
	TEST_ASSERT_EQUAL_INT(0, MinUI_getIndexChar("007 GoldenEye"));
	TEST_ASSERT_EQUAL_INT(0, MinUI_getIndexChar("1942"));
	TEST_ASSERT_EQUAL_INT(0, MinUI_getIndexChar("3D Worldrunner"));
}

void test_getIndexChar_special_chars_return_zero(void) {
	TEST_ASSERT_EQUAL_INT(0, MinUI_getIndexChar("!Exclaim"));
	TEST_ASSERT_EQUAL_INT(0, MinUI_getIndexChar("@Home"));
	TEST_ASSERT_EQUAL_INT(0, MinUI_getIndexChar(".hidden"));
}

void test_getIndexChar_empty_string_returns_zero(void) {
	TEST_ASSERT_EQUAL_INT(0, MinUI_getIndexChar(""));
}

void test_getIndexChar_single_letter(void) {
	TEST_ASSERT_EQUAL_INT(1, MinUI_getIndexChar("A"));
	TEST_ASSERT_EQUAL_INT(26, MinUI_getIndexChar("z"));
}

///////////////////////////////
// isConsoleDir() Tests
///////////////////////////////

void test_isConsoleDir_top_level_console_directory(void) {
	// /mnt/SDCARD/Roms/GB has parent /mnt/SDCARD/Roms
	int result = MinUI_isConsoleDir("/mnt/SDCARD/Roms/GB", "/mnt/SDCARD/Roms");

	TEST_ASSERT_TRUE(result);
}

void test_isConsoleDir_another_console(void) {
	int result = MinUI_isConsoleDir("/mnt/SDCARD/Roms/NES", "/mnt/SDCARD/Roms");

	TEST_ASSERT_TRUE(result);
}

void test_isConsoleDir_subdirectory_not_console(void) {
	// /mnt/SDCARD/Roms/GB/Homebrew has parent /mnt/SDCARD/Roms/GB (not Roms)
	int result = MinUI_isConsoleDir("/mnt/SDCARD/Roms/GB/Homebrew", "/mnt/SDCARD/Roms");

	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_deep_subdirectory_not_console(void) {
	int result =
	    MinUI_isConsoleDir("/mnt/SDCARD/Roms/GB/Collections/Action", "/mnt/SDCARD/Roms");

	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_non_roms_directory(void) {
	int result = MinUI_isConsoleDir("/mnt/SDCARD/Tools/say.pak", "/mnt/SDCARD/Roms");

	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_roms_path_itself_not_console(void) {
	// The Roms directory itself is not a console directory
	int result = MinUI_isConsoleDir("/mnt/SDCARD/Roms", "/mnt/SDCARD/Roms");

	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_different_roms_path(void) {
	// Test with different ROMS_PATH
	int result = MinUI_isConsoleDir("/sd/Roms/SNES", "/sd/Roms");

	TEST_ASSERT_TRUE(result);
}

void test_isConsoleDir_path_without_slash_returns_false(void) {
	// Edge case: path with no slashes
	int result = MinUI_isConsoleDir("GB", "/mnt/SDCARD/Roms");

	TEST_ASSERT_FALSE(result);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_index_char_for_rom_library(void) {
	// Simulate indexing a ROM library
	char* roms[] = {"Advance Wars", "Metroid", "Pokemon", "Zelda", "007 Racing", NULL};
	int expected[] = {1, 13, 16, 26, 0};

	for (int i = 0; roms[i] != NULL; i++) {
		TEST_ASSERT_EQUAL_INT(expected[i], MinUI_getIndexChar(roms[i]));
	}
}

void test_console_dir_classification(void) {
	const char* roms = "/mnt/SDCARD/Roms";

	// Console directories (direct children of Roms/)
	TEST_ASSERT_TRUE(MinUI_isConsoleDir("/mnt/SDCARD/Roms/GB", roms));
	TEST_ASSERT_TRUE(MinUI_isConsoleDir("/mnt/SDCARD/Roms/GBA", roms));
	TEST_ASSERT_TRUE(MinUI_isConsoleDir("/mnt/SDCARD/Roms/NES", roms));

	// Not console directories
	TEST_ASSERT_FALSE(MinUI_isConsoleDir("/mnt/SDCARD/Roms/GB/Action", roms));
	TEST_ASSERT_FALSE(MinUI_isConsoleDir("/mnt/SDCARD/Tools", roms));
	TEST_ASSERT_FALSE(MinUI_isConsoleDir("/mnt/SDCARD/Roms", roms));
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// getIndexChar tests
	RUN_TEST(test_getIndexChar_lowercase_letters);
	RUN_TEST(test_getIndexChar_uppercase_letters);
	RUN_TEST(test_getIndexChar_mixed_case);
	RUN_TEST(test_getIndexChar_numbers_return_zero);
	RUN_TEST(test_getIndexChar_special_chars_return_zero);
	RUN_TEST(test_getIndexChar_empty_string_returns_zero);
	RUN_TEST(test_getIndexChar_single_letter);

	// isConsoleDir tests
	RUN_TEST(test_isConsoleDir_top_level_console_directory);
	RUN_TEST(test_isConsoleDir_another_console);
	RUN_TEST(test_isConsoleDir_subdirectory_not_console);
	RUN_TEST(test_isConsoleDir_deep_subdirectory_not_console);
	RUN_TEST(test_isConsoleDir_non_roms_directory);
	RUN_TEST(test_isConsoleDir_roms_path_itself_not_console);
	RUN_TEST(test_isConsoleDir_different_roms_path);
	RUN_TEST(test_isConsoleDir_path_without_slash_returns_false);

	// Integration tests
	RUN_TEST(test_index_char_for_rom_library);
	RUN_TEST(test_console_dir_classification);

	return UNITY_END();
}
