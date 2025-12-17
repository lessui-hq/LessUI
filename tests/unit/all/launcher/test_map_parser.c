/**
 * test_map_parser.c - Unit tests for map.txt parser
 *
 * Tests ROM display name aliasing logic extracted from player.c/launcher.c.
 * Uses file system mocking to test file reading without actual files.
 *
 * Test coverage:
 * - Basic alias lookup (single entry, multiple entries)
 * - Tab-delimited format parsing
 * - Case sensitivity
 * - Missing files/entries
 * - Empty lines and malformed entries
 * - Hidden ROMs (alias starts with '.')
 * - Merged maps (pak-bundled + user maps with precedence)
 *
 * Note: Uses GCC --wrap for file mocking (Docker-only)
 */

#include "unity.h"
#include "../../../support/fs_mocks.h"
#include "launcher_map.h"

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

void test_getAlias_duplicate_uses_last_value(void) {
	// Duplicate entries - hash map semantics: last value wins
	mock_fs_add_file("/Roms/map.txt",
	                 "game.rom\tFirst Alias\n"
	                 "game.rom\tSecond Alias\n");

	char alias[256] = "";
	Map_getAlias("/Roms/game.rom", alias);

	TEST_ASSERT_EQUAL_STRING("Second Alias", alias);
}

///////////////////////////////
// Map_load() Direct Tests
///////////////////////////////

void test_Map_load_returns_null_for_nonexistent_file(void) {
	MapEntry* map = Map_load("/nonexistent/map.txt");
	TEST_ASSERT_NULL(map);
}

void test_Map_load_returns_empty_map_for_empty_file(void) {
	mock_fs_add_file("/Roms/map.txt", "");

	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(0, shlen(map));

	Map_free(map);
}

void test_Map_load_parses_single_entry(void) {
	mock_fs_add_file("/Roms/map.txt", "game.gb\tGame Name\n");

	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(1, shlen(map));

	char* value = shget(map, "game.gb");
	TEST_ASSERT_EQUAL_STRING("Game Name", value);

	Map_free(map);
}

void test_Map_load_parses_multiple_entries(void) {
	mock_fs_add_file("/Roms/map.txt",
	                 "game1.gb\tFirst Game\n"
	                 "game2.gb\tSecond Game\n"
	                 "game3.gb\tThird Game\n");

	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(3, shlen(map));

	TEST_ASSERT_EQUAL_STRING("First Game", shget(map, "game1.gb"));
	TEST_ASSERT_EQUAL_STRING("Second Game", shget(map, "game2.gb"));
	TEST_ASSERT_EQUAL_STRING("Third Game", shget(map, "game3.gb"));

	Map_free(map);
}

void test_Map_load_skips_malformed_lines(void) {
	mock_fs_add_file("/Roms/map.txt",
	                 "no tab here\n"
	                 "game.gb\tValid Entry\n"
	                 "also no tab\n");

	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(1, shlen(map));
	TEST_ASSERT_EQUAL_STRING("Valid Entry", shget(map, "game.gb"));

	Map_free(map);
}

void test_Map_load_handles_duplicate_keys(void) {
	mock_fs_add_file("/Roms/map.txt",
	                 "game.gb\tFirst Value\n"
	                 "game.gb\tSecond Value\n");

	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(1, shlen(map));
	// Last value wins
	TEST_ASSERT_EQUAL_STRING("Second Value", shget(map, "game.gb"));

	Map_free(map);
}

void test_Map_load_handles_windows_newlines(void) {
	mock_fs_add_file("/Roms/map.txt", "game.gb\tGame Name\r\n");

	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL_STRING("Game Name", shget(map, "game.gb"));

	Map_free(map);
}

void test_Map_load_skips_empty_lines(void) {
	mock_fs_add_file("/Roms/map.txt",
	                 "\n"
	                 "game.gb\tGame Name\n"
	                 "\n");

	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(1, shlen(map));

	Map_free(map);
}

///////////////////////////////
// Map_free() Direct Tests
///////////////////////////////

void test_Map_free_handles_null(void) {
	// Should not crash
	Map_free(NULL);
	TEST_PASS();
}

void test_Map_free_handles_empty_map(void) {
	mock_fs_add_file("/Roms/map.txt", "");
	MapEntry* map = Map_load("/Roms/map.txt");
	TEST_ASSERT_NOT_NULL(map);

	// Should not crash
	Map_free(map);
	TEST_PASS();
}

///////////////////////////////
// Map_loadForDirectory() Tests
///////////////////////////////

void test_loadForDirectory_returns_null_when_no_maps(void) {
	// No maps exist
	MapEntry* map = Map_loadForDirectory("/Roms/GB");
	TEST_ASSERT_NULL(map);
}

void test_loadForDirectory_loads_user_map_only(void) {
	mock_fs_add_file("/Roms/GB/map.txt",
	                 "mario.gb\tSuper Mario\n"
	                 "zelda.gb\tZelda\n");

	MapEntry* map = Map_loadForDirectory("/Roms/GB");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(2, shlen(map));
	TEST_ASSERT_EQUAL_STRING("Super Mario", shget(map, "mario.gb"));
	TEST_ASSERT_EQUAL_STRING("Zelda", shget(map, "zelda.gb"));

	Map_free(map);
}

void test_loadForDirectory_loads_pak_map_only(void) {
	// Pak map exists in .system/common/
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/GB.pak/map.txt",
	                 "mario.gb\tSuper Mario (Pak)\n");

	MapEntry* map = Map_loadForDirectory("/tmp/test/Roms/GB");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL_STRING("Super Mario (Pak)", shget(map, "mario.gb"));

	Map_free(map);
}

void test_loadForDirectory_merges_pak_and_user_maps(void) {
	// Pak map has game1 and game2
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/NES.pak/map.txt",
	                 "game1.nes\tGame One (Pak)\n"
	                 "game2.nes\tGame Two (Pak)\n");

	// User map overrides game2 and adds game3
	mock_fs_add_file("/tmp/test/Roms/NES/map.txt",
	                 "game2.nes\tGame Two (User)\n"
	                 "game3.nes\tGame Three (User)\n");

	MapEntry* map = Map_loadForDirectory("/tmp/test/Roms/NES");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(3, shlen(map));

	// game1 from pak (not overridden)
	TEST_ASSERT_EQUAL_STRING("Game One (Pak)", shget(map, "game1.nes"));
	// game2 from user (overridden)
	TEST_ASSERT_EQUAL_STRING("Game Two (User)", shget(map, "game2.nes"));
	// game3 from user (new)
	TEST_ASSERT_EQUAL_STRING("Game Three (User)", shget(map, "game3.nes"));

	Map_free(map);
}

void test_loadForDirectory_user_completely_overrides_pak_entry(void) {
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/SNES.pak/map.txt",
	                 "game.sfc\tOriginal Name\n");

	mock_fs_add_file("/tmp/test/Roms/SNES/map.txt",
	                 "game.sfc\tCustom Name\n");

	MapEntry* map = Map_loadForDirectory("/tmp/test/Roms/SNES");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL_STRING("Custom Name", shget(map, "game.sfc"));

	Map_free(map);
}

void test_loadForDirectory_handles_empty_user_map(void) {
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/GBA.pak/map.txt",
	                 "game.gba\tPak Game\n");

	mock_fs_add_file("/tmp/test/Roms/GBA/map.txt", "");

	MapEntry* map = Map_loadForDirectory("/tmp/test/Roms/GBA");
	TEST_ASSERT_NOT_NULL(map);
	// Should still have pak entries
	TEST_ASSERT_EQUAL_STRING("Pak Game", shget(map, "game.gba"));

	Map_free(map);
}

void test_loadForDirectory_handles_empty_pak_map(void) {
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/PCE.pak/map.txt", "");

	mock_fs_add_file("/tmp/test/Roms/PCE/map.txt",
	                 "game.pce\tUser Game\n");

	MapEntry* map = Map_loadForDirectory("/tmp/test/Roms/PCE");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL_STRING("User Game", shget(map, "game.pce"));

	Map_free(map);
}

void test_loadForDirectory_arcade_realistic_scenario(void) {
	// Large pak map (simulating arcade naming database)
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/FBNEO.pak/map.txt",
	                 "mslug.zip\tMetal Slug\n"
	                 "mslug2.zip\tMetal Slug 2\n"
	                 "mslug3.zip\tMetal Slug 3\n"
	                 "kof98.zip\tThe King of Fighters '98\n"
	                 "kof99.zip\tThe King of Fighters '99\n"
	                 "sf2.zip\tStreet Fighter II\n"
	                 "sf2ce.zip\tStreet Fighter II' Champion Edition\n");

	// User customizes a few names
	mock_fs_add_file("/tmp/test/Roms/FBNEO/map.txt",
	                 "mslug.zip\tMetal Slug (Best Game!)\n"
	                 "sf2.zip\t.Street Fighter II\n"); // Hidden

	MapEntry* map = Map_loadForDirectory("/tmp/test/Roms/FBNEO");
	TEST_ASSERT_NOT_NULL(map);
	TEST_ASSERT_EQUAL(7, shlen(map));

	// User overrides
	TEST_ASSERT_EQUAL_STRING("Metal Slug (Best Game!)", shget(map, "mslug.zip"));
	TEST_ASSERT_EQUAL_STRING(".Street Fighter II", shget(map, "sf2.zip"));

	// Pak defaults preserved
	TEST_ASSERT_EQUAL_STRING("Metal Slug 2", shget(map, "mslug2.zip"));
	TEST_ASSERT_EQUAL_STRING("The King of Fighters '98", shget(map, "kof98.zip"));

	Map_free(map);
}

///////////////////////////////
// Merged Map Tests (Pak + User)
///////////////////////////////

void test_getAlias_shared_common_map(void) {
	// Test shared map in .system/common/ (new generic shared system)
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/MAME.pak/map.txt",
	                 "test.zip\tTest Game (Shared)\n");

	char alias[256] = "";
	Map_getAlias("/tmp/test/Roms/MAME/test.zip", alias);
	TEST_ASSERT_EQUAL_STRING("Test Game (Shared)", alias);
}

void test_getAlias_platform_overrides_common(void) {
	// Test that platform-specific map overrides shared common map
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/MAME.pak/map.txt",
	                 "test.zip\tTest Game (Shared)\n");
	mock_fs_add_file("/tmp/test/.system/test/paks/Emus/MAME.pak/map.txt",
	                 "test.zip\tTest Game (Platform)\n");

	char alias[256] = "";
	Map_getAlias("/tmp/test/Roms/MAME/test.zip", alias);

	// Platform-specific should win
	TEST_ASSERT_EQUAL_STRING("Test Game (Platform)", alias);
}

void test_getAlias_uses_pak_map_when_no_user_map(void) {
	// Set up shared common pak map
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/MAME.pak/map.txt",
	                 "mslug.zip\tMetal Slug\n"
	                 "kof98.zip\tThe King of Fighters '98\n");

	char alias[256] = "";
	Map_getAlias("/tmp/test/Roms/MAME/mslug.zip", alias);

	TEST_ASSERT_EQUAL_STRING("Metal Slug", alias);
}

void test_getAlias_user_map_overrides_pak_map(void) {
	// Set up shared pak map with default name
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/MAME.pak/map.txt",
	                 "mslug.zip\tMetal Slug (Default)\n");

	// Set up user map with custom name
	mock_fs_add_file("/tmp/test/Roms/MAME/map.txt",
	                 "mslug.zip\tMetal Slug (Custom)\n");

	char alias[256] = "";
	Map_getAlias("/tmp/test/Roms/MAME/mslug.zip", alias);

	// User map should win
	TEST_ASSERT_EQUAL_STRING("Metal Slug (Custom)", alias);
}

void test_getAlias_merges_pak_and_user_maps(void) {
	// Shared pak map has game1 and game2
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/MAME.pak/map.txt",
	                 "game1.zip\tGame One (Pak)\n"
	                 "game2.zip\tGame Two (Pak)\n");

	// User map has game2 (override) and game3 (new)
	mock_fs_add_file("/tmp/test/Roms/MAME/map.txt",
	                 "game2.zip\tGame Two (User)\n"
	                 "game3.zip\tGame Three (User)\n");

	char alias1[256] = "";
	char alias2[256] = "";
	char alias3[256] = "";

	// game1 should come from pak
	Map_getAlias("/tmp/test/Roms/MAME/game1.zip", alias1);
	TEST_ASSERT_EQUAL_STRING("Game One (Pak)", alias1);

	// game2 should come from user (override)
	Map_getAlias("/tmp/test/Roms/MAME/game2.zip", alias2);
	TEST_ASSERT_EQUAL_STRING("Game Two (User)", alias2);

	// game3 should come from user (new)
	Map_getAlias("/tmp/test/Roms/MAME/game3.zip", alias3);
	TEST_ASSERT_EQUAL_STRING("Game Three (User)", alias3);
}

void test_getAlias_user_map_only_still_works(void) {
	// No pak map, only user map (backward compatibility)
	mock_fs_add_file("/tmp/test/Roms/GB/map.txt",
	                 "mario.gb\tSuper Mario Land\n");

	char alias[256] = "";
	Map_getAlias("/tmp/test/Roms/GB/mario.gb", alias);

	TEST_ASSERT_EQUAL_STRING("Super Mario Land", alias);
}

void test_getAlias_arcade_game_realistic_workflow(void) {
	// Realistic arcade setup: shared pak has 100+ games, user overrides a few
	mock_fs_add_file("/tmp/test/.system/common/paks/Emus/FBNEO.pak/map.txt",
	                 "mslug.zip\tMetal Slug\n"
	                 "mslug2.zip\tMetal Slug 2\n"
	                 "mslugx.zip\tMetal Slug X\n"
	                 "kof98.zip\tThe King of Fighters '98\n"
	                 "sf2.zip\tStreet Fighter II\n");

	// User only overrides one game
	mock_fs_add_file("/tmp/test/Roms/FBNEO/map.txt",
	                 "sf2.zip\tSF2 (My Favorite!)\n");

	char ms_alias[256] = "";
	char kof_alias[256] = "";
	char sf2_alias[256] = "";

	// Most games use shared pak defaults
	Map_getAlias("/tmp/test/Roms/FBNEO/mslug.zip", ms_alias);
	TEST_ASSERT_EQUAL_STRING("Metal Slug", ms_alias);

	Map_getAlias("/tmp/test/Roms/FBNEO/kof98.zip", kof_alias);
	TEST_ASSERT_EQUAL_STRING("The King of Fighters '98", kof_alias);

	// User's custom name wins
	Map_getAlias("/tmp/test/Roms/FBNEO/sf2.zip", sf2_alias);
	TEST_ASSERT_EQUAL_STRING("SF2 (My Favorite!)", sf2_alias);
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
	RUN_TEST(test_getAlias_duplicate_uses_last_value);

	// Map_load() direct tests
	RUN_TEST(test_Map_load_returns_null_for_nonexistent_file);
	RUN_TEST(test_Map_load_returns_empty_map_for_empty_file);
	RUN_TEST(test_Map_load_parses_single_entry);
	RUN_TEST(test_Map_load_parses_multiple_entries);
	RUN_TEST(test_Map_load_skips_malformed_lines);
	RUN_TEST(test_Map_load_handles_duplicate_keys);
	RUN_TEST(test_Map_load_handles_windows_newlines);
	RUN_TEST(test_Map_load_skips_empty_lines);

	// Map_free() direct tests
	RUN_TEST(test_Map_free_handles_null);
	RUN_TEST(test_Map_free_handles_empty_map);

	// Map_loadForDirectory() tests
	RUN_TEST(test_loadForDirectory_returns_null_when_no_maps);
	RUN_TEST(test_loadForDirectory_loads_user_map_only);
	RUN_TEST(test_loadForDirectory_loads_pak_map_only);
	RUN_TEST(test_loadForDirectory_merges_pak_and_user_maps);
	RUN_TEST(test_loadForDirectory_user_completely_overrides_pak_entry);
	RUN_TEST(test_loadForDirectory_handles_empty_user_map);
	RUN_TEST(test_loadForDirectory_handles_empty_pak_map);
	RUN_TEST(test_loadForDirectory_arcade_realistic_scenario);

	// Merged maps (pak + user) with .system/common/ shared resources
	RUN_TEST(test_getAlias_shared_common_map);
	RUN_TEST(test_getAlias_platform_overrides_common);
	RUN_TEST(test_getAlias_uses_pak_map_when_no_user_map);
	RUN_TEST(test_getAlias_user_map_overrides_pak_map);
	RUN_TEST(test_getAlias_merges_pak_and_user_maps);
	RUN_TEST(test_getAlias_user_map_only_still_works);
	RUN_TEST(test_getAlias_arcade_game_realistic_workflow);

	return UNITY_END();
}
