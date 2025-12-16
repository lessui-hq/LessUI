/**
 * test_launcher_directory.c - Unit tests for Launcher directory building utilities
 *
 * Tests directory building functions extracted from launcher.c.
 * Uses real temp directories for tests requiring file system operations.
 *
 * Test coverage:
 * - LauncherDir_isConsoleDir() - Console directory detection
 * - LauncherDir_determineEntryType() - Entry type determination
 * - LauncherDir_buildCollationPrefix() - Collation prefix extraction
 * - LauncherDir_matchesCollation() - Collation matching
 * - LauncherDirScanResult operations - Scan result management
 * - LauncherDir_scan() - Directory scanning with real temp dirs
 */

#include "unity.h"
#include "launcher_directory.h"
#include "stb_ds.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// isConsoleDir() Tests
///////////////////////////////

void test_isConsoleDir_returns_true_for_console_dir(void) {
	int result = LauncherDir_isConsoleDir("/mnt/SDCARD/Roms/GB", "/mnt/SDCARD/Roms");
	TEST_ASSERT_TRUE(result);
}

void test_isConsoleDir_returns_true_for_console_dir_with_region(void) {
	int result = LauncherDir_isConsoleDir("/mnt/SDCARD/Roms/Game Boy (USA)", "/mnt/SDCARD/Roms");
	TEST_ASSERT_TRUE(result);
}

void test_isConsoleDir_returns_false_for_subdirectory(void) {
	int result = LauncherDir_isConsoleDir("/mnt/SDCARD/Roms/GB/subfolder", "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_returns_false_for_roms_itself(void) {
	int result = LauncherDir_isConsoleDir("/mnt/SDCARD/Roms", "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_returns_false_for_different_parent(void) {
	int result = LauncherDir_isConsoleDir("/mnt/SDCARD/Tools/Clock", "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_handles_null_path(void) {
	int result = LauncherDir_isConsoleDir(NULL, "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_handles_null_roms_path(void) {
	int result = LauncherDir_isConsoleDir("/mnt/SDCARD/Roms/GB", NULL);
	TEST_ASSERT_FALSE(result);
}

///////////////////////////////
// determineEntryType() Tests
///////////////////////////////

void test_determineEntryType_directory_returns_entry_dir(void) {
	int result =
	    LauncherDir_determineEntryType("games", 1, "/mnt/SDCARD/Roms/GB", "/mnt/SDCARD/.launcher/Collections");
	TEST_ASSERT_EQUAL(ENTRY_DIR, result);
}

void test_determineEntryType_pak_returns_entry_pak(void) {
	int result =
	    LauncherDir_determineEntryType("MyApp.pak", 1, "/mnt/SDCARD/Tools", "/mnt/SDCARD/.launcher/Collections");
	TEST_ASSERT_EQUAL(ENTRY_PAK, result);
}

void test_determineEntryType_rom_returns_entry_rom(void) {
	int result =
	    LauncherDir_determineEntryType("game.gba", 0, "/mnt/SDCARD/Roms/GBA", "/mnt/SDCARD/.launcher/Collections");
	TEST_ASSERT_EQUAL(ENTRY_ROM, result);
}

void test_determineEntryType_file_in_collections_returns_entry_dir(void) {
	// Collection entries (files like .txt) are treated as pseudo-directories
	int result = LauncherDir_determineEntryType("favorites.txt", 0, "/mnt/SDCARD/.launcher/Collections",
	                                         "/mnt/SDCARD/.launcher/Collections");
	TEST_ASSERT_EQUAL(ENTRY_DIR, result);
}

void test_determineEntryType_file_in_collection_subdir_returns_entry_dir(void) {
	int result = LauncherDir_determineEntryType("game.txt", 0, "/mnt/SDCARD/.launcher/Collections/RPGs",
	                                         "/mnt/SDCARD/.launcher/Collections");
	TEST_ASSERT_EQUAL(ENTRY_DIR, result);
}

void test_determineEntryType_pak_suffix_case_insensitive(void) {
	// .PAK (uppercase) is also recognized as pak - suffixMatch is case-insensitive
	int result =
	    LauncherDir_determineEntryType("MyApp.PAK", 1, "/mnt/SDCARD/Tools", "/mnt/SDCARD/.launcher/Collections");
	// suffixMatch is case-insensitive, so .PAK matches .pak
	TEST_ASSERT_EQUAL(ENTRY_PAK, result);
}

void test_determineEntryType_handles_null_filename(void) {
	int result = LauncherDir_determineEntryType(NULL, 0, "/path", "/collections");
	TEST_ASSERT_EQUAL(ENTRY_ROM, result); // Default fallback
}

///////////////////////////////
// buildCollationPrefix() Tests
///////////////////////////////

void test_buildCollationPrefix_extracts_prefix(void) {
	char prefix[LAUNCHER_DIR_MAX_PATH];
	int result = LauncherDir_buildCollationPrefix("/mnt/SDCARD/Roms/Game Boy (USA)", prefix);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/Game Boy (", prefix);
}

void test_buildCollationPrefix_keeps_opening_paren(void) {
	// Must keep "(" to avoid matching "Game Boy" with "Game Boy Advance"
	char prefix[LAUNCHER_DIR_MAX_PATH];
	LauncherDir_buildCollationPrefix("/Roms/GB (USA)", prefix);

	TEST_ASSERT_EQUAL_STRING("/Roms/GB (", prefix);
}

void test_buildCollationPrefix_returns_false_no_paren(void) {
	char prefix[LAUNCHER_DIR_MAX_PATH];
	int result = LauncherDir_buildCollationPrefix("/mnt/SDCARD/Roms/GB", prefix);

	TEST_ASSERT_FALSE(result);
	TEST_ASSERT_EQUAL_STRING("", prefix);
}

void test_buildCollationPrefix_handles_null(void) {
	char prefix[LAUNCHER_DIR_MAX_PATH];
	int result = LauncherDir_buildCollationPrefix(NULL, prefix);

	TEST_ASSERT_FALSE(result);
}

void test_buildCollationPrefix_uses_rightmost_paren(void) {
	// Multiple parens - should use the rightmost one
	char prefix[LAUNCHER_DIR_MAX_PATH];
	LauncherDir_buildCollationPrefix("/Roms/PS1 (Disc) (USA)", prefix);

	TEST_ASSERT_EQUAL_STRING("/Roms/PS1 (Disc) (", prefix);
}

///////////////////////////////
// matchesCollation() Tests
///////////////////////////////

void test_matchesCollation_matches_same_prefix(void) {
	const char* prefix = "/Roms/Game Boy (";

	TEST_ASSERT_TRUE(LauncherDir_matchesCollation("/Roms/Game Boy (USA)", prefix));
	TEST_ASSERT_TRUE(LauncherDir_matchesCollation("/Roms/Game Boy (Japan)", prefix));
	TEST_ASSERT_TRUE(LauncherDir_matchesCollation("/Roms/Game Boy (Europe)", prefix));
}

void test_matchesCollation_rejects_different_prefix(void) {
	const char* prefix = "/Roms/Game Boy (";

	TEST_ASSERT_FALSE(LauncherDir_matchesCollation("/Roms/Game Boy Advance (USA)", prefix));
	TEST_ASSERT_FALSE(LauncherDir_matchesCollation("/Roms/GBA (USA)", prefix));
}

void test_matchesCollation_handles_null(void) {
	TEST_ASSERT_FALSE(LauncherDir_matchesCollation(NULL, "/prefix"));
	TEST_ASSERT_FALSE(LauncherDir_matchesCollation("/path", NULL));
	TEST_ASSERT_FALSE(LauncherDir_matchesCollation("/path", ""));
}

///////////////////////////////
// ScanResult Tests
///////////////////////////////

void test_ScanResult_new_creates_valid_struct(void) {
	LauncherDirScanResult* result = LauncherDirScanResult_new(10);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_NOT_NULL(result->paths);
	TEST_ASSERT_NOT_NULL(result->is_dirs);
	TEST_ASSERT_EQUAL(0, result->count);
	TEST_ASSERT_EQUAL(10, result->capacity);

	LauncherDirScanResult_free(result);
}

void test_ScanResult_add_stores_entry(void) {
	LauncherDirScanResult* result = LauncherDirScanResult_new(10);

	int ok = LauncherDirScanResult_add(result, "/test/path", 1);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_EQUAL(1, result->count);
	TEST_ASSERT_EQUAL_STRING("/test/path", result->paths[0]);
	TEST_ASSERT_TRUE(result->is_dirs[0]);

	LauncherDirScanResult_free(result);
}

void test_ScanResult_add_grows_capacity(void) {
	LauncherDirScanResult* result = LauncherDirScanResult_new(2);

	LauncherDirScanResult_add(result, "/path1", 0);
	LauncherDirScanResult_add(result, "/path2", 1);
	LauncherDirScanResult_add(result, "/path3", 0); // Should trigger growth

	TEST_ASSERT_EQUAL(3, result->count);
	TEST_ASSERT_TRUE(result->capacity >= 3);
	TEST_ASSERT_EQUAL_STRING("/path3", result->paths[2]);

	LauncherDirScanResult_free(result);
}

void test_ScanResult_add_copies_path(void) {
	LauncherDirScanResult* result = LauncherDirScanResult_new(10);
	char path[] = "/mutable/path";

	LauncherDirScanResult_add(result, path, 0);

	// Modify original
	path[0] = 'X';

	// Stored copy should be unaffected
	TEST_ASSERT_EQUAL_STRING("/mutable/path", result->paths[0]);

	LauncherDirScanResult_free(result);
}

void test_ScanResult_free_handles_null(void) {
	// Should not crash
	LauncherDirScanResult_free(NULL);
}

///////////////////////////////
// scan() Tests (using real temp directories)
///////////////////////////////

void test_scan_returns_non_hidden_entries(void) {
	// Create temp directory
	char temp_dir[] = "/tmp/scantest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create visible file
	char visible[512];
	snprintf(visible, sizeof(visible), "%s/visible.txt", temp_dir);
	FILE* f = fopen(visible, "w");
	fputs("content", f);
	fclose(f);

	// Create hidden file
	char hidden[512];
	snprintf(hidden, sizeof(hidden), "%s/.hidden", temp_dir);
	f = fopen(hidden, "w");
	fputs("hidden", f);
	fclose(f);

	LauncherDirScanResult* result = LauncherDir_scan(temp_dir);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL(1, result->count);

	// Find the visible file
	int found = 0;
	for (int i = 0; i < result->count; i++) {
		if (strstr(result->paths[i], "visible.txt")) {
			found = 1;
		}
	}
	TEST_ASSERT_TRUE(found);

	LauncherDirScanResult_free(result);

	// Cleanup
	unlink(visible);
	unlink(hidden);
	rmdir(temp_dir);
}

void test_scan_detects_directories(void) {
	char temp_dir[] = "/tmp/scantest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create subdirectory
	char subdir[512];
	snprintf(subdir, sizeof(subdir), "%s/subdir", temp_dir);
	mkdir(subdir, 0755);

	// Create file
	char file[512];
	snprintf(file, sizeof(file), "%s/file.txt", temp_dir);
	FILE* f = fopen(file, "w");
	fputs("content", f);
	fclose(f);

	LauncherDirScanResult* result = LauncherDir_scan(temp_dir);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL(2, result->count);

	// Find each entry (order may vary)
	int found_dir = 0, found_file = 0;
	for (int i = 0; i < result->count; i++) {
		if (strstr(result->paths[i], "subdir")) {
			TEST_ASSERT_TRUE(result->is_dirs[i]);
			found_dir = 1;
		} else if (strstr(result->paths[i], "file.txt")) {
			TEST_ASSERT_FALSE(result->is_dirs[i]);
			found_file = 1;
		}
	}
	TEST_ASSERT_TRUE(found_dir);
	TEST_ASSERT_TRUE(found_file);

	LauncherDirScanResult_free(result);

	// Cleanup
	unlink(file);
	rmdir(subdir);
	rmdir(temp_dir);
}

void test_scan_returns_null_for_nonexistent_dir(void) {
	LauncherDirScanResult* result = LauncherDir_scan("/nonexistent/path/that/does/not/exist");
	TEST_ASSERT_NULL(result);
}

void test_scan_handles_null_path(void) {
	LauncherDirScanResult* result = LauncherDir_scan(NULL);
	TEST_ASSERT_NULL(result);
}

void test_scan_empty_directory(void) {
	char temp_dir[] = "/tmp/scantest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	LauncherDirScanResult* result = LauncherDir_scan(temp_dir);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL(0, result->count);

	LauncherDirScanResult_free(result);
	rmdir(temp_dir);
}

///////////////////////////////
// scanCollated() Tests
///////////////////////////////

void test_scanCollated_finds_matching_region_dirs(void) {
	// Create temp Roms directory
	char temp_dir[] = "/tmp/collated_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create two Game Boy region directories
	char gb_usa[512], gb_japan[512];
	snprintf(gb_usa, sizeof(gb_usa), "%s/Game Boy (USA)", temp_dir);
	snprintf(gb_japan, sizeof(gb_japan), "%s/Game Boy (Japan)", temp_dir);
	mkdir(gb_usa, 0755);
	mkdir(gb_japan, 0755);

	// Add ROM files to each
	char rom1[512], rom2[512];
	snprintf(rom1, sizeof(rom1), "%s/tetris.gb", gb_usa);
	snprintf(rom2, sizeof(rom2), "%s/mario.gb", gb_japan);
	FILE* f = fopen(rom1, "w");
	fputs("rom", f);
	fclose(f);
	f = fopen(rom2, "w");
	fputs("rom", f);
	fclose(f);

	// Build collation prefix
	char prefix[LAUNCHER_DIR_MAX_PATH];
	LauncherDir_buildCollationPrefix(gb_usa, prefix);

	// Scan with collation
	LauncherDirScanResult* result = LauncherDir_scanCollated(temp_dir, prefix);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL(2, result->count); // Should find both ROMs

	// Verify both ROMs are found
	int found_tetris = 0, found_mario = 0;
	for (int i = 0; i < result->count; i++) {
		if (strstr(result->paths[i], "tetris.gb"))
			found_tetris = 1;
		if (strstr(result->paths[i], "mario.gb"))
			found_mario = 1;
	}
	TEST_ASSERT_TRUE(found_tetris);
	TEST_ASSERT_TRUE(found_mario);

	LauncherDirScanResult_free(result);

	// Cleanup
	unlink(rom1);
	unlink(rom2);
	rmdir(gb_usa);
	rmdir(gb_japan);
	rmdir(temp_dir);
}

void test_scanCollated_excludes_non_matching_dirs(void) {
	char temp_dir[] = "/tmp/collated_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create Game Boy and Game Boy Advance directories
	char gb_usa[512], gba_usa[512];
	snprintf(gb_usa, sizeof(gb_usa), "%s/Game Boy (USA)", temp_dir);
	snprintf(gba_usa, sizeof(gba_usa), "%s/Game Boy Advance (USA)", temp_dir);
	mkdir(gb_usa, 0755);
	mkdir(gba_usa, 0755);

	// Add ROMs
	char rom1[512], rom2[512];
	snprintf(rom1, sizeof(rom1), "%s/tetris.gb", gb_usa);
	snprintf(rom2, sizeof(rom2), "%s/pokemon.gba", gba_usa);
	FILE* f = fopen(rom1, "w");
	fputs("rom", f);
	fclose(f);
	f = fopen(rom2, "w");
	fputs("rom", f);
	fclose(f);

	// Build prefix for Game Boy (should NOT match Game Boy Advance)
	char prefix[LAUNCHER_DIR_MAX_PATH];
	LauncherDir_buildCollationPrefix(gb_usa, prefix);

	LauncherDirScanResult* result = LauncherDir_scanCollated(temp_dir, prefix);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL(1, result->count); // Only Game Boy ROM

	// Should find tetris but not pokemon
	int found_tetris = 0, found_pokemon = 0;
	for (int i = 0; i < result->count; i++) {
		if (strstr(result->paths[i], "tetris.gb"))
			found_tetris = 1;
		if (strstr(result->paths[i], "pokemon.gba"))
			found_pokemon = 1;
	}
	TEST_ASSERT_TRUE(found_tetris);
	TEST_ASSERT_FALSE(found_pokemon);

	LauncherDirScanResult_free(result);

	// Cleanup
	unlink(rom1);
	unlink(rom2);
	rmdir(gb_usa);
	rmdir(gba_usa);
	rmdir(temp_dir);
}

void test_scanCollated_returns_null_for_null_inputs(void) {
	TEST_ASSERT_NULL(LauncherDir_scanCollated(NULL, "/prefix("));
	TEST_ASSERT_NULL(LauncherDir_scanCollated("/path", NULL));
	TEST_ASSERT_NULL(LauncherDir_scanCollated("/path", ""));
}

void test_scanCollated_returns_null_for_nonexistent_dir(void) {
	LauncherDirScanResult* result = LauncherDir_scanCollated("/nonexistent/path", "/nonexistent/path/Game (");
	TEST_ASSERT_NULL(result);
}

void test_scanCollated_returns_empty_when_no_matches(void) {
	char temp_dir[] = "/tmp/collated_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create a directory that won't match our prefix
	char other_dir[512];
	snprintf(other_dir, sizeof(other_dir), "%s/NES", temp_dir);
	mkdir(other_dir, 0755);

	// Look for Game Boy (won't find any)
	char prefix[LAUNCHER_DIR_MAX_PATH];
	snprintf(prefix, sizeof(prefix), "%s/Game Boy (", temp_dir);

	LauncherDirScanResult* result = LauncherDir_scanCollated(temp_dir, prefix);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL(0, result->count);

	LauncherDirScanResult_free(result);

	rmdir(other_dir);
	rmdir(temp_dir);
}

///////////////////////////////
// Directory_free Tests
///////////////////////////////

void test_Directory_free_handles_null(void) {
	// Should not crash
	Directory_free(NULL);
}

void test_Directory_free_frees_all_fields(void) {
	// Create a minimal Directory for testing
	Directory* dir = malloc(sizeof(Directory));
	TEST_ASSERT_NOT_NULL(dir);

	dir->path = strdup("/test/path");
	dir->name = strdup("TestDir");
	dir->entries = NULL; // stb_ds dynamic array
	dir->alphas = IntArray_new();
	dir->selected = 0;
	dir->start = 0;
	dir->end = 0;

	// Add an entry to entries array
	Entry* entry = Entry_new("/test/path/game.gb", ENTRY_ROM);
	arrpush(dir->entries, entry);

	// Add an alpha index
	IntArray_push(dir->alphas, 0);

	// Should free all allocated memory without crashing
	Directory_free(dir);
}

///////////////////////////////
// DirectoryArray Tests
///////////////////////////////

void test_DirectoryArray_pop_removes_and_frees(void) {
	Directory** arr = NULL;

	// Create two directories
	Directory* dir1 = malloc(sizeof(Directory));
	dir1->path = strdup("/path1");
	dir1->name = strdup("Dir1");
	dir1->entries = NULL;
	dir1->alphas = IntArray_new();

	Directory* dir2 = malloc(sizeof(Directory));
	dir2->path = strdup("/path2");
	dir2->name = strdup("Dir2");
	dir2->entries = NULL;
	dir2->alphas = IntArray_new();

	arrpush(arr, dir1);
	arrpush(arr, dir2);

	TEST_ASSERT_EQUAL(2, arrlen(arr));

	// Pop should remove and free the last directory
	DirectoryArray_pop(arr);
	TEST_ASSERT_EQUAL(1, arrlen(arr));

	DirectoryArray_pop(arr);
	TEST_ASSERT_EQUAL(0, arrlen(arr));

	arrfree(arr);
}

void test_DirectoryArray_pop_handles_null(void) {
	// Should not crash
	DirectoryArray_pop(NULL);
}

void test_DirectoryArray_free_frees_all_directories(void) {
	Directory** arr = NULL;

	// Create and add directories
	for (int i = 0; i < 3; i++) {
		Directory* dir = malloc(sizeof(Directory));
		char path[64], name[64];
		snprintf(path, sizeof(path), "/path%d", i);
		snprintf(name, sizeof(name), "Dir%d", i);
		dir->path = strdup(path);
		dir->name = strdup(name);
		dir->entries = NULL;
		dir->alphas = IntArray_new();
		arrpush(arr, dir);
	}

	TEST_ASSERT_EQUAL(3, arrlen(arr));

	// Should free all directories and the array
	DirectoryArray_free(arr);
}

void test_DirectoryArray_free_handles_null(void) {
	// Should not crash
	DirectoryArray_free(NULL);
}

///////////////////////////////
// ScanResult capacity growth edge cases
///////////////////////////////

void test_ScanResult_new_uses_default_capacity_for_zero(void) {
	LauncherDirScanResult* result = LauncherDirScanResult_new(0);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_GREATER_THAN(0, result->capacity); // Has some default when <= 0

	LauncherDirScanResult_free(result);
}

void test_ScanResult_new_uses_default_capacity_for_negative(void) {
	LauncherDirScanResult* result = LauncherDirScanResult_new(-5);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_GREATER_THAN(0, result->capacity); // Has some default when <= 0

	LauncherDirScanResult_free(result);
}

void test_ScanResult_add_handles_null_result(void) {
	int ok = LauncherDirScanResult_add(NULL, "/path", 0);
	TEST_ASSERT_FALSE(ok);
}

void test_ScanResult_add_handles_null_path(void) {
	LauncherDirScanResult* result = LauncherDirScanResult_new(10);

	int ok = LauncherDirScanResult_add(result, NULL, 0);
	TEST_ASSERT_FALSE(ok);
	TEST_ASSERT_EQUAL(0, result->count); // Nothing added

	LauncherDirScanResult_free(result);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// isConsoleDir tests
	RUN_TEST(test_isConsoleDir_returns_true_for_console_dir);
	RUN_TEST(test_isConsoleDir_returns_true_for_console_dir_with_region);
	RUN_TEST(test_isConsoleDir_returns_false_for_subdirectory);
	RUN_TEST(test_isConsoleDir_returns_false_for_roms_itself);
	RUN_TEST(test_isConsoleDir_returns_false_for_different_parent);
	RUN_TEST(test_isConsoleDir_handles_null_path);
	RUN_TEST(test_isConsoleDir_handles_null_roms_path);

	// determineEntryType tests
	RUN_TEST(test_determineEntryType_directory_returns_entry_dir);
	RUN_TEST(test_determineEntryType_pak_returns_entry_pak);
	RUN_TEST(test_determineEntryType_rom_returns_entry_rom);
	RUN_TEST(test_determineEntryType_file_in_collections_returns_entry_dir);
	RUN_TEST(test_determineEntryType_file_in_collection_subdir_returns_entry_dir);
	RUN_TEST(test_determineEntryType_pak_suffix_case_insensitive);
	RUN_TEST(test_determineEntryType_handles_null_filename);

	// buildCollationPrefix tests
	RUN_TEST(test_buildCollationPrefix_extracts_prefix);
	RUN_TEST(test_buildCollationPrefix_keeps_opening_paren);
	RUN_TEST(test_buildCollationPrefix_returns_false_no_paren);
	RUN_TEST(test_buildCollationPrefix_handles_null);
	RUN_TEST(test_buildCollationPrefix_uses_rightmost_paren);

	// matchesCollation tests
	RUN_TEST(test_matchesCollation_matches_same_prefix);
	RUN_TEST(test_matchesCollation_rejects_different_prefix);
	RUN_TEST(test_matchesCollation_handles_null);

	// ScanResult tests
	RUN_TEST(test_ScanResult_new_creates_valid_struct);
	RUN_TEST(test_ScanResult_add_stores_entry);
	RUN_TEST(test_ScanResult_add_grows_capacity);
	RUN_TEST(test_ScanResult_add_copies_path);
	RUN_TEST(test_ScanResult_free_handles_null);

	// scan tests (using real temp dirs)
	RUN_TEST(test_scan_returns_non_hidden_entries);
	RUN_TEST(test_scan_detects_directories);
	RUN_TEST(test_scan_returns_null_for_nonexistent_dir);
	RUN_TEST(test_scan_handles_null_path);
	RUN_TEST(test_scan_empty_directory);

	// scanCollated tests (using real temp dirs)
	RUN_TEST(test_scanCollated_finds_matching_region_dirs);
	RUN_TEST(test_scanCollated_excludes_non_matching_dirs);
	RUN_TEST(test_scanCollated_returns_null_for_null_inputs);
	RUN_TEST(test_scanCollated_returns_null_for_nonexistent_dir);
	RUN_TEST(test_scanCollated_returns_empty_when_no_matches);

	// Directory_free tests
	RUN_TEST(test_Directory_free_handles_null);
	RUN_TEST(test_Directory_free_frees_all_fields);

	// DirectoryArray tests
	RUN_TEST(test_DirectoryArray_pop_removes_and_frees);
	RUN_TEST(test_DirectoryArray_pop_handles_null);
	RUN_TEST(test_DirectoryArray_free_frees_all_directories);
	RUN_TEST(test_DirectoryArray_free_handles_null);

	// ScanResult edge cases
	RUN_TEST(test_ScanResult_new_uses_default_capacity_for_zero);
	RUN_TEST(test_ScanResult_new_uses_default_capacity_for_negative);
	RUN_TEST(test_ScanResult_add_handles_null_result);
	RUN_TEST(test_ScanResult_add_handles_null_path);

	return UNITY_END();
}
