/**
 * test_player_archive.c - Tests for player_archive module
 *
 * Tests the archive extraction module that shells out to 7z binary.
 */

#include "unity.h"
#include "../../../../workspace/all/player/player_archive.h"
#include <string.h>

void setUp(void) {
	// This runs before each test
}

void tearDown(void) {
	// This runs after each test
}

///////////////////////////////////////
// PlayerArchive_isArchive Tests
///////////////////////////////////////

void test_isArchive_zip_lowercase(void) {
	TEST_ASSERT_TRUE(PlayerArchive_isArchive("/path/to/game.zip"));
}

void test_isArchive_zip_uppercase(void) {
	TEST_ASSERT_TRUE(PlayerArchive_isArchive("/path/to/GAME.ZIP"));
}

void test_isArchive_zip_mixedcase(void) {
	TEST_ASSERT_TRUE(PlayerArchive_isArchive("/path/to/game.Zip"));
}

void test_isArchive_7z_lowercase(void) {
	TEST_ASSERT_TRUE(PlayerArchive_isArchive("/path/to/game.7z"));
}

void test_isArchive_7z_uppercase(void) {
	TEST_ASSERT_TRUE(PlayerArchive_isArchive("/path/to/GAME.7Z"));
}

void test_isArchive_7z_mixedcase(void) {
	TEST_ASSERT_TRUE(PlayerArchive_isArchive("/path/to/game.7Z"));
}

void test_isArchive_not_archive_gb(void) {
	TEST_ASSERT_FALSE(PlayerArchive_isArchive("/path/to/game.gb"));
}

void test_isArchive_not_archive_gba(void) {
	TEST_ASSERT_FALSE(PlayerArchive_isArchive("/path/to/game.gba"));
}

void test_isArchive_not_archive_nes(void) {
	TEST_ASSERT_FALSE(PlayerArchive_isArchive("/path/to/game.nes"));
}

void test_isArchive_not_archive_no_extension(void) {
	TEST_ASSERT_FALSE(PlayerArchive_isArchive("/path/to/game"));
}

void test_isArchive_null_path(void) {
	TEST_ASSERT_FALSE(PlayerArchive_isArchive(NULL));
}

void test_isArchive_empty_path(void) {
	TEST_ASSERT_FALSE(PlayerArchive_isArchive(""));
}

void test_isArchive_partial_match_zip(void) {
	// Should not match if .zip is in middle of filename
	TEST_ASSERT_FALSE(PlayerArchive_isArchive("/path/to/game.zip.txt"));
}

void test_isArchive_partial_match_7z(void) {
	// Should not match if .7z is in middle of filename
	TEST_ASSERT_FALSE(PlayerArchive_isArchive("/path/to/game.7z.txt"));
}

// Note: We don't test PlayerArchive_extract() or PlayerArchive_findMatch()
// here because they require the 7z binary and real archive files.
// Those should be tested via integration tests with fixture archives.

int main(void) {
	UNITY_BEGIN();

	// isArchive tests
	RUN_TEST(test_isArchive_zip_lowercase);
	RUN_TEST(test_isArchive_zip_uppercase);
	RUN_TEST(test_isArchive_zip_mixedcase);
	RUN_TEST(test_isArchive_7z_lowercase);
	RUN_TEST(test_isArchive_7z_uppercase);
	RUN_TEST(test_isArchive_7z_mixedcase);
	RUN_TEST(test_isArchive_not_archive_gb);
	RUN_TEST(test_isArchive_not_archive_gba);
	RUN_TEST(test_isArchive_not_archive_nes);
	RUN_TEST(test_isArchive_not_archive_no_extension);
	RUN_TEST(test_isArchive_null_path);
	RUN_TEST(test_isArchive_empty_path);
	RUN_TEST(test_isArchive_partial_match_zip);
	RUN_TEST(test_isArchive_partial_match_7z);

	return UNITY_END();
}
