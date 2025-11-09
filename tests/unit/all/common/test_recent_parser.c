/**
 * test_recent_parser.c - Unit tests for recent.txt parser
 *
 * Tests recently played games file parsing extracted from minui.c.
 * Recent.txt uses tab-delimited format: path<TAB>alias
 *
 * Test coverage:
 * - Basic parsing (with and without aliases)
 * - Tab-delimited format handling
 * - Missing ROM filtering
 * - Empty line handling
 * - Order preservation
 *
 * Note: Uses GCC --wrap for file mocking (Docker-only)
 * For write tests, see test_recent_writer.c (uses real temp files)
 */

#include "../../../support/unity/unity.h"
#include "../../../support/fs_mocks.h"
#include "../../../../workspace/all/common/recent_file.h"

#include <string.h>

void setUp(void) {
	mock_fs_reset();
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Basic Parsing Tests
///////////////////////////////

void test_Recent_parse_single_entry_no_alias(void) {
	mock_fs_add_file("/recent.txt", "/Roms/GB/mario.gb\n");
	mock_fs_add_file("/mnt/SDCARD/Roms/GB/mario.gb", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/mario.gb", entries[0]->path);
	TEST_ASSERT_NULL(entries[0]->alias);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_single_entry_with_alias(void) {
	mock_fs_add_file("/recent.txt", "/Roms/GB/mario.gb\tSuper Mario Land\n");
	mock_fs_add_file("/mnt/SDCARD/Roms/GB/mario.gb", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/mario.gb", entries[0]->path);
	TEST_ASSERT_EQUAL_STRING("Super Mario Land", entries[0]->alias);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_multiple_entries_mixed(void) {
	mock_fs_add_file("/recent.txt",
	                 "/Roms/GB/mario.gb\tSuper Mario\n"
	                 "/Roms/NES/zelda.nes\n"
	                 "/Roms/SNES/metroid.smc\tSuper Metroid\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/GB/mario.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/NES/zelda.nes", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/SNES/metroid.smc", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(3, count);

	// First entry: has alias
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/mario.gb", entries[0]->path);
	TEST_ASSERT_EQUAL_STRING("Super Mario", entries[0]->alias);

	// Second entry: no alias
	TEST_ASSERT_EQUAL_STRING("/Roms/NES/zelda.nes", entries[1]->path);
	TEST_ASSERT_NULL(entries[1]->alias);

	// Third entry: has alias
	TEST_ASSERT_EQUAL_STRING("/Roms/SNES/metroid.smc", entries[2]->path);
	TEST_ASSERT_EQUAL_STRING("Super Metroid", entries[2]->alias);

	Recent_freeEntries(entries, count);
}

///////////////////////////////
// Filtering Tests
///////////////////////////////

void test_Recent_parse_skips_missing_roms(void) {
	mock_fs_add_file("/recent.txt",
	                 "/Roms/exists.gb\n"
	                 "/Roms/missing.gb\n"
	                 "/Roms/also_exists.nes\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/exists.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/also_exists.nes", "rom");
	// Not creating missing.gb

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	// Should only include the 2 that exist
	TEST_ASSERT_EQUAL_INT(2, count);
	TEST_ASSERT_EQUAL_STRING("/Roms/exists.gb", entries[0]->path);
	TEST_ASSERT_EQUAL_STRING("/Roms/also_exists.nes", entries[1]->path);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_skips_empty_lines(void) {
	mock_fs_add_file("/recent.txt",
	                 "\n"
	                 "/Roms/game1.gb\n"
	                 "\n"
	                 "/Roms/game2.gb\n"
	                 "\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/game1.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/game2.gb", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(2, count);

	Recent_freeEntries(entries, count);
}

///////////////////////////////
// Format Tests
///////////////////////////////

void test_Recent_parse_handles_windows_newlines(void) {
	mock_fs_add_file("/recent.txt", "/Roms/game.gb\tAlias\r\n");
	mock_fs_add_file("/mnt/SDCARD/Roms/game.gb", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("Alias", entries[0]->alias);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_alias_with_special_characters(void) {
	mock_fs_add_file("/recent.txt", "/Roms/game.gb\tGame™ - The Best! (v1.1)\n");
	mock_fs_add_file("/mnt/SDCARD/Roms/game.gb", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_STRING("Game™ - The Best! (v1.1)", entries[0]->alias);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_entry_without_tab_has_no_alias(void) {
	mock_fs_add_file("/recent.txt", "/Roms/game.gb\n");
	mock_fs_add_file("/mnt/SDCARD/Roms/game.gb", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_NULL(entries[0]->alias);

	Recent_freeEntries(entries, count);
}

///////////////////////////////
// Error Cases
///////////////////////////////

void test_Recent_parse_file_not_found_returns_zero(void) {
	int count;
	Recent_Entry** entries = Recent_parse("/nonexistent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_empty_file_returns_zero(void) {
	mock_fs_add_file("/recent.txt", "");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_all_roms_missing_returns_zero(void) {
	mock_fs_add_file("/recent.txt",
	                 "/Roms/missing1.gb\n"
	                 "/Roms/missing2.nes\n");

	// Don't create any ROM files
	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	Recent_freeEntries(entries, count);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_Recent_parse_realistic_recent_list(void) {
	mock_fs_add_file("/mnt/SDCARD/.minui/recent.txt",
	                 "/Roms/PS1/Final Fantasy VII/FF7 (Disc 1).bin\tFF7\n"
	                 "/Roms/GB/Pokemon - Red Version (USA).gb\tPokemon Red\n"
	                 "/Roms/NES/Super Mario Bros (World).nes\n"
	                 "/Roms/SNES/Super Metroid (USA).smc\tMetroid\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/PS1/Final Fantasy VII/FF7 (Disc 1).bin", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA).gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/NES/Super Mario Bros (World).nes", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/SNES/Super Metroid (USA).smc", "rom");

	int count;
	Recent_Entry** entries =
	    Recent_parse("/mnt/SDCARD/.minui/recent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(4, count);

	// Verify paths and aliases
	TEST_ASSERT_TRUE(strstr(entries[0]->path, "FF7") != NULL);
	TEST_ASSERT_EQUAL_STRING("FF7", entries[0]->alias);

	TEST_ASSERT_TRUE(strstr(entries[1]->path, "Pokemon") != NULL);
	TEST_ASSERT_EQUAL_STRING("Pokemon Red", entries[1]->alias);

	TEST_ASSERT_TRUE(strstr(entries[2]->path, "Mario") != NULL);
	TEST_ASSERT_NULL(entries[2]->alias);

	Recent_freeEntries(entries, count);
}

void test_Recent_parse_maintains_order(void) {
	// Recent list is ordered newest first
	mock_fs_add_file("/recent.txt",
	                 "/Roms/newest.gb\n"
	                 "/Roms/middle.gb\n"
	                 "/Roms/oldest.gb\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/newest.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/middle.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/oldest.gb", "rom");

	int count;
	Recent_Entry** entries = Recent_parse("/recent.txt", "/mnt/SDCARD", &count);

	// Order should match file order (newest first)
	TEST_ASSERT_EQUAL_STRING("/Roms/newest.gb", entries[0]->path);
	TEST_ASSERT_EQUAL_STRING("/Roms/middle.gb", entries[1]->path);
	TEST_ASSERT_EQUAL_STRING("/Roms/oldest.gb", entries[2]->path);

	Recent_freeEntries(entries, count);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic parsing
	RUN_TEST(test_Recent_parse_single_entry_no_alias);
	RUN_TEST(test_Recent_parse_single_entry_with_alias);
	RUN_TEST(test_Recent_parse_multiple_entries_mixed);

	// Filtering
	RUN_TEST(test_Recent_parse_skips_missing_roms);
	RUN_TEST(test_Recent_parse_skips_empty_lines);

	// Format handling
	RUN_TEST(test_Recent_parse_handles_windows_newlines);
	RUN_TEST(test_Recent_parse_alias_with_special_characters);
	RUN_TEST(test_Recent_parse_entry_without_tab_has_no_alias);

	// Error cases
	RUN_TEST(test_Recent_parse_file_not_found_returns_zero);
	RUN_TEST(test_Recent_parse_empty_file_returns_zero);
	RUN_TEST(test_Recent_parse_all_roms_missing_returns_zero);

	// Integration
	RUN_TEST(test_Recent_parse_realistic_recent_list);
	RUN_TEST(test_Recent_parse_maintains_order);

	return UNITY_END();
}
