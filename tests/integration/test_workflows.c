/**
 * test_workflows.c - Integration tests for MinUI workflows
 *
 * Tests multiple components working together with real file I/O to verify
 * end-to-end functionality. Uses real temp directories and files instead of mocks.
 *
 * Test Scenarios:
 * 1. Multi-disc game workflow (M3U + Map + Recent integration)
 * 2. Collection with aliases (Collection + Map integration)
 * 3. Recent games round-trip (Recent parse + save)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../workspace/all/common/collection_parser.h"
#include "../../workspace/all/common/m3u_parser.h"
#include "../../workspace/all/common/map_parser.h"
#include "../../workspace/all/common/minui_file_utils.h"
#include "../../workspace/all/common/recent_file.h"
#include "../support/platform.h"
#include "../support/unity/unity.h"
#include "integration_support.h"

// Test directory (created in setUp, removed in tearDown)
static char test_dir[256];

void setUp(void) {
	// Create temp directory for each test
	strcpy(test_dir, "/tmp/minui_integration_XXXXXX");
	char* result = create_test_minui_structure(test_dir);
	TEST_ASSERT_NOT_NULL(result);
}

void tearDown(void) {
	// Clean up temp directory
	int removed = rmdir_recursive(test_dir);
	TEST_ASSERT_TRUE(removed);
}

///////////////////////////////
// Multi-Disc Game Workflow Tests
///////////////////////////////

/**
 * Integration test: Multi-disc game with M3U, map.txt, and recent.txt
 *
 * Workflow:
 * 1. Create multi-disc PS1 game (Final Fantasy VII)
 * 2. Parse M3U to get all discs
 * 3. Get display aliases from map.txt
 * 4. Save to recent games
 * 5. Load recent games and verify
 */
void test_multi_disc_game_complete_workflow(void) {
	char path[512];

	// Step 1: Create realistic multi-disc game structure
	// Create disc files
	const char* disc_files[] = {"FF7 (Disc 1).bin", "FF7 (Disc 2).bin",
	                            "FF7 (Disc 3).bin"};

	for (int i = 0; i < 3; i++) {
		snprintf(path, sizeof(path), "%s/Roms/PS1/%s", test_dir, disc_files[i]);
		TEST_ASSERT_TRUE(create_test_rom(path));
	}

	// Create M3U file
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF7.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, disc_files, 3));

	// Create map.txt with display names
	const char* rom_names[] = {"FF7 (Disc 1).bin", "FF7 (Disc 2).bin",
	                           "FF7 (Disc 3).bin"};
	const char* aliases[] = {"Final Fantasy VII - Disc 1",
	                         "Final Fantasy VII - Disc 2",
	                         "Final Fantasy VII - Disc 3"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, rom_names, aliases, 3));

	// Step 2: Parse M3U and get all discs
	char m3u_path[512];
	snprintf(m3u_path, sizeof(m3u_path), "%s/Roms/PS1/FF7.m3u", test_dir);

	int disc_count = 0;
	M3U_Disc** discs = M3U_getAllDiscs(m3u_path, &disc_count);
	TEST_ASSERT_NOT_NULL(discs);
	TEST_ASSERT_EQUAL(3, disc_count);

	// Verify disc paths
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF7 (Disc 1).bin", test_dir);
	TEST_ASSERT_EQUAL_STRING(path, discs[0]->path);
	TEST_ASSERT_EQUAL_STRING("Disc 1", discs[0]->name);
	TEST_ASSERT_EQUAL(1, discs[0]->disc_number);

	// Step 3: Get display aliases from map.txt
	char alias[256];
	Map_getAlias(discs[0]->path, alias);
	// Note: Map_getAlias modifies the alias buffer only if found,
	// so we need to initialize it first
	strcpy(alias, discs[0]->name); // Default to disc name
	Map_getAlias(discs[0]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Final Fantasy VII - Disc 1", alias);

	// Step 4: Save to recent games
	// NOTE: Recent_save expects relative paths (starting with /Roms...)
	Recent_Entry* entry = malloc(sizeof(Recent_Entry));
	entry->path = strdup("/Roms/PS1/FF7.m3u");  // Relative path
	entry->alias = strdup(alias);

	Recent_Entry** entries = malloc(sizeof(Recent_Entry*));
	entries[0] = entry;

	char recent_path[512];
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.minui/recent.txt",
	         test_dir);

	int saved = Recent_save(recent_path, entries, 1);
	TEST_ASSERT_TRUE(saved);

	// Step 5: Load recent games and verify integration
	int loaded_count = 0;
	Recent_Entry** loaded = Recent_parse(recent_path, test_dir, &loaded_count);
	TEST_ASSERT_NOT_NULL(loaded);
	TEST_ASSERT_EQUAL(1, loaded_count);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF7.m3u", loaded[0]->path);
	TEST_ASSERT_EQUAL_STRING(alias, loaded[0]->alias);

	// Cleanup
	M3U_freeDiscs(discs, disc_count);
	Recent_freeEntries(entries, 1);
	Recent_freeEntries(loaded, loaded_count);
}

/**
 * Integration test: M3U detection and file utilities
 *
 * Workflow:
 * 1. Create game with .m3u and .cue files
 * 2. Verify MinUI_hasM3u() detects M3U
 * 3. Verify MinUI_hasCue() detects CUE
 * 4. Test interaction between M3U and CUE detection
 */
void test_multi_disc_detection(void) {
	char path[512];

	// Create multi-disc game structure
	// MinUI_hasM3u expects: /Roms/PS1/Game/disc.bin and looks for /Roms/PS1/Game.m3u
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* disc_files[] = {"Game/disc1.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, disc_files, 1));

	// Create CUE file in game directory
	// MinUI_hasCue expects directory path and looks for dir/dirname.cue
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/Game.cue", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Test M3U detection
	char rom_path[512];
	snprintf(rom_path, sizeof(rom_path), "%s/Roms/PS1/Game/disc1.bin", test_dir);

	char m3u_path[512];
	int has_m3u = MinUI_hasM3u(rom_path, m3u_path);
	TEST_ASSERT_TRUE(has_m3u);
	TEST_ASSERT_EQUAL_STRING_LEN("/Roms/PS1/Game.m3u", m3u_path + strlen(test_dir),
	                             strlen("/Roms/PS1/Game.m3u"));

	// Test CUE detection (expects directory path, not ROM path)
	char dir_path[512];
	snprintf(dir_path, sizeof(dir_path), "%s/Roms/PS1/Game", test_dir);

	char cue_path[512];
	int has_cue = MinUI_hasCue(dir_path, cue_path);
	TEST_ASSERT_TRUE(has_cue);

	// Both should be detected
	TEST_ASSERT_TRUE(has_m3u && has_cue);
}

///////////////////////////////
// Collection Integration Tests
///////////////////////////////

/**
 * Integration test: Collection with custom ROM aliases
 *
 * Workflow:
 * 1. Create collection file with ROMs from multiple systems
 * 2. Create map.txt files with custom names
 * 3. Parse collection
 * 4. Verify aliases work across systems
 */
void test_collection_with_aliases(void) {
	char path[512];

	// Create ROMs across multiple systems
	snprintf(path, sizeof(path), "%s/Roms/GB/mario.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/NES/zelda.nes", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/SNES/metroid.smc", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Create map.txt files for each system
	const char* gb_names[] = {"mario.gb"};
	const char* gb_aliases[] = {"Super Mario Land"};
	snprintf(path, sizeof(path), "%s/Roms/GB/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, gb_names, gb_aliases, 1));

	const char* nes_names[] = {"zelda.nes"};
	const char* nes_aliases[] = {"The Legend of Zelda"};
	snprintf(path, sizeof(path), "%s/Roms/NES/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, nes_names, nes_aliases, 1));

	// Create collection file
	char mario_path[256], zelda_path[256], metroid_path[256];
	snprintf(mario_path, sizeof(mario_path), "/Roms/GB/mario.gb");
	snprintf(zelda_path, sizeof(zelda_path), "/Roms/NES/zelda.nes");
	snprintf(metroid_path, sizeof(metroid_path), "/Roms/SNES/metroid.smc");

	const char* collection_paths[] = {mario_path, zelda_path, metroid_path};
	snprintf(path, sizeof(path), "%s/Collections/Favorites.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_collection(path, collection_paths, 3));

	// Parse collection
	int entry_count = 0;
	Collection_Entry** entries = Collection_parse(path, test_dir, &entry_count);
	TEST_ASSERT_NOT_NULL(entries);
	TEST_ASSERT_EQUAL(3, entry_count);

	// Verify collection entries exist
	char expected_path[512];
	snprintf(expected_path, sizeof(expected_path), "%s/Roms/GB/mario.gb", test_dir);
	TEST_ASSERT_EQUAL_STRING(expected_path, entries[0]->path);

	// Now verify map aliases work for collection entries
	char alias[256];

	// Mario
	strcpy(alias, "");
	Map_getAlias(entries[0]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Super Mario Land", alias);

	// Zelda
	strcpy(alias, "");
	Map_getAlias(entries[1]->path, alias);
	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", alias);

	// Metroid (no map.txt, so no alias should be set)
	strcpy(alias, "Original Name");
	Map_getAlias(entries[2]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Original Name", alias); // Unchanged

	// Cleanup
	Collection_freeEntries(entries, entry_count);
}

/**
 * Integration test: Recent games round-trip with multiple entries
 *
 * Workflow:
 * 1. Create several ROMs
 * 2. Save multiple entries to recent.txt
 * 3. Load and verify order preserved
 * 4. Modify list and save again
 * 5. Verify changes persist
 */
void test_recent_games_roundtrip(void) {
	char path[512];

	// Create ROMs
	snprintf(path, sizeof(path), "%s/Roms/GB/game1.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/GB/game2.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/NES/game3.nes", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Create recent entries
	Recent_Entry** entries = malloc(3 * sizeof(Recent_Entry*));

	entries[0] = malloc(sizeof(Recent_Entry));
	snprintf(path, sizeof(path), "/Roms/GB/game1.gb");
	entries[0]->path = strdup(path);
	entries[0]->alias = strdup("Game One");

	entries[1] = malloc(sizeof(Recent_Entry));
	snprintf(path, sizeof(path), "/Roms/GB/game2.gb");
	entries[1]->path = strdup(path);
	entries[1]->alias = NULL; // No alias

	entries[2] = malloc(sizeof(Recent_Entry));
	snprintf(path, sizeof(path), "/Roms/NES/game3.nes");
	entries[2]->path = strdup(path);
	entries[2]->alias = strdup("Game Three");

	// Save to recent.txt
	char recent_path[512];
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.minui/recent.txt",
	         test_dir);

	int saved = Recent_save(recent_path, entries, 3);
	TEST_ASSERT_TRUE(saved);

	// Load and verify
	int loaded_count = 0;
	Recent_Entry** loaded = Recent_parse(recent_path, test_dir, &loaded_count);
	TEST_ASSERT_NOT_NULL(loaded);
	TEST_ASSERT_EQUAL(3, loaded_count);

	// Verify order and content (loaded entries have relative paths)
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/game1.gb", loaded[0]->path);
	TEST_ASSERT_EQUAL_STRING("Game One", loaded[0]->alias);

	TEST_ASSERT_EQUAL_STRING("/Roms/GB/game2.gb", loaded[1]->path);
	TEST_ASSERT_NULL(loaded[1]->alias);

	TEST_ASSERT_EQUAL_STRING("/Roms/NES/game3.nes", loaded[2]->path);
	TEST_ASSERT_EQUAL_STRING("Game Three", loaded[2]->alias);

	// Modify list (remove middle entry, add new one at front)
	Recent_Entry** modified = malloc(3 * sizeof(Recent_Entry*));

	// New entry at front (most recent)
	modified[0] = malloc(sizeof(Recent_Entry));
	snprintf(path, sizeof(path), "/Roms/NES/game3.nes");
	modified[0]->path = strdup(path);
	modified[0]->alias = strdup("Game Three (Updated)");

	// Keep first and third from original
	modified[1] = malloc(sizeof(Recent_Entry));
	snprintf(path, sizeof(path), "/Roms/GB/game1.gb");
	modified[1]->path = strdup(path);
	modified[1]->alias = strdup("Game One");

	modified[2] = malloc(sizeof(Recent_Entry));
	snprintf(path, sizeof(path), "%s/Roms/GB/game2.gb", test_dir);
	// Remove the test_dir prefix for storage (paths are relative)
	snprintf(path, sizeof(path), "/Roms/GB/game2.gb");
	modified[2]->path = strdup(path);
	modified[2]->alias = NULL;

	// Save modified list
	saved = Recent_save(recent_path, modified, 3);
	TEST_ASSERT_TRUE(saved);

	// Load again and verify changes
	Recent_Entry** reloaded = Recent_parse(recent_path, test_dir, &loaded_count);
	TEST_ASSERT_NOT_NULL(reloaded);
	TEST_ASSERT_EQUAL(3, loaded_count);

	// Verify new order (loaded entries have relative paths)
	TEST_ASSERT_EQUAL_STRING("/Roms/NES/game3.nes", reloaded[0]->path);
	TEST_ASSERT_EQUAL_STRING("Game Three (Updated)", reloaded[0]->alias);

	// Cleanup
	Recent_freeEntries(entries, 3);
	Recent_freeEntries(loaded, 3);
	Recent_freeEntries(modified, 3);
	Recent_freeEntries(reloaded, 3);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Multi-disc workflows
	RUN_TEST(test_multi_disc_game_complete_workflow);
	RUN_TEST(test_multi_disc_detection);

	// Collection workflows
	RUN_TEST(test_collection_with_aliases);

	// Recent games workflows
	RUN_TEST(test_recent_games_roundtrip);

	return UNITY_END();
}
