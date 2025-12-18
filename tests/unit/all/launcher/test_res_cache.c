/**
 * Test suite for workspace/all/launcher/launcher_res_cache.c
 *
 * Tests the thumbnail (.res) directory cache that eliminates repeated
 * filesystem checks during ROM browsing.
 */

#include "launcher_res_cache.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Test fixture paths
static char test_dir[512];
static char roms_path[512];

void setUp(void) {
	// Create unique temp directory for each test
	snprintf(test_dir, sizeof(test_dir), "/tmp/res_cache_test_XXXXXX");
	char* result = mkdtemp(test_dir);
	TEST_ASSERT_NOT_NULL_MESSAGE(result, "Failed to create temp directory");

	// Set up paths
	snprintf(roms_path, sizeof(roms_path), "%s/Roms", test_dir);

	// Initialize cache
	ResCache_init();
}

void tearDown(void) {
	// Clean up cache
	ResCache_free();

	// Remove test directory and contents
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
	int ret = system(cmd);
	(void)ret;
}

// Helper to create a ROM directory with optional thumbnails
static void createRomDir(const char* console, const char** rom_names, int rom_count,
                         const char** thumb_names, int thumb_count) {
	char rom_dir[512];
	char res_dir[512];
	char mkdir_cmd[1024];
	int ret;

	// Create ROM directory
	snprintf(rom_dir, sizeof(rom_dir), "%s/%s", roms_path, console);
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", rom_dir);
	ret = system(mkdir_cmd);
	(void)ret;

	// Create ROM files (empty files)
	for (int i = 0; i < rom_count; i++) {
		char rom_path[512];
		snprintf(rom_path, sizeof(rom_path), "%s/%s", rom_dir, rom_names[i]);
		FILE* f = fopen(rom_path, "w");
		if (f)
			fclose(f);
	}

	// Create .res directory with thumbnails
	if (thumb_count > 0) {
		snprintf(res_dir, sizeof(res_dir), "%s/.res", rom_dir);
		snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", res_dir);
		ret = system(mkdir_cmd);
		(void)ret;

		for (int i = 0; i < thumb_count; i++) {
			char thumb_path[512];
			snprintf(thumb_path, sizeof(thumb_path), "%s/%s", res_dir, thumb_names[i]);
			FILE* f = fopen(thumb_path, "w");
			if (f) {
				// Write minimal PNG header (not a real PNG, but has .png extension)
				fprintf(f, "FAKE_PNG");
				fclose(f);
			}
		}
	}
}

///////////////////////////////
// Basic functionality tests
///////////////////////////////

void test_cache_not_initialized(void) {
	ResCache_free(); // Ensure not initialized
	// Should return 0 for any path
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail("/Roms/GB/Tetris.gb"));
	TEST_ASSERT_EQUAL_INT(0, ResCache_dirCount());
}

void test_cache_init_empty(void) {
	// After init with no directories, should have 0 cached dirs
	TEST_ASSERT_EQUAL_INT(0, ResCache_dirCount());
}

void test_has_thumbnail_returns_true_when_exists(void) {
	const char* roms[] = {"Tetris.gb", "Zelda.gb"};
	const char* thumbs[] = {"Tetris.gb.png"};
	createRomDir("GB", roms, 2, thumbs, 1);

	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Tetris.gb", roms_path);

	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount()); // Dir was scanned and cached
}

void test_has_thumbnail_returns_false_when_not_exists(void) {
	const char* roms[] = {"Tetris.gb", "Zelda.gb"};
	const char* thumbs[] = {"Tetris.gb.png"};
	createRomDir("GB", roms, 2, thumbs, 1);

	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Zelda.gb", roms_path);

	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount()); // Dir was still scanned
}

void test_has_thumbnail_no_res_directory(void) {
	const char* roms[] = {"Tetris.gb"};
	createRomDir("GB", roms, 1, NULL, 0);

	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Tetris.gb", roms_path);

	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount()); // Cached as "no thumbnails"
}

void test_get_thumb_path_returns_path_when_exists(void) {
	const char* roms[] = {"Tetris.gb"};
	const char* thumbs[] = {"Tetris.gb.png"};
	createRomDir("GB", roms, 1, thumbs, 1);

	char entry_path[512];
	char thumb_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Tetris.gb", roms_path);

	int result = ResCache_getThumbPath(entry_path, thumb_path);
	TEST_ASSERT_EQUAL_INT(1, result);

	// Verify the path is correct
	char expected[512];
	snprintf(expected, sizeof(expected), "%s/GB/.res/Tetris.gb.png", roms_path);
	TEST_ASSERT_EQUAL_STRING(expected, thumb_path);
}

void test_get_thumb_path_returns_zero_when_not_exists(void) {
	const char* roms[] = {"Tetris.gb"};
	createRomDir("GB", roms, 1, NULL, 0);

	char entry_path[512];
	char thumb_path[512];
	thumb_path[0] = 'X'; // Set to detect if it's cleared
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Tetris.gb", roms_path);

	int result = ResCache_getThumbPath(entry_path, thumb_path);
	TEST_ASSERT_EQUAL_INT(0, result);
	TEST_ASSERT_EQUAL_CHAR('\0', thumb_path[0]); // Path should be cleared
}

///////////////////////////////
// Caching behavior tests
///////////////////////////////

void test_directory_scanned_once(void) {
	const char* roms[] = {"Game1.gb", "Game2.gb", "Game3.gb"};
	const char* thumbs[] = {"Game1.gb.png", "Game2.gb.png"};
	createRomDir("GB", roms, 3, thumbs, 2);

	char path1[512], path2[512], path3[512];
	snprintf(path1, sizeof(path1), "%s/GB/Game1.gb", roms_path);
	snprintf(path2, sizeof(path2), "%s/GB/Game2.gb", roms_path);
	snprintf(path3, sizeof(path3), "%s/GB/Game3.gb", roms_path);

	// First call scans directory
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(path1));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());

	// Subsequent calls use cache (dir count shouldn't increase)
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(path2));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());

	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(path3));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());
}

void test_multiple_directories_cached(void) {
	const char* gb_roms[] = {"Tetris.gb"};
	const char* gb_thumbs[] = {"Tetris.gb.png"};
	createRomDir("GB", gb_roms, 1, gb_thumbs, 1);

	const char* gba_roms[] = {"Mario.gba"};
	const char* gba_thumbs[] = {"Mario.gba.png"};
	createRomDir("GBA", gba_roms, 1, gba_thumbs, 1);

	char gb_path[512], gba_path[512];
	snprintf(gb_path, sizeof(gb_path), "%s/GB/Tetris.gb", roms_path);
	snprintf(gba_path, sizeof(gba_path), "%s/GBA/Mario.gba", roms_path);

	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(gb_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());

	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(gba_path));
	TEST_ASSERT_EQUAL_INT(2, ResCache_dirCount()); // Now 2 directories cached
}

///////////////////////////////
// Collections support tests
///////////////////////////////

void test_directory_entry_thumbnail(void) {
	// Test thumbnails for directory entries (e.g., console folders)
	// Entry: /Roms/GB (a directory)
	// Thumbnail: /Roms/.res/GB.png

	// Create the Roms/.res directory with console thumbnails
	char res_dir[512];
	char mkdir_cmd[1024];
	snprintf(res_dir, sizeof(res_dir), "%s/.res", roms_path);
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", res_dir);
	int ret = system(mkdir_cmd);
	(void)ret;

	// Create GB.png thumbnail (for the GB directory)
	char thumb_path[512];
	snprintf(thumb_path, sizeof(thumb_path), "%s/GB.png", res_dir);
	FILE* f = fopen(thumb_path, "w");
	if (f) {
		fprintf(f, "FAKE_PNG");
		fclose(f);
	}

	// Also create the GB directory itself
	char gb_dir[512];
	snprintf(gb_dir, sizeof(gb_dir), "%s/GB", roms_path);
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", gb_dir);
	ret = system(mkdir_cmd);
	(void)ret;

	// Now check if the directory entry has a thumbnail
	// Entry path is /Roms/GB (no trailing slash)
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(gb_dir));

	// Verify the path is built correctly
	char result_path[512];
	TEST_ASSERT_EQUAL_INT(1, ResCache_getThumbPath(gb_dir, result_path));

	char expected[512];
	snprintf(expected, sizeof(expected), "%s/.res/GB.png", roms_path);
	TEST_ASSERT_EQUAL_STRING(expected, result_path);
}

void test_collection_entries_from_different_dirs(void) {
	// Simulate a collection with entries from different directories
	const char* gb_roms[] = {"Tetris.gb"};
	const char* gb_thumbs[] = {"Tetris.gb.png"};
	createRomDir("GB", gb_roms, 1, gb_thumbs, 1);

	const char* nes_roms[] = {"Mario.nes"};
	const char* nes_thumbs[] = {"Mario.nes.png"};
	createRomDir("NES", nes_roms, 1, nes_thumbs, 1);

	const char* snes_roms[] = {"Zelda.sfc"};
	// No thumbnails for SNES
	createRomDir("SNES", snes_roms, 1, NULL, 0);

	char gb_path[512], nes_path[512], snes_path[512];
	snprintf(gb_path, sizeof(gb_path), "%s/GB/Tetris.gb", roms_path);
	snprintf(nes_path, sizeof(nes_path), "%s/NES/Mario.nes", roms_path);
	snprintf(snes_path, sizeof(snes_path), "%s/SNES/Zelda.sfc", roms_path);

	// Check each - they should all work, each parent dir scanned once
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(gb_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(nes_path));
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(snes_path));

	TEST_ASSERT_EQUAL_INT(3, ResCache_dirCount()); // 3 directories cached
}

///////////////////////////////
// Edge cases
///////////////////////////////

void test_null_path(void) {
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(NULL));

	char thumb_path[512];
	TEST_ASSERT_EQUAL_INT(0, ResCache_getThumbPath(NULL, thumb_path));
}

void test_empty_path(void) {
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(""));

	char thumb_path[512];
	TEST_ASSERT_EQUAL_INT(0, ResCache_getThumbPath("", thumb_path));
}

void test_path_without_slash(void) {
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail("nopath.gb"));
}

void test_path_ending_with_slash(void) {
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail("/Roms/GB/"));
}

void test_root_level_path_supported(void) {
	// Root-level paths (like "/game.gb") are accepted and parsed correctly.
	// While not used in practice (all paths are under SDCARD_PATH),
	// we support it defensively.

	// The path "/game.gb" should be accepted (not rejected).
	// It will return 0 because /.res doesn't exist, but it should
	// attempt the lookup rather than rejecting the path format.

	// First verify it doesn't crash and returns 0 (no thumbnail, not path error)
	int result = ResCache_hasThumbnail("/game.gb");
	TEST_ASSERT_EQUAL_INT(0, result);

	// The directory "/" should have been scanned and cached (as empty/no thumbnails)
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());

	// Now test with a path we can actually create thumbnails for
	// Create /.res directory in our test environment
	char res_dir[512];
	char mkdir_cmd[1024];
	snprintf(res_dir, sizeof(res_dir), "%s/.res", test_dir);
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", res_dir);
	int ret = system(mkdir_cmd);
	(void)ret;

	// Create thumbnail
	char thumb_file[512];
	snprintf(thumb_file, sizeof(thumb_file), "%s/game.gb.png", res_dir);
	FILE* f = fopen(thumb_file, "w");
	if (f) {
		fprintf(f, "FAKE_PNG");
		fclose(f);
	}

	// Build path using test_dir
	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/game.gb", test_dir);

	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(2, ResCache_dirCount()); // "/" and test_dir

	// Verify correct thumb path is built
	char result_path[512];
	TEST_ASSERT_EQUAL_INT(1, ResCache_getThumbPath(entry_path, result_path));

	char expected[512];
	snprintf(expected, sizeof(expected), "%s/.res/game.gb.png", test_dir);
	TEST_ASSERT_EQUAL_STRING(expected, result_path);
}

void test_hidden_thumbnails_ignored(void) {
	const char* roms[] = {"Game.gb"};
	const char* thumbs[] = {".hidden.gb.png", "Game.gb.png"};
	createRomDir("GB", roms, 1, thumbs, 2);

	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Game.gb", roms_path);

	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(entry_path));

	// Hidden file should not be found
	char hidden_path[512];
	snprintf(hidden_path, sizeof(hidden_path), "%s/GB/.hidden.gb", roms_path);
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(hidden_path));
}

void test_non_png_files_ignored(void) {
	const char* roms[] = {"Game.gb"};
	const char* thumbs[] = {"Game.gb.jpg"}; // Wrong extension
	createRomDir("GB", roms, 1, thumbs, 1);

	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Game.gb", roms_path);

	// Should not find .jpg file
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(entry_path));
}

void test_invalidate_directory(void) {
	const char* roms[] = {"Game.gb"};
	const char* thumbs[] = {"Game.gb.png"};
	createRomDir("GB", roms, 1, thumbs, 1);

	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Game.gb", roms_path);

	// First check caches the directory
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());

	// Invalidate
	char dir_path[512];
	snprintf(dir_path, sizeof(dir_path), "%s/GB", roms_path);
	ResCache_invalidateDir(dir_path);
	TEST_ASSERT_EQUAL_INT(0, ResCache_dirCount());

	// Next check re-scans
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());
}

void test_free_safe_multiple_calls(void) {
	// Should be safe to call free multiple times
	ResCache_free();
	ResCache_free();
	ResCache_free();

	// Should also work before init
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail("/test/path.gb"));
}

void test_reinit(void) {
	const char* roms[] = {"Game.gb"};
	const char* thumbs[] = {"Game.gb.png"};
	createRomDir("GB", roms, 1, thumbs, 1);

	char entry_path[512];
	snprintf(entry_path, sizeof(entry_path), "%s/GB/Game.gb", roms_path);

	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());

	// Reinit clears cache
	ResCache_init();
	TEST_ASSERT_EQUAL_INT(0, ResCache_dirCount());

	// Should work again after reinit
	TEST_ASSERT_EQUAL_INT(1, ResCache_hasThumbnail(entry_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount());
}

void test_path_too_long_rejected(void) {
	// Build a path that would overflow when constructing thumbnail path
	// Full thumb path = dir + "/.res/" + filename + ".png" = dir + filename + 11
	// If dir + filename + 11 > MAX_PATH (512), it should be rejected

	// Create a path with combined length that would overflow
	char long_path[600];
	memset(long_path, 'a', sizeof(long_path) - 1);
	long_path[0] = '/';
	long_path[300] = '/'; // Split into dir (300) and filename (299)
	long_path[599] = '\0';

	// dir_len=300, filename_len=298, total path overhead=11
	// 300 + 298 + 11 = 609 > 512, should be rejected
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(long_path));
	TEST_ASSERT_EQUAL_INT(0, ResCache_dirCount()); // Path rejected, nothing cached

	char thumb_path[512];
	TEST_ASSERT_EQUAL_INT(0, ResCache_getThumbPath(long_path, thumb_path));
}

void test_path_at_max_boundary_accepted(void) {
	// Test boundary: dir_len + filename_len + 11 = 512 (should pass)
	// Condition is: total > MAX_PATH fails, so total = 512 should pass
	// Need: dir_len + filename_len = 501

	// Build path: /aaa...aaa/aaa...aaa where dir_len=250, filename_len=251
	// Total: 250 + 251 + 11 = 512, should pass (not > 512)
	char boundary_path[503];
	memset(boundary_path, 'a', 502);
	boundary_path[0] = '/';
	boundary_path[250] = '/';
	boundary_path[502] = '\0';

	// dir_len=250, filename_len=251, 250+251+11=512 <= 512
	// Will return 0 because directory doesn't exist, but should be cached
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(boundary_path));
	TEST_ASSERT_EQUAL_INT(1, ResCache_dirCount()); // Path accepted, dir cached
}

void test_path_over_max_boundary_rejected(void) {
	// Test boundary: dir_len + filename_len + 11 = 513 (should fail)
	// Condition is: total > MAX_PATH fails
	// Need: dir_len + filename_len = 502

	// Build path: /aaa...aaa/aaa...aaa where dir_len=251, filename_len=251
	// Total: 251 + 251 + 11 = 513 > 512, should fail
	char boundary_path[504];
	memset(boundary_path, 'a', 503);
	boundary_path[0] = '/';
	boundary_path[251] = '/';
	boundary_path[503] = '\0';

	// dir_len=251, filename_len=251, 251+251+11=513 > 512, should be rejected
	TEST_ASSERT_EQUAL_INT(0, ResCache_hasThumbnail(boundary_path));
	TEST_ASSERT_EQUAL_INT(0, ResCache_dirCount()); // Path rejected, nothing cached
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic functionality
	RUN_TEST(test_cache_not_initialized);
	RUN_TEST(test_cache_init_empty);
	RUN_TEST(test_has_thumbnail_returns_true_when_exists);
	RUN_TEST(test_has_thumbnail_returns_false_when_not_exists);
	RUN_TEST(test_has_thumbnail_no_res_directory);
	RUN_TEST(test_get_thumb_path_returns_path_when_exists);
	RUN_TEST(test_get_thumb_path_returns_zero_when_not_exists);

	// Caching behavior
	RUN_TEST(test_directory_scanned_once);
	RUN_TEST(test_multiple_directories_cached);

	// Collections support
	RUN_TEST(test_directory_entry_thumbnail);
	RUN_TEST(test_collection_entries_from_different_dirs);

	// Edge cases
	RUN_TEST(test_null_path);
	RUN_TEST(test_empty_path);
	RUN_TEST(test_path_without_slash);
	RUN_TEST(test_path_ending_with_slash);
	RUN_TEST(test_root_level_path_supported);
	RUN_TEST(test_hidden_thumbnails_ignored);
	RUN_TEST(test_non_png_files_ignored);
	RUN_TEST(test_invalidate_directory);
	RUN_TEST(test_free_safe_multiple_calls);
	RUN_TEST(test_reinit);
	RUN_TEST(test_path_too_long_rejected);
	RUN_TEST(test_path_at_max_boundary_accepted);
	RUN_TEST(test_path_over_max_boundary_rejected);

	return UNITY_END();
}
