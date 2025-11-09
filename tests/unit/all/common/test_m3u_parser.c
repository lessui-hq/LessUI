/**
 * test_m3u_parser.c - Unit tests for M3U playlist parser
 *
 * Tests M3U parsing logic extracted from minui.c.
 * Uses file system mocking to test file reading without actual files.
 *
 * Test coverage:
 * - Basic M3U parsing (single disc, multiple discs)
 * - Empty line handling
 * - Non-existent files
 * - Path construction (relative paths from M3U directory)
 * - Edge cases (empty M3U, missing disc files)
 *
 * Note: Uses GCC --wrap for file mocking (Docker-only, won't run natively on macOS)
 */

#include "../../../support/unity/unity.h"
#include "../../../support/fs_mocks.h"
#include "../../../../workspace/all/common/m3u_parser.h"

#include <string.h>

void setUp(void) {
	mock_fs_reset();
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Basic M3U Parsing Tests
///////////////////////////////

void test_getFirstDisc_single_disc_m3u(void) {
	// Create M3U file with one disc
	mock_fs_add_file("/Roms/PS1/FF7.m3u", "FF7 (Disc 1).bin\n");
	mock_fs_add_file("/Roms/PS1/FF7 (Disc 1).bin", "disc data");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/FF7.m3u", disc_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF7 (Disc 1).bin", disc_path);
}

void test_getFirstDisc_multi_disc_m3u_returns_first(void) {
	// Create M3U with multiple discs
	mock_fs_add_file("/Roms/PS1/Game.m3u",
	                 "Game (Disc 1).bin\n"
	                 "Game (Disc 2).bin\n"
	                 "Game (Disc 3).bin\n");
	mock_fs_add_file("/Roms/PS1/Game (Disc 1).bin", "disc 1");
	mock_fs_add_file("/Roms/PS1/Game (Disc 2).bin", "disc 2");
	mock_fs_add_file("/Roms/PS1/Game (Disc 3).bin", "disc 3");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/Game.m3u", disc_path);

	TEST_ASSERT_TRUE(result);
	// Should return FIRST disc only
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/Game (Disc 1).bin", disc_path);
}

void test_getFirstDisc_skips_empty_lines(void) {
	// M3U with empty lines before first disc
	mock_fs_add_file("/Roms/PS1/Test.m3u",
	                 "\n"
	                 "\n"
	                 "Test Disc.bin\n");
	mock_fs_add_file("/Roms/PS1/Test Disc.bin", "data");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/Test.m3u", disc_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/Test Disc.bin", disc_path);
}

void test_getFirstDisc_handles_windows_newlines(void) {
	// M3U created on Windows with \r\n line endings
	mock_fs_add_file("/Roms/PS1/Game.m3u", "Disc.bin\r\n");
	mock_fs_add_file("/Roms/PS1/Disc.bin", "data");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/Game.m3u", disc_path);

	TEST_ASSERT_TRUE(result);
	// normalizeNewline should have handled \r\n
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/Disc.bin", disc_path);
}

///////////////////////////////
// Path Construction Tests
///////////////////////////////

void test_getFirstDisc_constructs_full_path(void) {
	// Disc path is relative to M3U location
	mock_fs_add_file("/Games/Saturn/Panzer.m3u", "panzer1.cue\n");
	mock_fs_add_file("/Games/Saturn/panzer1.cue", "cue data");

	char disc_path[256];
	M3U_getFirstDisc("/Games/Saturn/Panzer.m3u", disc_path);

	// Should prepend directory path
	TEST_ASSERT_EQUAL_STRING("/Games/Saturn/panzer1.cue", disc_path);
}

void test_getFirstDisc_deep_directory(void) {
	mock_fs_add_file("/a/b/c/d/game.m3u", "disc.iso\n");
	mock_fs_add_file("/a/b/c/d/disc.iso", "data");

	char disc_path[256];
	M3U_getFirstDisc("/a/b/c/d/game.m3u", disc_path);

	TEST_ASSERT_EQUAL_STRING("/a/b/c/d/disc.iso", disc_path);
}

void test_getFirstDisc_subdirectory_in_disc_path(void) {
	// Some M3Us reference discs in subdirectories
	mock_fs_add_file("/Roms/PS1/FF7.m3u", "discs/FF7_1.bin\n");
	mock_fs_add_file("/Roms/PS1/discs/FF7_1.bin", "data");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/FF7.m3u", disc_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/discs/FF7_1.bin", disc_path);
}

///////////////////////////////
// Error Cases
///////////////////////////////

void test_getFirstDisc_m3u_file_not_found(void) {
	// M3U doesn't exist
	char disc_path[256];
	int result = M3U_getFirstDisc("/nonexistent.m3u", disc_path);

	TEST_ASSERT_FALSE(result);
}

void test_getFirstDisc_disc_file_not_found(void) {
	// M3U exists but disc doesn't
	mock_fs_add_file("/Roms/PS1/Game.m3u", "missing_disc.bin\n");
	// Note: not creating the disc file

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/Game.m3u", disc_path);

	TEST_ASSERT_FALSE(result);
}

void test_getFirstDisc_empty_m3u_file(void) {
	// M3U exists but is empty
	mock_fs_add_file("/Roms/PS1/Empty.m3u", "");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/Empty.m3u", disc_path);

	TEST_ASSERT_FALSE(result);
}

void test_getFirstDisc_m3u_only_empty_lines(void) {
	// M3U with only whitespace/newlines
	mock_fs_add_file("/Roms/PS1/Blank.m3u", "\n\n\n\n");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/PS1/Blank.m3u", disc_path);

	TEST_ASSERT_FALSE(result);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_getFirstDisc_realistic_final_fantasy(void) {
	// Realistic Final Fantasy VII scenario
	mock_fs_add_file("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA).m3u",
	                 "Final Fantasy VII (USA) (Disc 1).bin\n"
	                 "Final Fantasy VII (USA) (Disc 2).bin\n"
	                 "Final Fantasy VII (USA) (Disc 3).bin\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA) (Disc 1).bin", "disc");
	mock_fs_add_file("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA) (Disc 2).bin", "disc");
	mock_fs_add_file("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA) (Disc 3).bin", "disc");

	char disc_path[256];
	int result = M3U_getFirstDisc("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA).m3u", disc_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA) (Disc 1).bin",
	                          disc_path);
}

void test_getFirstDisc_with_cue_files(void) {
	// Some games use .cue files instead of .bin
	mock_fs_add_file("/Roms/Saturn/Panzer Dragoon.m3u",
	                 "Panzer Dragoon (Disc 1).cue\n"
	                 "Panzer Dragoon (Disc 2).cue\n");

	mock_fs_add_file("/Roms/Saturn/Panzer Dragoon (Disc 1).cue", "cue");
	mock_fs_add_file("/Roms/Saturn/Panzer Dragoon (Disc 2).cue", "cue");

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/Saturn/Panzer Dragoon.m3u", disc_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/Saturn/Panzer Dragoon (Disc 1).cue", disc_path);
}

void test_getFirstDisc_stops_at_first_valid_disc(void) {
	// First line references missing file, second line has valid disc
	mock_fs_add_file("/Roms/Test.m3u",
	                 "missing.bin\n"
	                 "exists.bin\n");

	mock_fs_add_file("/Roms/exists.bin", "data");
	// Note: not creating missing.bin

	char disc_path[256];
	int result = M3U_getFirstDisc("/Roms/Test.m3u", disc_path);

	// Should return false because first disc doesn't exist
	// (Function only checks first non-empty line)
	TEST_ASSERT_FALSE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/missing.bin", disc_path);
}

///////////////////////////////
// M3U_getAllDiscs() Tests
///////////////////////////////

void test_getAllDiscs_single_disc(void) {
	mock_fs_add_file("/Roms/PS1/Game.m3u", "game.bin\n");
	mock_fs_add_file("/Roms/PS1/game.bin", "disc");

	int count;
	M3U_Disc** discs = M3U_getAllDiscs("/Roms/PS1/Game.m3u", &count);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/game.bin", discs[0]->path);
	TEST_ASSERT_EQUAL_STRING("Disc 1", discs[0]->name);
	TEST_ASSERT_EQUAL_INT(1, discs[0]->disc_number);

	M3U_freeDiscs(discs, count);
}

void test_getAllDiscs_multiple_discs(void) {
	mock_fs_add_file("/Roms/PS1/FF7.m3u",
	                 "FF7 (Disc 1).bin\n"
	                 "FF7 (Disc 2).bin\n"
	                 "FF7 (Disc 3).bin\n");
	mock_fs_add_file("/Roms/PS1/FF7 (Disc 1).bin", "d1");
	mock_fs_add_file("/Roms/PS1/FF7 (Disc 2).bin", "d2");
	mock_fs_add_file("/Roms/PS1/FF7 (Disc 3).bin", "d3");

	int count;
	M3U_Disc** discs = M3U_getAllDiscs("/Roms/PS1/FF7.m3u", &count);

	TEST_ASSERT_EQUAL_INT(3, count);

	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF7 (Disc 1).bin", discs[0]->path);
	TEST_ASSERT_EQUAL_STRING("Disc 1", discs[0]->name);
	TEST_ASSERT_EQUAL_INT(1, discs[0]->disc_number);

	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF7 (Disc 2).bin", discs[1]->path);
	TEST_ASSERT_EQUAL_STRING("Disc 2", discs[1]->name);

	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF7 (Disc 3).bin", discs[2]->path);
	TEST_ASSERT_EQUAL_STRING("Disc 3", discs[2]->name);

	M3U_freeDiscs(discs, count);
}

void test_getAllDiscs_skips_missing_discs(void) {
	mock_fs_add_file("/Roms/Game.m3u",
	                 "disc1.bin\n"
	                 "disc2.bin\n"
	                 "disc3.bin\n");
	// Only create disc1 and disc3
	mock_fs_add_file("/Roms/disc1.bin", "d1");
	mock_fs_add_file("/Roms/disc3.bin", "d3");

	int count;
	M3U_Disc** discs = M3U_getAllDiscs("/Roms/Game.m3u", &count);

	// Should only return discs that exist (1 and 3)
	// But numbered sequentially as 1 and 2
	TEST_ASSERT_EQUAL_INT(2, count);
	TEST_ASSERT_EQUAL_STRING("/Roms/disc1.bin", discs[0]->path);
	TEST_ASSERT_EQUAL_STRING("Disc 1", discs[0]->name);
	TEST_ASSERT_EQUAL_STRING("/Roms/disc3.bin", discs[1]->path);
	TEST_ASSERT_EQUAL_STRING("Disc 2", discs[1]->name);

	M3U_freeDiscs(discs, count);
}

void test_getAllDiscs_skips_empty_lines(void) {
	mock_fs_add_file("/Test.m3u",
	                 "\n"
	                 "d1.bin\n"
	                 "\n"
	                 "d2.bin\n");
	mock_fs_add_file("/d1.bin", "disc");
	mock_fs_add_file("/d2.bin", "disc");

	int count;
	M3U_Disc** discs = M3U_getAllDiscs("/Test.m3u", &count);

	TEST_ASSERT_EQUAL_INT(2, count);

	M3U_freeDiscs(discs, count);
}

void test_getAllDiscs_empty_m3u_returns_zero(void) {
	mock_fs_add_file("/Empty.m3u", "");

	int count;
	M3U_Disc** discs = M3U_getAllDiscs("/Empty.m3u", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	M3U_freeDiscs(discs, count);
}

void test_getAllDiscs_m3u_not_found_returns_zero(void) {
	int count;
	M3U_Disc** discs = M3U_getAllDiscs("/nonexistent.m3u", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	M3U_freeDiscs(discs, count);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// getFirstDisc() tests
	RUN_TEST(test_getFirstDisc_single_disc_m3u);
	RUN_TEST(test_getFirstDisc_multi_disc_m3u_returns_first);
	RUN_TEST(test_getFirstDisc_skips_empty_lines);
	RUN_TEST(test_getFirstDisc_handles_windows_newlines);
	RUN_TEST(test_getFirstDisc_constructs_full_path);
	RUN_TEST(test_getFirstDisc_deep_directory);
	RUN_TEST(test_getFirstDisc_subdirectory_in_disc_path);
	RUN_TEST(test_getFirstDisc_m3u_file_not_found);
	RUN_TEST(test_getFirstDisc_disc_file_not_found);
	RUN_TEST(test_getFirstDisc_empty_m3u_file);
	RUN_TEST(test_getFirstDisc_m3u_only_empty_lines);
	RUN_TEST(test_getFirstDisc_realistic_final_fantasy);
	RUN_TEST(test_getFirstDisc_with_cue_files);
	RUN_TEST(test_getFirstDisc_stops_at_first_valid_disc);

	// getAllDiscs() tests
	RUN_TEST(test_getAllDiscs_single_disc);
	RUN_TEST(test_getAllDiscs_multiple_discs);
	RUN_TEST(test_getAllDiscs_skips_missing_discs);
	RUN_TEST(test_getAllDiscs_skips_empty_lines);
	RUN_TEST(test_getAllDiscs_empty_m3u_returns_zero);
	RUN_TEST(test_getAllDiscs_m3u_not_found_returns_zero);

	return UNITY_END();
}
