/**
 * test_minarch_game.c - Unit tests for game file handling utilities
 *
 * Tests the game file loading functions extracted from minarch.c:
 * - MinArchGame_parseExtensions - Parse pipe-delimited extension list
 * - MinArchGame_matchesExtension - Match filename against extension list
 * - MinArchGame_buildM3uPath - Build M3U path from ROM path
 *
 * These are pure functions that can be tested without file I/O mocking.
 */

// Note: MINARCH_GAME_TEST is defined via compiler flag (-DMINARCH_GAME_TEST)
// This skips the MinArchGame_detectM3uPath() function which depends on exists()

#include "../../support/unity/unity.h"
#include "minarch_game.h"

#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// MinArchGame_parseExtensions tests
///////////////////////////////

void test_parseExtensions_single_extension(void) {
	char exts[] = "gb";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("gb", out[0]);
	TEST_ASSERT_NULL(out[1]);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_multiple_extensions(void) {
	char exts[] = "gb|gbc|dmg";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_EQUAL_STRING("gb", out[0]);
	TEST_ASSERT_EQUAL_STRING("gbc", out[1]);
	TEST_ASSERT_EQUAL_STRING("dmg", out[2]);
	TEST_ASSERT_NULL(out[3]);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_with_zip_support(void) {
	char exts[] = "nes|fds|zip";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_TRUE(supports_zip);
}

void test_parseExtensions_zip_in_middle(void) {
	char exts[] = "nes|zip|fds";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_TRUE(supports_zip);
}

void test_parseExtensions_zip_only(void) {
	char exts[] = "zip";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_TRUE(supports_zip);
}

void test_parseExtensions_empty_string(void) {
	char exts[] = "";
	char* out[32];
	bool supports_zip = true; // Start true to verify it gets set false

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(0, count);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_null_string(void) {
	char* out[32];
	bool supports_zip = true;

	int count = MinArchGame_parseExtensions(NULL, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(0, count);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_null_output(void) {
	char exts[] = "gb|gbc";
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, NULL, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(0, count);
}

void test_parseExtensions_respects_max(void) {
	char exts[] = "a|b|c|d|e|f|g|h|i|j";
	char* out[5];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 5, &supports_zip);

	TEST_ASSERT_EQUAL_INT(5, count);
	TEST_ASSERT_EQUAL_STRING("a", out[0]);
	TEST_ASSERT_EQUAL_STRING("e", out[4]);
}

void test_parseExtensions_typical_core(void) {
	// Typical SNES core extensions
	char exts[] = "smc|sfc|swc|fig|bs|st|bin";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(7, count);
	TEST_ASSERT_EQUAL_STRING("smc", out[0]);
	TEST_ASSERT_EQUAL_STRING("bin", out[6]);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_null_supports_zip_pointer(void) {
	char exts[] = "gb|zip";
	char* out[32];

	// Should not crash when supports_zip is NULL
	int count = MinArchGame_parseExtensions(exts, out, 32, NULL);

	TEST_ASSERT_EQUAL_INT(2, count);
}

///////////////////////////////
// MinArchGame_matchesExtension tests
///////////////////////////////

void test_matchesExtension_exact_match(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.gb", extensions));
}

void test_matchesExtension_second_extension(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.gbc", extensions));
}

void test_matchesExtension_last_extension(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.dmg", extensions));
}

void test_matchesExtension_no_match(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game.nes", extensions));
}

void test_matchesExtension_case_insensitive(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.GB", extensions));
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.GBC", extensions));
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.Gb", extensions));
}

void test_matchesExtension_no_extension(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game", extensions));
}

void test_matchesExtension_dot_only(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension(".", extensions));
}

void test_matchesExtension_hidden_file_with_ext(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension(".hidden.gb", extensions));
}

void test_matchesExtension_empty_extensions(void) {
	char* extensions[] = {NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game.gb", extensions));
}

void test_matchesExtension_null_filename(void) {
	char* extensions[] = {"gb", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension(NULL, extensions));
}

void test_matchesExtension_null_extensions(void) {
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game.gb", NULL));
}

void test_matchesExtension_path_with_extension(void) {
	char* extensions[] = {"cue", "bin", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("/path/to/game/disc.cue", extensions));
}

void test_matchesExtension_double_extension(void) {
	// p8.png is a special PICO-8 format
	char* extensions[] = {"png", "p8", NULL};
	// Should match "png" (the actual extension)
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.p8.png", extensions));
}

///////////////////////////////
// MinArchGame_buildM3uPath tests
///////////////////////////////

void test_buildM3uPath_typical_disc(void) {
	char out[256];
	bool result =
	    MinArchGame_buildM3uPath("/Roms/PS/Game (Disc 1)/image.cue", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS/Game (Disc 1).m3u", out);
}

void test_buildM3uPath_simple_path(void) {
	char out[256];
	bool result =
	    MinArchGame_buildM3uPath("/path/to/folder/file.bin", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/path/to/folder.m3u", out);
}

void test_buildM3uPath_deep_path(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath(
	    "/mnt/SDCARD/Roms/PlayStation/Game Name (USA) (Disc 1)/disc.cue", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/PlayStation/Game Name (USA) (Disc 1).m3u", out);
}

void test_buildM3uPath_special_chars(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath(
	    "/Roms/PS/Game - Title (USA) [Rev 1]/track01.bin", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS/Game - Title (USA) [Rev 1].m3u", out);
}

void test_buildM3uPath_null_rom_path(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath(NULL, out, sizeof(out));
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_null_output(void) {
	bool result = MinArchGame_buildM3uPath("/path/to/file.bin", NULL, 256);
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_zero_size(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath("/path/to/file.bin", out, 0);
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_buffer_too_small(void) {
	char out[10]; // Too small for the result
	bool result = MinArchGame_buildM3uPath("/path/to/folder/file.bin", out, sizeof(out));
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_single_component(void) {
	char out[256];
	// Path with only one component - should fail (no parent directory)
	bool result = MinArchGame_buildM3uPath("/file.bin", out, sizeof(out));
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_root_dir(void) {
	char out[256];
	// ROM in root directory
	bool result = MinArchGame_buildM3uPath("/folder/file.bin", out, sizeof(out));
	// Should fail - can't go above root
	TEST_ASSERT_FALSE(result);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// parseExtensions tests
	RUN_TEST(test_parseExtensions_single_extension);
	RUN_TEST(test_parseExtensions_multiple_extensions);
	RUN_TEST(test_parseExtensions_with_zip_support);
	RUN_TEST(test_parseExtensions_zip_in_middle);
	RUN_TEST(test_parseExtensions_zip_only);
	RUN_TEST(test_parseExtensions_empty_string);
	RUN_TEST(test_parseExtensions_null_string);
	RUN_TEST(test_parseExtensions_null_output);
	RUN_TEST(test_parseExtensions_respects_max);
	RUN_TEST(test_parseExtensions_typical_core);
	RUN_TEST(test_parseExtensions_null_supports_zip_pointer);

	// matchesExtension tests
	RUN_TEST(test_matchesExtension_exact_match);
	RUN_TEST(test_matchesExtension_second_extension);
	RUN_TEST(test_matchesExtension_last_extension);
	RUN_TEST(test_matchesExtension_no_match);
	RUN_TEST(test_matchesExtension_case_insensitive);
	RUN_TEST(test_matchesExtension_no_extension);
	RUN_TEST(test_matchesExtension_dot_only);
	RUN_TEST(test_matchesExtension_hidden_file_with_ext);
	RUN_TEST(test_matchesExtension_empty_extensions);
	RUN_TEST(test_matchesExtension_null_filename);
	RUN_TEST(test_matchesExtension_null_extensions);
	RUN_TEST(test_matchesExtension_path_with_extension);
	RUN_TEST(test_matchesExtension_double_extension);

	// buildM3uPath tests
	RUN_TEST(test_buildM3uPath_typical_disc);
	RUN_TEST(test_buildM3uPath_simple_path);
	RUN_TEST(test_buildM3uPath_deep_path);
	RUN_TEST(test_buildM3uPath_special_chars);
	RUN_TEST(test_buildM3uPath_null_rom_path);
	RUN_TEST(test_buildM3uPath_null_output);
	RUN_TEST(test_buildM3uPath_zero_size);
	RUN_TEST(test_buildM3uPath_buffer_too_small);
	RUN_TEST(test_buildM3uPath_single_component);
	RUN_TEST(test_buildM3uPath_root_dir);

	return UNITY_END();
}
