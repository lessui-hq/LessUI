/**
 * test_minui_directory.c - Unit tests for MinUI directory building utilities
 *
 * Tests directory building functions extracted from minui.c.
 * Uses real temp directories for tests requiring file system operations.
 *
 * Test coverage:
 * - MinUIDir_isConsoleDir() - Console directory detection
 * - MinUIDir_determineEntryType() - Entry type determination
 * - MinUIDir_buildCollationPrefix() - Collation prefix extraction
 * - MinUIDir_matchesCollation() - Collation matching
 * - MinUIDirScanResult operations - Scan result management
 * - MinUIDir_scan() - Directory scanning with real temp dirs
 */

#include "../../../support/unity/unity.h"
#include "minui_directory.h"

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
	int result = MinUIDir_isConsoleDir("/mnt/SDCARD/Roms/GB", "/mnt/SDCARD/Roms");
	TEST_ASSERT_TRUE(result);
}

void test_isConsoleDir_returns_true_for_console_dir_with_region(void) {
	int result = MinUIDir_isConsoleDir("/mnt/SDCARD/Roms/Game Boy (USA)", "/mnt/SDCARD/Roms");
	TEST_ASSERT_TRUE(result);
}

void test_isConsoleDir_returns_false_for_subdirectory(void) {
	int result = MinUIDir_isConsoleDir("/mnt/SDCARD/Roms/GB/subfolder", "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_returns_false_for_roms_itself(void) {
	int result = MinUIDir_isConsoleDir("/mnt/SDCARD/Roms", "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_returns_false_for_different_parent(void) {
	int result = MinUIDir_isConsoleDir("/mnt/SDCARD/Tools/Clock", "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_handles_null_path(void) {
	int result = MinUIDir_isConsoleDir(NULL, "/mnt/SDCARD/Roms");
	TEST_ASSERT_FALSE(result);
}

void test_isConsoleDir_handles_null_roms_path(void) {
	int result = MinUIDir_isConsoleDir("/mnt/SDCARD/Roms/GB", NULL);
	TEST_ASSERT_FALSE(result);
}

///////////////////////////////
// determineEntryType() Tests
///////////////////////////////

void test_determineEntryType_directory_returns_entry_dir(void) {
	int result =
	    MinUIDir_determineEntryType("games", 1, "/mnt/SDCARD/Roms/GB", "/mnt/SDCARD/.minui/Collections");
	TEST_ASSERT_EQUAL(ENTRY_DIR, result);
}

void test_determineEntryType_pak_returns_entry_pak(void) {
	int result =
	    MinUIDir_determineEntryType("MyApp.pak", 1, "/mnt/SDCARD/Tools", "/mnt/SDCARD/.minui/Collections");
	TEST_ASSERT_EQUAL(ENTRY_PAK, result);
}

void test_determineEntryType_rom_returns_entry_rom(void) {
	int result =
	    MinUIDir_determineEntryType("game.gba", 0, "/mnt/SDCARD/Roms/GBA", "/mnt/SDCARD/.minui/Collections");
	TEST_ASSERT_EQUAL(ENTRY_ROM, result);
}

void test_determineEntryType_file_in_collections_returns_entry_dir(void) {
	// Collection entries (files like .txt) are treated as pseudo-directories
	int result = MinUIDir_determineEntryType("favorites.txt", 0, "/mnt/SDCARD/.minui/Collections",
	                                         "/mnt/SDCARD/.minui/Collections");
	TEST_ASSERT_EQUAL(ENTRY_DIR, result);
}

void test_determineEntryType_file_in_collection_subdir_returns_entry_dir(void) {
	int result = MinUIDir_determineEntryType("game.txt", 0, "/mnt/SDCARD/.minui/Collections/RPGs",
	                                         "/mnt/SDCARD/.minui/Collections");
	TEST_ASSERT_EQUAL(ENTRY_DIR, result);
}

void test_determineEntryType_pak_suffix_case_insensitive(void) {
	// .PAK (uppercase) is also recognized as pak - suffixMatch is case-insensitive
	int result =
	    MinUIDir_determineEntryType("MyApp.PAK", 1, "/mnt/SDCARD/Tools", "/mnt/SDCARD/.minui/Collections");
	// suffixMatch is case-insensitive, so .PAK matches .pak
	TEST_ASSERT_EQUAL(ENTRY_PAK, result);
}

void test_determineEntryType_handles_null_filename(void) {
	int result = MinUIDir_determineEntryType(NULL, 0, "/path", "/collections");
	TEST_ASSERT_EQUAL(ENTRY_ROM, result); // Default fallback
}

///////////////////////////////
// buildCollationPrefix() Tests
///////////////////////////////

void test_buildCollationPrefix_extracts_prefix(void) {
	char prefix[MINUI_DIR_MAX_PATH];
	int result = MinUIDir_buildCollationPrefix("/mnt/SDCARD/Roms/Game Boy (USA)", prefix);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/Game Boy (", prefix);
}

void test_buildCollationPrefix_keeps_opening_paren(void) {
	// Must keep "(" to avoid matching "Game Boy" with "Game Boy Advance"
	char prefix[MINUI_DIR_MAX_PATH];
	MinUIDir_buildCollationPrefix("/Roms/GB (USA)", prefix);

	TEST_ASSERT_EQUAL_STRING("/Roms/GB (", prefix);
}

void test_buildCollationPrefix_returns_false_no_paren(void) {
	char prefix[MINUI_DIR_MAX_PATH];
	int result = MinUIDir_buildCollationPrefix("/mnt/SDCARD/Roms/GB", prefix);

	TEST_ASSERT_FALSE(result);
	TEST_ASSERT_EQUAL_STRING("", prefix);
}

void test_buildCollationPrefix_handles_null(void) {
	char prefix[MINUI_DIR_MAX_PATH];
	int result = MinUIDir_buildCollationPrefix(NULL, prefix);

	TEST_ASSERT_FALSE(result);
}

void test_buildCollationPrefix_uses_rightmost_paren(void) {
	// Multiple parens - should use the rightmost one
	char prefix[MINUI_DIR_MAX_PATH];
	MinUIDir_buildCollationPrefix("/Roms/PS1 (Disc) (USA)", prefix);

	TEST_ASSERT_EQUAL_STRING("/Roms/PS1 (Disc) (", prefix);
}

///////////////////////////////
// matchesCollation() Tests
///////////////////////////////

void test_matchesCollation_matches_same_prefix(void) {
	const char* prefix = "/Roms/Game Boy (";

	TEST_ASSERT_TRUE(MinUIDir_matchesCollation("/Roms/Game Boy (USA)", prefix));
	TEST_ASSERT_TRUE(MinUIDir_matchesCollation("/Roms/Game Boy (Japan)", prefix));
	TEST_ASSERT_TRUE(MinUIDir_matchesCollation("/Roms/Game Boy (Europe)", prefix));
}

void test_matchesCollation_rejects_different_prefix(void) {
	const char* prefix = "/Roms/Game Boy (";

	TEST_ASSERT_FALSE(MinUIDir_matchesCollation("/Roms/Game Boy Advance (USA)", prefix));
	TEST_ASSERT_FALSE(MinUIDir_matchesCollation("/Roms/GBA (USA)", prefix));
}

void test_matchesCollation_handles_null(void) {
	TEST_ASSERT_FALSE(MinUIDir_matchesCollation(NULL, "/prefix"));
	TEST_ASSERT_FALSE(MinUIDir_matchesCollation("/path", NULL));
	TEST_ASSERT_FALSE(MinUIDir_matchesCollation("/path", ""));
}

///////////////////////////////
// ScanResult Tests
///////////////////////////////

void test_ScanResult_new_creates_valid_struct(void) {
	MinUIDirScanResult* result = MinUIDirScanResult_new(10);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_NOT_NULL(result->paths);
	TEST_ASSERT_NOT_NULL(result->is_dirs);
	TEST_ASSERT_EQUAL(0, result->count);
	TEST_ASSERT_EQUAL(10, result->capacity);

	MinUIDirScanResult_free(result);
}

void test_ScanResult_add_stores_entry(void) {
	MinUIDirScanResult* result = MinUIDirScanResult_new(10);

	int ok = MinUIDirScanResult_add(result, "/test/path", 1);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_EQUAL(1, result->count);
	TEST_ASSERT_EQUAL_STRING("/test/path", result->paths[0]);
	TEST_ASSERT_TRUE(result->is_dirs[0]);

	MinUIDirScanResult_free(result);
}

void test_ScanResult_add_grows_capacity(void) {
	MinUIDirScanResult* result = MinUIDirScanResult_new(2);

	MinUIDirScanResult_add(result, "/path1", 0);
	MinUIDirScanResult_add(result, "/path2", 1);
	MinUIDirScanResult_add(result, "/path3", 0); // Should trigger growth

	TEST_ASSERT_EQUAL(3, result->count);
	TEST_ASSERT_TRUE(result->capacity >= 3);
	TEST_ASSERT_EQUAL_STRING("/path3", result->paths[2]);

	MinUIDirScanResult_free(result);
}

void test_ScanResult_add_copies_path(void) {
	MinUIDirScanResult* result = MinUIDirScanResult_new(10);
	char path[] = "/mutable/path";

	MinUIDirScanResult_add(result, path, 0);

	// Modify original
	path[0] = 'X';

	// Stored copy should be unaffected
	TEST_ASSERT_EQUAL_STRING("/mutable/path", result->paths[0]);

	MinUIDirScanResult_free(result);
}

void test_ScanResult_free_handles_null(void) {
	// Should not crash
	MinUIDirScanResult_free(NULL);
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

	MinUIDirScanResult* result = MinUIDir_scan(temp_dir);

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

	MinUIDirScanResult_free(result);

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

	MinUIDirScanResult* result = MinUIDir_scan(temp_dir);

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

	MinUIDirScanResult_free(result);

	// Cleanup
	unlink(file);
	rmdir(subdir);
	rmdir(temp_dir);
}

void test_scan_returns_null_for_nonexistent_dir(void) {
	MinUIDirScanResult* result = MinUIDir_scan("/nonexistent/path/that/does/not/exist");
	TEST_ASSERT_NULL(result);
}

void test_scan_handles_null_path(void) {
	MinUIDirScanResult* result = MinUIDir_scan(NULL);
	TEST_ASSERT_NULL(result);
}

void test_scan_empty_directory(void) {
	char temp_dir[] = "/tmp/scantest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	MinUIDirScanResult* result = MinUIDir_scan(temp_dir);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL(0, result->count);

	MinUIDirScanResult_free(result);
	rmdir(temp_dir);
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

	return UNITY_END();
}
