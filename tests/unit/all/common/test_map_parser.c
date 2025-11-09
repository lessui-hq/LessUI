/**
 * test_map_parser.c - Unit tests for map.txt parser
 *
 * Tests ROM display name aliasing logic extracted from minarch.c/minui.c.
 * Uses file system mocking to test file reading without actual files.
 *
 * Test coverage:
 * - Basic alias lookup (single entry, multiple entries)
 * - Tab-delimited format parsing
 * - Case sensitivity
 * - Missing files/entries
 * - Empty lines and malformed entries
 * - Hidden ROMs (alias starts with '.')
 *
 * Note: Uses GCC --wrap for file mocking (Docker-only)
 */

#include "../../../support/unity/unity.h"
#include "../../../support/fs_mocks.h"
#include "../../../../workspace/all/common/map_parser.h"

#include <string.h>

void setUp(void) {
	mock_fs_reset();
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Basic Alias Lookup Tests
///////////////////////////////

void test_getAlias_finds_single_entry(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "mario.gb\tSuper Mario Land\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/mario.gb", alias);

	TEST_ASSERT_EQUAL_STRING("Super Mario Land", alias);
}

void test_getAlias_finds_entry_in_multi_line_map(void) {
	mock_fs_add_file("/Roms/GB/map.txt",
	                 "mario.gb\tSuper Mario Land\n"
	                 "zelda.gb\tLink's Awakening\n"
	                 "tetris.gb\tTetris\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/zelda.gb", alias);

	TEST_ASSERT_EQUAL_STRING("Link's Awakening", alias);
}

void test_getAlias_finds_first_entry(void) {
	mock_fs_add_file("/Roms/NES/map.txt",
	                 "mario.nes\tSuper Mario Bros\n"
	                 "zelda.nes\tThe Legend of Zelda\n");

	char alias[256] = "";
	Map_getAlias("/Roms/NES/mario.nes", alias);

	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", alias);
}

void test_getAlias_finds_last_entry(void) {
	mock_fs_add_file("/Roms/NES/map.txt",
	                 "mario.nes\tSuper Mario Bros\n"
	                 "zelda.nes\tThe Legend of Zelda\n");

	char alias[256] = "";
	Map_getAlias("/Roms/NES/zelda.nes", alias);

	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", alias);
}

///////////////////////////////
// No Map / No Match Tests
///////////////////////////////

void test_getAlias_no_map_file_leaves_alias_unchanged(void) {
	// No map.txt exists
	char alias[256] = "Original Name";
	Map_getAlias("/Roms/GB/game.gb", alias);

	// Alias should be unchanged
	TEST_ASSERT_EQUAL_STRING("Original Name", alias);
}

void test_getAlias_rom_not_in_map_leaves_alias_unchanged(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "mario.gb\tSuper Mario Land\n");

	char alias[256] = "Tetris";
	Map_getAlias("/Roms/GB/tetris.gb", alias);

	// Alias should be unchanged (tetris.gb not in map)
	TEST_ASSERT_EQUAL_STRING("Tetris", alias);
}

void test_getAlias_empty_map_file_leaves_alias_unchanged(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "");

	char alias[256] = "Default";
	Map_getAlias("/Roms/GB/game.gb", alias);

	TEST_ASSERT_EQUAL_STRING("Default", alias);
}

///////////////////////////////
// Format Handling Tests
///////////////////////////////

void test_getAlias_skips_empty_lines(void) {
	mock_fs_add_file("/Roms/GB/map.txt",
	                 "\n"
	                 "\n"
	                 "mario.gb\tSuper Mario\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/mario.gb", alias);

	TEST_ASSERT_EQUAL_STRING("Super Mario", alias);
}

void test_getAlias_handles_lines_without_tab(void) {
	// Malformed entry (no tab separator)
	mock_fs_add_file("/Roms/GB/map.txt",
	                 "broken line without tab\n"
	                 "mario.gb\tSuper Mario\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/mario.gb", alias);

	// Should skip broken line and find mario
	TEST_ASSERT_EQUAL_STRING("Super Mario", alias);
}

void test_getAlias_exact_match_required(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "mario.gb\tSuper Mario\n");

	char alias[256] = "Original";

	// Partial match should not work
	Map_getAlias("/Roms/GB/mario2.gb", alias);
	TEST_ASSERT_EQUAL_STRING("Original", alias);

	// Different case should not match (exactMatch is case-sensitive)
	Map_getAlias("/Roms/GB/MARIO.GB", alias);
	TEST_ASSERT_EQUAL_STRING("Original", alias);
}

void test_getAlias_handles_windows_newlines(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "mario.gb\tSuper Mario\r\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/mario.gb", alias);

	TEST_ASSERT_EQUAL_STRING("Super Mario", alias);
}

///////////////////////////////
// Special Characters Tests
///////////////////////////////

void test_getAlias_with_special_characters_in_filename(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "game (USA) (v1.1).gb\tGame USA\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/game (USA) (v1.1).gb", alias);

	TEST_ASSERT_EQUAL_STRING("Game USA", alias);
}

void test_getAlias_with_special_characters_in_alias(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "mario.gb\tSuper Mario™ - The Game!\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/mario.gb", alias);

	TEST_ASSERT_EQUAL_STRING("Super Mario™ - The Game!", alias);
}

void test_getAlias_hidden_rom_starts_with_dot(void) {
	mock_fs_add_file("/Roms/GB/map.txt", "hidden.gb\t.Hidden Game\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/hidden.gb", alias);

	// Should get the alias (even though it starts with '.')
	TEST_ASSERT_EQUAL_STRING(".Hidden Game", alias);
}

///////////////////////////////
// Path Tests
///////////////////////////////

void test_getAlias_different_directories(void) {
	mock_fs_add_file("/a/b/c/map.txt", "file.rom\tAliased Name\n");

	char alias[256] = "";
	Map_getAlias("/a/b/c/file.rom", alias);

	TEST_ASSERT_EQUAL_STRING("Aliased Name", alias);
}

void test_getAlias_deep_directory_structure(void) {
	mock_fs_add_file("/mnt/SDCARD/Roms/PS1/RPG/map.txt", "ff7.bin\tFinal Fantasy VII\n");

	char alias[256] = "";
	Map_getAlias("/mnt/SDCARD/Roms/PS1/RPG/ff7.bin", alias);

	TEST_ASSERT_EQUAL_STRING("Final Fantasy VII", alias);
}

void test_getAlias_looks_in_rom_directory_not_parent(void) {
	// map.txt in wrong location (parent directory)
	mock_fs_add_file("/Roms/map.txt", "game.gb\tWrong Location\n");
	mock_fs_add_file("/Roms/GB/map.txt", "game.gb\tCorrect Location\n");

	char alias[256] = "";
	Map_getAlias("/Roms/GB/game.gb", alias);

	// Should use map.txt from /Roms/GB/, not /Roms/
	TEST_ASSERT_EQUAL_STRING("Correct Location", alias);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_getAlias_realistic_rom_library(void) {
	// Real-world map.txt for Game Boy
	mock_fs_add_file("/mnt/SDCARD/Roms/GB/map.txt",
	                 "Super Mario Land (World).gb\tMario Land\n"
	                 "The Legend of Zelda - Link's Awakening (USA, Europe) (Rev 2).gb\tZelda LA\n"
	                 "Pokemon - Red Version (USA, Europe).gb\tPokemon Red\n"
	                 "Tetris (World) (Rev 1).gb\tTetris\n");

	char alias[256];

	// Test each ROM
	strcpy(alias, "");
	Map_getAlias("/mnt/SDCARD/Roms/GB/Super Mario Land (World).gb", alias);
	TEST_ASSERT_EQUAL_STRING("Mario Land", alias);

	strcpy(alias, "");
	Map_getAlias("/mnt/SDCARD/Roms/GB/The Legend of Zelda - Link's Awakening (USA, Europe) (Rev 2).gb", alias);
	TEST_ASSERT_EQUAL_STRING("Zelda LA", alias);

	strcpy(alias, "");
	Map_getAlias("/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb", alias);
	TEST_ASSERT_EQUAL_STRING("Pokemon Red", alias);
}

void test_getAlias_multiple_roms_same_directory(void) {
	mock_fs_add_file("/Roms/NES/map.txt",
	                 "mario1.nes\tSuper Mario Bros\n"
	                 "mario2.nes\tSuper Mario Bros 2\n"
	                 "mario3.nes\tSuper Mario Bros 3\n");

	char alias1[256] = "";
	char alias2[256] = "";
	char alias3[256] = "";

	Map_getAlias("/Roms/NES/mario1.nes", alias1);
	Map_getAlias("/Roms/NES/mario2.nes", alias2);
	Map_getAlias("/Roms/NES/mario3.nes", alias3);

	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", alias1);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros 2", alias2);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros 3", alias3);
}

void test_getAlias_hidden_roms_workflow(void) {
	// Map with both visible and hidden ROMs
	mock_fs_add_file("/Roms/GB/map.txt",
	                 "good.gb\tGood Game\n"
	                 "bad.gb\t.Bad Game\n"
	                 "test.gb\t.Test ROM\n");

	char alias1[256] = "";
	char alias2[256] = "";
	char alias3[256] = "";

	Map_getAlias("/Roms/GB/good.gb", alias1);
	Map_getAlias("/Roms/GB/bad.gb", alias2);
	Map_getAlias("/Roms/GB/test.gb", alias3);

	// All should get their aliases
	TEST_ASSERT_EQUAL_STRING("Good Game", alias1);
	TEST_ASSERT_EQUAL_STRING(".Bad Game", alias2); // Hidden (starts with .)
	TEST_ASSERT_EQUAL_STRING(".Test ROM", alias3); // Hidden
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_getAlias_path_without_directory(void) {
	char alias[256] = "Default";
	Map_getAlias("mario.gb", alias);

	// No directory, can't find map.txt
	TEST_ASSERT_EQUAL_STRING("Default", alias);
}

void test_getAlias_stops_at_first_match(void) {
	// Duplicate entries (should use first)
	mock_fs_add_file("/Roms/map.txt",
	                 "game.rom\tFirst Alias\n"
	                 "game.rom\tSecond Alias\n");

	char alias[256] = "";
	Map_getAlias("/Roms/game.rom", alias);

	TEST_ASSERT_EQUAL_STRING("First Alias", alias);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic lookup
	RUN_TEST(test_getAlias_finds_single_entry);
	RUN_TEST(test_getAlias_finds_entry_in_multi_line_map);
	RUN_TEST(test_getAlias_finds_first_entry);
	RUN_TEST(test_getAlias_finds_last_entry);

	// No map / no match
	RUN_TEST(test_getAlias_no_map_file_leaves_alias_unchanged);
	RUN_TEST(test_getAlias_rom_not_in_map_leaves_alias_unchanged);
	RUN_TEST(test_getAlias_empty_map_file_leaves_alias_unchanged);

	// Format handling
	RUN_TEST(test_getAlias_skips_empty_lines);
	RUN_TEST(test_getAlias_handles_lines_without_tab);
	RUN_TEST(test_getAlias_exact_match_required);
	RUN_TEST(test_getAlias_handles_windows_newlines);

	// Special characters
	RUN_TEST(test_getAlias_with_special_characters_in_filename);
	RUN_TEST(test_getAlias_with_special_characters_in_alias);
	RUN_TEST(test_getAlias_hidden_rom_starts_with_dot);

	// Path tests
	RUN_TEST(test_getAlias_different_directories);
	RUN_TEST(test_getAlias_deep_directory_structure);
	RUN_TEST(test_getAlias_looks_in_rom_directory_not_parent);

	// Integration
	RUN_TEST(test_getAlias_realistic_rom_library);
	RUN_TEST(test_getAlias_multiple_roms_same_directory);
	RUN_TEST(test_getAlias_hidden_roms_workflow);

	// Edge cases
	RUN_TEST(test_getAlias_path_without_directory);
	RUN_TEST(test_getAlias_stops_at_first_match);

	return UNITY_END();
}
