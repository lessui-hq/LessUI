/**
 * test_collection_parser.c - Unit tests for collection file parser
 *
 * Tests collection .txt parsing logic extracted from minui.c.
 * Collections are custom ROM lists (one path per line).
 *
 * Test coverage:
 * - Basic parsing (single entry, multiple entries)
 * - Path validation (only existing ROMs included)
 * - Empty line handling
 * - PAK vs ROM detection
 * - Error handling (missing files, empty collections)
 *
 * Note: Uses GCC --wrap for file mocking (Docker-only)
 */

#include "../../../support/unity/unity.h"
#include "../../../support/fs_mocks.h"
#include "../../../../workspace/all/common/collection_parser.h"

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

void test_Collection_parse_single_rom(void) {
	mock_fs_add_file("/Collections/Test.txt", "/Roms/GB/mario.gb\n");
	mock_fs_add_file("/mnt/SDCARD/Roms/GB/mario.gb", "rom");

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Test.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/mario.gb", entries[0]->path);
	TEST_ASSERT_FALSE(entries[0]->is_pak);

	Collection_freeEntries(entries, count);
}

void test_Collection_parse_multiple_roms(void) {
	mock_fs_add_file("/Collections/Favorites.txt",
	                 "/Roms/GB/mario.gb\n"
	                 "/Roms/NES/zelda.nes\n"
	                 "/Roms/SNES/metroid.smc\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/GB/mario.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/NES/zelda.nes", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/SNES/metroid.smc", "rom");

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Favorites.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/mario.gb", entries[0]->path);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/NES/zelda.nes", entries[1]->path);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/SNES/metroid.smc", entries[2]->path);

	Collection_freeEntries(entries, count);
}

void test_Collection_parse_skips_missing_roms(void) {
	mock_fs_add_file("/Collections/Test.txt",
	                 "/Roms/exists.gb\n"
	                 "/Roms/missing.gb\n"
	                 "/Roms/also_exists.nes\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/exists.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/also_exists.nes", "rom");
	// Not creating missing.gb

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Test.txt", "/mnt/SDCARD", &count);

	// Should only include the 2 that exist
	TEST_ASSERT_EQUAL_INT(2, count);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/exists.gb", entries[0]->path);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/also_exists.nes", entries[1]->path);

	Collection_freeEntries(entries, count);
}

void test_Collection_parse_skips_empty_lines(void) {
	mock_fs_add_file("/Collections/Test.txt",
	                 "\n"
	                 "/Roms/game1.gb\n"
	                 "\n"
	                 "\n"
	                 "/Roms/game2.gb\n"
	                 "\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/game1.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/game2.gb", "rom");

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Test.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(2, count);

	Collection_freeEntries(entries, count);
}

///////////////////////////////
// PAK vs ROM Detection Tests
///////////////////////////////

void test_Collection_parse_detects_pak_files(void) {
	mock_fs_add_file("/Collections/Apps.txt", "/Tools/say.pak\n");
	mock_fs_add_file("/mnt/SDCARD/Tools/say.pak", "pak");

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Apps.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Tools/say.pak", entries[0]->path);
	TEST_ASSERT_TRUE(entries[0]->is_pak);

	Collection_freeEntries(entries, count);
}

void test_Collection_parse_mixed_roms_and_paks(void) {
	mock_fs_add_file("/Collections/Mixed.txt",
	                 "/Roms/GB/game.gb\n"
	                 "/Tools/app.pak\n"
	                 "/Roms/NES/game.nes\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/GB/game.gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Tools/app.pak", "pak");
	mock_fs_add_file("/mnt/SDCARD/Roms/NES/game.nes", "rom");

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Mixed.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_FALSE(entries[0]->is_pak); // game.gb
	TEST_ASSERT_TRUE(entries[1]->is_pak);  // app.pak
	TEST_ASSERT_FALSE(entries[2]->is_pak); // game.nes

	Collection_freeEntries(entries, count);
}

///////////////////////////////
// Error Cases
///////////////////////////////

void test_Collection_parse_file_not_found_returns_zero(void) {
	int count;
	Collection_Entry** entries = Collection_parse("/nonexistent.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	Collection_freeEntries(entries, count);
}

void test_Collection_parse_empty_file_returns_zero(void) {
	mock_fs_add_file("/Collections/Empty.txt", "");

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Empty.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	Collection_freeEntries(entries, count);
}

void test_Collection_parse_all_roms_missing_returns_zero(void) {
	mock_fs_add_file("/Collections/Test.txt",
	                 "/Roms/missing1.gb\n"
	                 "/Roms/missing2.nes\n");

	// Don't create any ROM files
	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Test.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(0, count);

	Collection_freeEntries(entries, count);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_Collection_parse_realistic_favorites(void) {
	mock_fs_add_file("/mnt/SDCARD/Collections/Favorites.txt",
	                 "/Roms/GB/Super Mario Land (World).gb\n"
	                 "/Roms/NES/Super Mario Bros (World).nes\n"
	                 "/Roms/SNES/Super Metroid (USA, Europe).smc\n"
	                 "/Roms/GBA/Pokemon - Emerald (USA).gba\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/GB/Super Mario Land (World).gb", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/NES/Super Mario Bros (World).nes", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/SNES/Super Metroid (USA, Europe).smc", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/GBA/Pokemon - Emerald (USA).gba", "rom");

	int count;
	Collection_Entry** entries =
	    Collection_parse("/mnt/SDCARD/Collections/Favorites.txt", "/mnt/SDCARD", &count);

	TEST_ASSERT_EQUAL_INT(4, count);

	// Verify paths
	TEST_ASSERT_TRUE(strstr(entries[0]->path, "Super Mario Land") != NULL);
	TEST_ASSERT_TRUE(strstr(entries[1]->path, "Super Mario Bros") != NULL);
	TEST_ASSERT_TRUE(strstr(entries[2]->path, "Super Metroid") != NULL);
	TEST_ASSERT_TRUE(strstr(entries[3]->path, "Pokemon") != NULL);

	Collection_freeEntries(entries, count);
}

void test_Collection_parse_maintains_order(void) {
	mock_fs_add_file("/Collections/Ordered.txt",
	                 "/Roms/game3.rom\n"
	                 "/Roms/game1.rom\n"
	                 "/Roms/game2.rom\n");

	mock_fs_add_file("/mnt/SDCARD/Roms/game1.rom", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/game2.rom", "rom");
	mock_fs_add_file("/mnt/SDCARD/Roms/game3.rom", "rom");

	int count;
	Collection_Entry** entries = Collection_parse("/Collections/Ordered.txt", "/mnt/SDCARD", &count);

	// Order should match file order, not alphabetical
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/game3.rom", entries[0]->path);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/game1.rom", entries[1]->path);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/game2.rom", entries[2]->path);

	Collection_freeEntries(entries, count);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic parsing
	RUN_TEST(test_Collection_parse_single_rom);
	RUN_TEST(test_Collection_parse_multiple_roms);
	RUN_TEST(test_Collection_parse_skips_missing_roms);
	RUN_TEST(test_Collection_parse_skips_empty_lines);

	// PAK detection
	RUN_TEST(test_Collection_parse_detects_pak_files);
	RUN_TEST(test_Collection_parse_mixed_roms_and_paks);

	// Error cases
	RUN_TEST(test_Collection_parse_file_not_found_returns_zero);
	RUN_TEST(test_Collection_parse_empty_file_returns_zero);
	RUN_TEST(test_Collection_parse_all_roms_missing_returns_zero);

	// Integration
	RUN_TEST(test_Collection_parse_realistic_favorites);
	RUN_TEST(test_Collection_parse_maintains_order);

	return UNITY_END();
}
