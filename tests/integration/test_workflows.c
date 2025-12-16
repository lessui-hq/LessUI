/**
 * test_workflows.c - Integration tests for Launcher workflows
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

#include "../../workspace/all/common/binary_file_utils.h"
#include "collection_parser.h"
#include "launcher_m3u.h"
#include "launcher_map.h"
#include "player_paths.h"
#include "launcher_file_utils.h"
#include "recent_file.h"
#include "../support/platform.h"
#include "unity.h"
#include "integration_support.h"

// Test directory (created in setUp, removed in tearDown)
static char test_dir[256];

void setUp(void) {
	// Create temp directory for each test
	strcpy(test_dir, "/tmp/launcher_integration_XXXXXX");
	char* result = create_test_launcher_structure(test_dir);
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
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.launcher/recent.txt",
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
 * 2. Verify Launcher_hasM3u() detects M3U
 * 3. Verify Launcher_hasCue() detects CUE
 * 4. Test interaction between M3U and CUE detection
 */
void test_multi_disc_detection(void) {
	char path[512];

	// Create multi-disc game structure
	// Launcher_hasM3u expects: /Roms/PS1/Game/disc.bin and looks for /Roms/PS1/Game.m3u
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* disc_files[] = {"Game/disc1.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, disc_files, 1));

	// Create CUE file in game directory
	// Launcher_hasCue expects directory path and looks for dir/dirname.cue
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/Game.cue", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Test M3U detection
	char rom_path[512];
	snprintf(rom_path, sizeof(rom_path), "%s/Roms/PS1/Game/disc1.bin", test_dir);

	char m3u_path[512];
	int has_m3u = Launcher_hasM3u(rom_path, m3u_path);
	TEST_ASSERT_TRUE(has_m3u);
	TEST_ASSERT_EQUAL_STRING_LEN("/Roms/PS1/Game.m3u", m3u_path + strlen(test_dir),
	                             strlen("/Roms/PS1/Game.m3u"));

	// Test CUE detection (expects directory path, not ROM path)
	char dir_path[512];
	snprintf(dir_path, sizeof(dir_path), "%s/Roms/PS1/Game", test_dir);

	char cue_path[512];
	int has_cue = Launcher_hasCue(dir_path, cue_path);
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
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.launcher/recent.txt",
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
// Player Save File Workflows
///////////////////////////////

/**
 * Integration test: Save state path generation + binary file I/O
 *
 * Workflow:
 * 1. Generate save state paths using Player_getSaveStatePath
 * 2. Write save state data using BinaryFile_write
 * 3. Read back using BinaryFile_read
 * 4. Verify data integrity across path generation and file I/O
 */
void test_player_save_state_workflow(void) {
	char path[512];
	char save_path[256];
	char states_dir[512];

	// Setup ROM
	snprintf(path, sizeof(path), "%s/Roms/GB/mario.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Generate save state path for slot 0
	snprintf(states_dir, sizeof(states_dir), "%s/.userdata/miyoomini/gambatte",
	         test_dir);
	PlayerPaths_getState(save_path, states_dir, "mario", 0);

	// Verify path format
	TEST_ASSERT_TRUE(strstr(save_path, ".st0") != NULL);
	TEST_ASSERT_TRUE(strstr(save_path, "gambatte") != NULL);

	// Create parent directory for save file
	TEST_ASSERT_TRUE(create_parent_dir(save_path));

	// Write save state data
	unsigned char save_data[1024];
	for (int i = 0; i < 1024; i++) {
		save_data[i] = i % 256;
	}

	int written = BinaryFile_write(save_path, save_data, 1024);
	TEST_ASSERT_EQUAL(1024, written);

	// Read back and verify
	unsigned char read_data[1024];
	int read = BinaryFile_read(save_path, read_data, 1024);
	TEST_ASSERT_EQUAL(1024, read);

	// Verify data integrity
	for (int i = 0; i < 1024; i++) {
		TEST_ASSERT_EQUAL(save_data[i], read_data[i]);
	}
}

/**
 * Integration test: SRAM and RTC file handling
 *
 * Workflow:
 * 1. Generate SRAM and RTC paths
 * 2. Write data to both files
 * 3. Verify both exist and contain correct data
 * 4. Test Player save file integration
 */
void test_player_sram_rtc_workflow(void) {
	char sram_path[256];
	char rtc_path[256];
	char saves_dir[512];

	// Setup save directory
	snprintf(saves_dir, sizeof(saves_dir), "%s/.userdata/miyoomini/gambatte",
	         test_dir);

	// Generate SRAM path
	PlayerPaths_getSRAM(sram_path, saves_dir, "pokemon");
	TEST_ASSERT_TRUE(strstr(sram_path, ".sav") != NULL);

	// Generate RTC path
	PlayerPaths_getRTC(rtc_path, saves_dir, "pokemon");
	TEST_ASSERT_TRUE(strstr(rtc_path, ".rtc") != NULL);

	// Create parent directories
	TEST_ASSERT_TRUE(create_parent_dir(sram_path));
	TEST_ASSERT_TRUE(create_parent_dir(rtc_path));

	// Write SRAM data (32KB like Game Boy)
	unsigned char sram[32768];
	memset(sram, 0xFF, sizeof(sram));
	sram[0] = 0xAB; // Header byte
	sram[1] = 0xCD;

	int written = BinaryFile_write(sram_path, sram, sizeof(sram));
	TEST_ASSERT_EQUAL(32768, written);

	// Write RTC data (8 bytes)
	unsigned char rtc[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	written = BinaryFile_write(rtc_path, rtc, 8);
	TEST_ASSERT_EQUAL(8, written);

	// Read back and verify
	unsigned char sram_read[32768];
	int read = BinaryFile_read(sram_path, sram_read, sizeof(sram_read));
	TEST_ASSERT_EQUAL(32768, read);
	TEST_ASSERT_EQUAL(0xAB, sram_read[0]);
	TEST_ASSERT_EQUAL(0xCD, sram_read[1]);

	unsigned char rtc_read[8];
	read = BinaryFile_read(rtc_path, rtc_read, 8);
	TEST_ASSERT_EQUAL(8, read);
	TEST_ASSERT_EQUAL_MEMORY(rtc, rtc_read, 8);
}

///////////////////////////////
// Collection + M3U Integration
///////////////////////////////

/**
 * Integration test: Collection containing M3U files with aliases
 *
 * Workflow:
 * 1. Create collection with multi-disc games
 * 2. Each game has M3U and map.txt
 * 3. Parse collection
 * 4. Verify M3U detection works for collection entries
 * 5. Verify aliases apply correctly
 */
void test_collection_with_m3u_games(void) {
	char path[512];

	// Create first multi-disc game (FF7)
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF7/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF7/disc2.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* ff7_discs[] = {"FF7/disc1.bin", "FF7/disc2.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF7.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, ff7_discs, 2));

	// Create second multi-disc game (MGS)
	snprintf(path, sizeof(path), "%s/Roms/PS1/MGS/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/MGS/disc2.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* mgs_discs[] = {"MGS/disc1.bin", "MGS/disc2.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/MGS.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, mgs_discs, 2));

	// Create map.txt with aliases
	const char* rom_names[] = {"FF7.m3u", "MGS.m3u"};
	const char* aliases[] = {"Final Fantasy VII", "Metal Gear Solid"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, rom_names, aliases, 2));

	// Create collection
	const char* collection_paths[] = {"/Roms/PS1/FF7.m3u", "/Roms/PS1/MGS.m3u"};
	snprintf(path, sizeof(path), "%s/Collections/MultiDisc.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_collection(path, collection_paths, 2));

	// Parse collection
	int entry_count = 0;
	Collection_Entry** entries = Collection_parse(path, test_dir, &entry_count);
	TEST_ASSERT_NOT_NULL(entries);
	TEST_ASSERT_EQUAL(2, entry_count);

	// Verify entries are M3U files (check extension)
	TEST_ASSERT_TRUE(strstr(entries[0]->path, ".m3u") != NULL);
	TEST_ASSERT_TRUE(strstr(entries[1]->path, ".m3u") != NULL);

	// Parse the M3U and verify discs
	int disc_count = 0;
	M3U_Disc** discs = M3U_getAllDiscs(entries[0]->path, &disc_count);
	TEST_ASSERT_NOT_NULL(discs);
	TEST_ASSERT_EQUAL(2, disc_count);

	// Verify map aliases work
	char alias[256];
	strcpy(alias, "");
	Map_getAlias(entries[0]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Final Fantasy VII", alias);

	strcpy(alias, "");
	Map_getAlias(entries[1]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Metal Gear Solid", alias);

	// Cleanup
	M3U_freeDiscs(discs, disc_count);
	Collection_freeEntries(entries, entry_count);
}

///////////////////////////////
// File Detection Integration
///////////////////////////////

/**
 * Integration test: All file detection utilities working together
 *
 * Workflow:
 * 1. Create complex ROM directory with mixed content
 * 2. Test hasEmu, hasM3u, hasCue, hasNonHiddenFiles together
 * 3. Verify correct detection of all file types
 */
void test_file_detection_integration(void) {
	char path[512];

	// Create emulator pak (shared location)
	snprintf(path, sizeof(path), "%s/Emus/PCSX.pak/launch.sh", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Also create in Paks directory for detection
	snprintf(path, sizeof(path), "%s/Paks/Emus/PCSX.pak/launch.sh", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Create multi-disc game structure
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/Game.cue", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* disc_files[] = {"Game/disc1.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, disc_files, 1));

	// Create hidden files in directory
	snprintf(path, sizeof(path), "%s/Roms/PS1/.DS_Store", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/PS1/.hidden", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Test emulator detection
	char paks_path[512];
	snprintf(paks_path, sizeof(paks_path), "%s/Paks", test_dir);
	int has_emu = Launcher_hasEmu("PCSX", paks_path, test_dir, "miyoomini");
	TEST_ASSERT_TRUE(has_emu);

	// Test M3U detection
	char rom_path[512];
	snprintf(rom_path, sizeof(rom_path), "%s/Roms/PS1/Game/disc1.bin", test_dir);
	char m3u_path[512];
	has_emu = Launcher_hasM3u(rom_path, m3u_path);
	TEST_ASSERT_TRUE(has_emu);

	// Test CUE detection
	char dir_path[512];
	snprintf(dir_path, sizeof(dir_path), "%s/Roms/PS1/Game", test_dir);
	char cue_path[512];
	int has_cue = Launcher_hasCue(dir_path, cue_path);
	TEST_ASSERT_TRUE(has_cue);

	// Test hasNonHiddenFiles (should see ROM files, not hidden files)
	snprintf(dir_path, sizeof(dir_path), "%s/Roms/PS1", test_dir);
	int has_files = Launcher_hasNonHiddenFiles(dir_path);
	TEST_ASSERT_TRUE(has_files);

	// Test directory with ONLY hidden files
	snprintf(path, sizeof(path), "%s/Roms/Empty/.DS_Store", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(dir_path, sizeof(dir_path), "%s/Roms/Empty", test_dir);
	has_files = Launcher_hasNonHiddenFiles(dir_path);
	TEST_ASSERT_FALSE(has_files);
}

///////////////////////////////
// Error Handling Integration
///////////////////////////////

/**
 * Integration test: Error handling across multiple modules
 *
 * Workflow:
 * 1. Test with missing files, empty files, invalid data
 * 2. Verify modules handle errors gracefully when integrated
 */
void test_error_handling_integration(void) {
	char path[512];

	// Test 1: Collection with all missing ROMs
	const char* missing_paths[] = {"/Roms/GB/missing1.gb", "/Roms/GB/missing2.gb"};
	snprintf(path, sizeof(path), "%s/Collections/Missing.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_collection(path, missing_paths, 2));

	int entry_count = 0;
	Collection_Entry** entries = Collection_parse(path, test_dir, &entry_count);
	TEST_ASSERT_EQUAL(0, entry_count); // All ROMs missing, so empty
	Collection_freeEntries(entries, entry_count);

	// Test 2: Recent.txt with missing ROMs
	Recent_Entry** recent_entries = malloc(2 * sizeof(Recent_Entry*));
	recent_entries[0] = malloc(sizeof(Recent_Entry));
	recent_entries[0]->path = strdup("/Roms/GB/exists.gb");
	recent_entries[0]->alias = strdup("Good Game");

	recent_entries[1] = malloc(sizeof(Recent_Entry));
	recent_entries[1]->path = strdup("/Roms/GB/missing.gb");
	recent_entries[1]->alias = strdup("Missing Game");

	// Create only the first ROM
	snprintf(path, sizeof(path), "%s/Roms/GB/exists.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Save and load
	char recent_path[512];
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.launcher/recent.txt",
	         test_dir);
	int saved = Recent_save(recent_path, recent_entries, 2);
	TEST_ASSERT_TRUE(saved);

	int loaded_count = 0;
	Recent_Entry** loaded = Recent_parse(recent_path, test_dir, &loaded_count);
	TEST_ASSERT_NOT_NULL(loaded);
	TEST_ASSERT_EQUAL(1, loaded_count); // Only one ROM exists
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/exists.gb", loaded[0]->path);

	// Cleanup
	Recent_freeEntries(recent_entries, 2);
	Recent_freeEntries(loaded, loaded_count);

	// Test 3: M3U with all missing discs
	const char* missing_discs[] = {"missing1.bin", "missing2.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/BadGame.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, missing_discs, 2));

	int disc_count = 0;
	M3U_Disc** discs = M3U_getAllDiscs(path, &disc_count);
	TEST_ASSERT_EQUAL(0, disc_count); // All discs missing
	M3U_freeDiscs(discs, disc_count);
}

///////////////////////////////
// Complex Multi-System Workflows
///////////////////////////////

/**
 * Integration test: Collection spanning multiple systems with mixed features
 *
 * Workflow:
 * 1. Create collection with ROMs from GB, NES, PS1
 * 2. Include single-disc games, multi-disc games, and games with aliases
 * 3. Test all modules working together: Collection + M3U + Map + File detection
 */
void test_multi_system_collection_workflow(void) {
	char path[512];

	// GB: Simple ROM
	snprintf(path, sizeof(path), "%s/Roms/GB/tetris.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// NES: ROM with alias
	snprintf(path, sizeof(path), "%s/Roms/NES/smb.nes", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* nes_names[] = {"smb.nes"};
	const char* nes_aliases[] = {"Super Mario Bros."};
	snprintf(path, sizeof(path), "%s/Roms/NES/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, nes_names, nes_aliases, 1));

	// PS1: Multi-disc game with alias
	snprintf(path, sizeof(path), "%s/Roms/PS1/RE2/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/RE2/disc2.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* re2_discs[] = {"RE2/disc1.bin", "RE2/disc2.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/RE2.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, re2_discs, 2));

	const char* ps1_names[] = {"RE2.m3u"};
	const char* ps1_aliases[] = {"Resident Evil 2"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, ps1_names, ps1_aliases, 1));

	// Create collection with all three
	const char* collection_paths[] = {"/Roms/GB/tetris.gb", "/Roms/NES/smb.nes",
	                                  "/Roms/PS1/RE2.m3u"};
	snprintf(path, sizeof(path), "%s/Collections/Best.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_collection(path, collection_paths, 3));

	// Parse collection
	int entry_count = 0;
	Collection_Entry** entries = Collection_parse(path, test_dir, &entry_count);
	TEST_ASSERT_NOT_NULL(entries);
	TEST_ASSERT_EQUAL(3, entry_count);

	// Verify each entry
	char alias[256];

	// Tetris: no alias
	strcpy(alias, "Original");
	Map_getAlias(entries[0]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Original", alias); // Unchanged

	// SMB: has alias
	strcpy(alias, "");
	Map_getAlias(entries[1]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros.", alias);

	// RE2: has alias and M3U
	strcpy(alias, "");
	Map_getAlias(entries[2]->path, alias);
	TEST_ASSERT_EQUAL_STRING("Resident Evil 2", alias);

	// Verify it's an M3U file
	TEST_ASSERT_TRUE(strstr(entries[2]->path, ".m3u") != NULL);

	// Parse M3U and verify discs
	int disc_count = 0;
	M3U_Disc** discs = M3U_getAllDiscs(entries[2]->path, &disc_count);
	TEST_ASSERT_EQUAL(2, disc_count);

	// Cleanup
	M3U_freeDiscs(discs, disc_count);
	Collection_freeEntries(entries, entry_count);
}

/**
 * Integration test: Recent games with save states
 *
 * Workflow:
 * 1. Create ROM and save state
 * 2. Add to recent games
 * 3. Verify save state exists for recent game
 * 4. Test cross-module file verification
 */
void test_recent_with_save_states(void) {
	char path[512];

	// Create ROM
	snprintf(path, sizeof(path), "%s/Roms/GB/game.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Generate save state path
	char save_path[256];
	char states_dir[512];
	snprintf(states_dir, sizeof(states_dir), "%s/.userdata/miyoomini/gambatte",
	         test_dir);
	PlayerPaths_getState(save_path, states_dir, "game", 0);

	// Create parent directory
	TEST_ASSERT_TRUE(create_parent_dir(save_path));

	// Write save state
	unsigned char save_data[512];
	memset(save_data, 0x42, sizeof(save_data));
	int written = BinaryFile_write(save_path, save_data, sizeof(save_data));
	TEST_ASSERT_EQUAL(512, written);

	// Add to recent games
	Recent_Entry** entries = malloc(sizeof(Recent_Entry*));
	entries[0] = malloc(sizeof(Recent_Entry));
	entries[0]->path = strdup("/Roms/GB/game.gb");
	entries[0]->alias = strdup("My Game");

	char recent_path[512];
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.launcher/recent.txt",
	         test_dir);
	int saved = Recent_save(recent_path, entries, 1);
	TEST_ASSERT_TRUE(saved);

	// Load recent games
	int loaded_count = 0;
	Recent_Entry** loaded = Recent_parse(recent_path, test_dir, &loaded_count);
	TEST_ASSERT_EQUAL(1, loaded_count);

	// Verify save state path matches what we used earlier
	char verify_save_path[256];
	PlayerPaths_getState(verify_save_path, states_dir, "game", 0);
	TEST_ASSERT_EQUAL_STRING(save_path, verify_save_path);

	// Verify can read save state data
	unsigned char read_data[512];
	int read = BinaryFile_read(save_path, read_data, sizeof(read_data));
	TEST_ASSERT_EQUAL(512, read);
	TEST_ASSERT_EQUAL(0x42, read_data[0]);

	// Cleanup
	Recent_freeEntries(entries, 1);
	Recent_freeEntries(loaded, loaded_count);
}

///////////////////////////////
// Config File Workflows
///////////////////////////////

/**
 * Integration test: Player config file path generation + file I/O
 *
 * Workflow:
 * 1. Generate game-specific config path
 * 2. Write config data
 * 3. Generate global config path
 * 4. Verify both configs can coexist
 */
void test_player_config_file_integration(void) {
	char game_cfg[256];
	char global_cfg[256];
	char config_dir[512];

	snprintf(config_dir, sizeof(config_dir), "%s/.userdata/miyoomini/gpsp",
	         test_dir);

	// Generate game-specific config
	PlayerConfig_getPath(game_cfg, config_dir, "Pokemon", NULL);
	TEST_ASSERT_TRUE(strstr(game_cfg, "Pokemon.cfg") != NULL);

	// Generate global config
	PlayerConfig_getPath(global_cfg, config_dir, NULL, NULL);
	TEST_ASSERT_TRUE(strstr(global_cfg, "player.cfg") != NULL);

	// Verify they're different
	TEST_ASSERT_NOT_EQUAL(strcmp(game_cfg, global_cfg), 0);

	// Create parent dir
	TEST_ASSERT_TRUE(create_parent_dir(game_cfg));

	// Write config data
	const char* game_config = "frameskip=0\nvolume=80\n";
	const char* global_config = "show_fps=1\nauto_save=1\n";

	FILE* f = fopen(game_cfg, "w");
	TEST_ASSERT_NOT_NULL(f);
	fputs(game_config, f);
	fclose(f);

	f = fopen(global_cfg, "w");
	TEST_ASSERT_NOT_NULL(f);
	fputs(global_config, f);
	fclose(f);

	// Verify both exist
	f = fopen(game_cfg, "r");
	TEST_ASSERT_NOT_NULL(f);
	fclose(f);

	f = fopen(global_cfg, "r");
	TEST_ASSERT_NOT_NULL(f);
	fclose(f);
}

/**
 * Integration test: Device-specific config tags
 *
 * Workflow:
 * 1. Generate configs for different devices
 * 2. Verify device tags applied correctly
 * 3. Test cross-device config isolation
 */
void test_config_device_tags(void) {
	char miyoo_cfg[256];
	char rg35_cfg[256];
	char config_dir[512];

	snprintf(config_dir, sizeof(config_dir), "%s/.userdata/shared/gpsp", test_dir);

	// Generate miyoomini config
	PlayerConfig_getPath(miyoo_cfg, config_dir, "Game", "miyoomini");
	TEST_ASSERT_TRUE(strstr(miyoo_cfg, "-miyoomini.cfg") != NULL);

	// Generate rg35xx config
	PlayerConfig_getPath(rg35_cfg, config_dir, "Game", "rg35xx");
	TEST_ASSERT_TRUE(strstr(rg35_cfg, "-rg35xx.cfg") != NULL);

	// Verify they're different
	TEST_ASSERT_NOT_EQUAL(strcmp(miyoo_cfg, rg35_cfg), 0);

	// Same game, different devices should have different configs
	TEST_ASSERT_TRUE(strstr(miyoo_cfg, "Game-miyoomini") != NULL);
	TEST_ASSERT_TRUE(strstr(rg35_cfg, "Game-rg35xx") != NULL);
}

///////////////////////////////
// Auto-Resume Workflows
///////////////////////////////

/**
 * Integration test: Auto-resume with slot 9
 *
 * Workflow:
 * 1. Create save state on slot 9 (auto-resume slot)
 * 2. Add game to recent list
 * 3. Verify resume capability
 */
void test_auto_resume_slot_9(void) {
	char save_path[256];
	char states_dir[512];

	// Create ROM
	char rom_path[512];
	snprintf(rom_path, sizeof(rom_path), "%s/Roms/GB/zelda.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(rom_path));

	// Generate slot 9 save state (auto-resume)
	snprintf(states_dir, sizeof(states_dir), "%s/.userdata/miyoomini/gambatte",
	         test_dir);
	PlayerPaths_getState(save_path, states_dir, "zelda", 9);

	TEST_ASSERT_TRUE(strstr(save_path, ".st9") != NULL);

	// Create save state
	TEST_ASSERT_TRUE(create_parent_dir(save_path));
	unsigned char save_data[256];
	memset(save_data, 0x99, sizeof(save_data));
	int written = BinaryFile_write(save_path, save_data, sizeof(save_data));
	TEST_ASSERT_EQUAL(256, written);

	// Add to recent games
	Recent_Entry** entries = malloc(sizeof(Recent_Entry*));
	entries[0] = malloc(sizeof(Recent_Entry));
	entries[0]->path = strdup("/Roms/GB/zelda.gb");
	entries[0]->alias = strdup("The Legend of Zelda");

	char recent_path[512];
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.launcher/recent.txt",
	         test_dir);
	int saved = Recent_save(recent_path, entries, 1);
	TEST_ASSERT_TRUE(saved);

	// Load and verify
	int count = 0;
	Recent_Entry** loaded = Recent_parse(recent_path, test_dir, &count);
	TEST_ASSERT_EQUAL(1, count);

	// Verify slot 9 save exists for recent game (could enable resume)
	char verify_path[256];
	char full_rom[512];
	snprintf(full_rom, sizeof(full_rom), "%s%s", test_dir, loaded[0]->path);
	PlayerPaths_getState(verify_path, states_dir, "zelda", 9);
	TEST_ASSERT_EQUAL_STRING(save_path, verify_path);

	// Verify save data is intact
	unsigned char read_data[256];
	int read = BinaryFile_read(save_path, read_data, sizeof(read_data));
	TEST_ASSERT_EQUAL(256, read);
	TEST_ASSERT_EQUAL(0x99, read_data[0]);

	Recent_freeEntries(entries, 1);
	Recent_freeEntries(loaded, count);
}

/**
 * Integration test: All 10 save state slots
 *
 * Workflow:
 * 1. Create save states for all slots (0-9)
 * 2. Verify all can be written and read
 * 3. Test slot isolation
 */
void test_all_save_slots(void) {
	char states_dir[512];
	snprintf(states_dir, sizeof(states_dir), "%s/.userdata/miyoomini/snes9x",
	         test_dir);

	// Create saves for all 10 slots
	for (int slot = 0; slot < 10; slot++) {
		char save_path[256];
		PlayerPaths_getState(save_path, states_dir, "metroid", slot);

		// Verify slot number in filename
		char expected[10];
		snprintf(expected, sizeof(expected), ".st%d", slot);
		TEST_ASSERT_TRUE(strstr(save_path, expected) != NULL);

		// Write unique data per slot
		TEST_ASSERT_TRUE(create_parent_dir(save_path));
		unsigned char data[64];
		memset(data, slot, sizeof(data));

		int written = BinaryFile_write(save_path, data, sizeof(data));
		TEST_ASSERT_EQUAL(64, written);
	}

	// Read back and verify each slot has correct data
	for (int slot = 0; slot < 10; slot++) {
		char save_path[256];
		PlayerPaths_getState(save_path, states_dir, "metroid", slot);

		unsigned char data[64];
		int read = BinaryFile_read(save_path, data, sizeof(data));
		TEST_ASSERT_EQUAL(64, read);
		TEST_ASSERT_EQUAL(slot, data[0]);
		TEST_ASSERT_EQUAL(slot, data[63]);
	}
}

///////////////////////////////
// Hidden ROM Workflows
///////////////////////////////

/**
 * Integration test: Hidden ROMs in map.txt filtered from collection
 *
 * Workflow:
 * 1. Create collection with 3 ROMs
 * 2. One ROM has alias starting with '.' (hidden)
 * 3. Verify hidden ROM handling in integration
 */
void test_hidden_roms_in_map(void) {
	char path[512];

	// Create ROMs
	snprintf(path, sizeof(path), "%s/Roms/GB/good.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/GB/hidden.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/GB/another.gb", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Create map with one hidden ROM
	const char* rom_names[] = {"good.gb", "hidden.gb", "another.gb"};
	const char* aliases[] = {"Good Game", ".Hidden Game", "Another Game"};
	snprintf(path, sizeof(path), "%s/Roms/GB/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, rom_names, aliases, 3));

	// Get aliases and verify
	char alias[256];
	char rom_path[512];

	// Good ROM
	snprintf(rom_path, sizeof(rom_path), "%s/Roms/GB/good.gb", test_dir);
	strcpy(alias, "");
	Map_getAlias(rom_path, alias);
	TEST_ASSERT_EQUAL_STRING("Good Game", alias);

	// Hidden ROM - alias starts with '.'
	snprintf(rom_path, sizeof(rom_path), "%s/Roms/GB/hidden.gb", test_dir);
	strcpy(alias, "");
	Map_getAlias(rom_path, alias);
	TEST_ASSERT_EQUAL_STRING(".Hidden Game", alias);
	TEST_ASSERT_EQUAL('.', alias[0]); // Verify starts with dot

	// Another ROM
	snprintf(rom_path, sizeof(rom_path), "%s/Roms/GB/another.gb", test_dir);
	strcpy(alias, "");
	Map_getAlias(rom_path, alias);
	TEST_ASSERT_EQUAL_STRING("Another Game", alias);
}

///////////////////////////////
// Advanced Multi-Disc Scenarios
///////////////////////////////

/**
 * Integration test: M3U getFirstDisc vs getAllDiscs consistency
 *
 * Workflow:
 * 1. Create M3U with multiple discs
 * 2. Get first disc with M3U_getFirstDisc
 * 3. Get all discs with M3U_getAllDiscs
 * 4. Verify first disc from both methods matches
 */
void test_m3u_first_vs_all_consistency(void) {
	char path[512];

	// Create multi-disc game
	snprintf(path, sizeof(path), "%s/Roms/PS1/GT2/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/GT2/disc2.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	const char* discs[] = {"GT2/disc1.bin", "GT2/disc2.bin"};
	char m3u_path[512];
	snprintf(m3u_path, sizeof(m3u_path), "%s/Roms/PS1/GT2.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(m3u_path, discs, 2));

	// Get first disc
	char first_disc[256];
	int found = M3U_getFirstDisc(m3u_path, first_disc);
	TEST_ASSERT_TRUE(found);

	// Get all discs
	int disc_count = 0;
	M3U_Disc** all_discs = M3U_getAllDiscs(m3u_path, &disc_count);
	TEST_ASSERT_EQUAL(2, disc_count);

	// Verify first disc matches
	TEST_ASSERT_EQUAL_STRING(first_disc, all_discs[0]->path);

	M3U_freeDiscs(all_discs, disc_count);
}

/**
 * Integration test: Nested game directory structures
 *
 * Workflow:
 * 1. Create deeply nested ROM structure
 * 2. Test M3U detection in nested dirs
 * 3. Test path handling across modules
 */
void test_nested_directories(void) {
	char path[512];

	// Create nested structure: Roms/PS1/Games/Action/FF7/disc1.bin
	snprintf(path, sizeof(path), "%s/Roms/PS1/Games/Action/FF7/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/PS1/Games/Action/FF7/disc2.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// M3U at Action level, disc paths relative to M3U location
	const char* discs[] = {"FF7/disc1.bin", "FF7/disc2.bin"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/Games/Action/FF7.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(path, discs, 2));

	// Parse M3U
	int disc_count = 0;
	M3U_Disc** m3u_discs = M3U_getAllDiscs(path, &disc_count);
	TEST_ASSERT_EQUAL(2, disc_count);

	// Verify paths constructed correctly (relative to M3U location)
	TEST_ASSERT_TRUE(strstr(m3u_discs[0]->path, "FF7/disc1.bin") != NULL);
	TEST_ASSERT_TRUE(strstr(m3u_discs[1]->path, "FF7/disc2.bin") != NULL);

	M3U_freeDiscs(m3u_discs, disc_count);
}

///////////////////////////////
// Directory Utilities Advanced Integration
///////////////////////////////

/**
 * Integration test: Empty directory with collection
 *
 * Workflow:
 * 1. Create collection pointing to empty directory
 * 2. Verify hasNonHiddenFiles returns false
 * 3. Test collection parser handles this gracefully
 */
void test_empty_directory_collection(void) {
	char path[512];

	// Create empty directory (create a temp file then remove it to create the dir)
	snprintf(path, sizeof(path), "%s/Roms/N64/.tmp", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	unlink(path); // Remove the file, leaving empty directory

	// Verify it's empty
	snprintf(path, sizeof(path), "%s/Roms/N64", test_dir);
	int has_files = Launcher_hasNonHiddenFiles(path);
	TEST_ASSERT_FALSE(has_files);

	// Create collection pointing to non-existent ROMs in empty dir
	const char* missing[] = {"/Roms/N64/game1.z64", "/Roms/N64/game2.z64"};
	snprintf(path, sizeof(path), "%s/Collections/N64Games.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_collection(path, missing, 2));

	// Parse should return 0 entries (all missing)
	int count = 0;
	Collection_Entry** entries = Collection_parse(path, test_dir, &count);
	TEST_ASSERT_EQUAL(0, count);
	Collection_freeEntries(entries, count);
}

///////////////////////////////
// Cross-Module Comprehensive Workflows
///////////////////////////////

/**
 * Integration test: ROM with ALL features enabled
 *
 * Workflow:
 * 1. Create multi-disc game with M3U + CUE
 * 2. Add map.txt alias
 * 3. Create save state
 * 4. Add to recent games
 * 5. Verify all features work together
 */
void test_rom_with_all_features(void) {
	char path[512];

	// Create multi-disc game
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF8/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF8/disc2.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF8/disc3.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF8/disc4.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Create M3U
	const char* discs[] = {"FF8/disc1.bin", "FF8/disc2.bin", "FF8/disc3.bin",
	                       "FF8/disc4.bin"};
	char m3u_path[512];
	snprintf(m3u_path, sizeof(m3u_path), "%s/Roms/PS1/FF8.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(m3u_path, discs, 4));

	// Create CUE file
	snprintf(path, sizeof(path), "%s/Roms/PS1/FF8/FF8.cue", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Create map.txt with alias
	const char* names[] = {"FF8.m3u"};
	const char* aliases[] = {"Final Fantasy VIII"};
	snprintf(path, sizeof(path), "%s/Roms/PS1/map.txt", test_dir);
	TEST_ASSERT_TRUE(create_test_map(path, names, aliases, 1));

	// Create save state on slot 0
	char save_path[256];
	char states_dir[512];
	snprintf(states_dir, sizeof(states_dir), "%s/.userdata/miyoomini/pcsx", test_dir);
	PlayerPaths_getState(save_path, states_dir, "FF8", 0);
	TEST_ASSERT_TRUE(create_parent_dir(save_path));

	unsigned char save[512];
	memset(save, 0xF8, sizeof(save));
	TEST_ASSERT_EQUAL(512, BinaryFile_write(save_path, save, sizeof(save)));

	// Create SRAM
	char sram_path[256];
	PlayerPaths_getSRAM(sram_path, states_dir, "FF8");
	TEST_ASSERT_TRUE(create_parent_dir(sram_path));

	unsigned char sram[8192];
	memset(sram, 0x00, sizeof(sram));
	TEST_ASSERT_EQUAL(8192, BinaryFile_write(sram_path, sram, sizeof(sram)));

	// Add to recent
	Recent_Entry** entries = malloc(sizeof(Recent_Entry*));
	entries[0] = malloc(sizeof(Recent_Entry));
	entries[0]->path = strdup("/Roms/PS1/FF8.m3u");
	entries[0]->alias = NULL; // Will get from map.txt

	char recent_path[512];
	snprintf(recent_path, sizeof(recent_path), "%s/.userdata/.launcher/recent.txt",
	         test_dir);
	TEST_ASSERT_TRUE(Recent_save(recent_path, entries, 1));

	// NOW VERIFY ALL FEATURES WORK TOGETHER

	// 1. M3U parsing
	int disc_count = 0;
	M3U_Disc** m3u_discs = M3U_getAllDiscs(m3u_path, &disc_count);
	TEST_ASSERT_EQUAL(4, disc_count);
	TEST_ASSERT_EQUAL_STRING("Disc 1", m3u_discs[0]->name);
	TEST_ASSERT_EQUAL_STRING("Disc 4", m3u_discs[3]->name);

	// 2. Map alias
	char alias[256];
	strcpy(alias, "");
	Map_getAlias(m3u_path, alias);
	TEST_ASSERT_EQUAL_STRING("Final Fantasy VIII", alias);

	// 3. CUE detection
	char dir_path[512];
	snprintf(dir_path, sizeof(dir_path), "%s/Roms/PS1/FF8", test_dir);
	char cue_path[512];
	TEST_ASSERT_TRUE(Launcher_hasCue(dir_path, cue_path));

	// 4. Recent games
	int count = 0;
	Recent_Entry** loaded = Recent_parse(recent_path, test_dir, &count);
	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF8.m3u", loaded[0]->path);

	// 5. Save states exist
	char verify_save[256];
	PlayerPaths_getState(verify_save, states_dir, "FF8", 0);
	unsigned char verify_data[512];
	TEST_ASSERT_EQUAL(512, BinaryFile_read(verify_save, verify_data, sizeof(verify_data)));
	TEST_ASSERT_EQUAL(0xF8, verify_data[0]);

	// 6. SRAM exists
	char verify_sram[256];
	PlayerPaths_getSRAM(verify_sram, states_dir, "FF8");
	unsigned char verify_sram_data[8192];
	TEST_ASSERT_EQUAL(8192,
	                  BinaryFile_read(verify_sram, verify_sram_data, sizeof(verify_sram_data)));

	M3U_freeDiscs(m3u_discs, disc_count);
	Recent_freeEntries(entries, 1);
	Recent_freeEntries(loaded, count);
}

/**
 * Integration test: Multi-platform userdata structure
 *
 * Workflow:
 * 1. Create save data for same ROM on different platforms
 * 2. Verify data is isolated per platform
 * 3. Test cross-platform compatibility
 */
void test_multi_platform_save_isolation(void) {
	char miyoo_save[256];
	char rg35_save[256];
	char miyoo_dir[512];
	char rg35_dir[512];

	// Generate paths for miyoomini
	snprintf(miyoo_dir, sizeof(miyoo_dir), "%s/.userdata/miyoomini/gpsp", test_dir);
	PlayerPaths_getState(miyoo_save, miyoo_dir, "pokemon", 0);

	// Generate paths for rg35xx
	snprintf(rg35_dir, sizeof(rg35_dir), "%s/.userdata/rg35xx/gpsp", test_dir);
	PlayerPaths_getState(rg35_save, rg35_dir, "pokemon", 0);

	// Verify they're different
	TEST_ASSERT_NOT_EQUAL(strcmp(miyoo_save, rg35_save), 0);
	TEST_ASSERT_TRUE(strstr(miyoo_save, "miyoomini") != NULL);
	TEST_ASSERT_TRUE(strstr(rg35_save, "rg35xx") != NULL);

	// Create both saves with different data
	TEST_ASSERT_TRUE(create_parent_dir(miyoo_save));
	TEST_ASSERT_TRUE(create_parent_dir(rg35_save));

	unsigned char miyoo_data[128];
	memset(miyoo_data, 0xAA, sizeof(miyoo_data));
	TEST_ASSERT_EQUAL(128, BinaryFile_write(miyoo_save, miyoo_data, sizeof(miyoo_data)));

	unsigned char rg35_data[128];
	memset(rg35_data, 0xBB, sizeof(rg35_data));
	TEST_ASSERT_EQUAL(128, BinaryFile_write(rg35_save, rg35_data, sizeof(rg35_data)));

	// Read back and verify isolation
	unsigned char read_miyoo[128];
	unsigned char read_rg35[128];

	TEST_ASSERT_EQUAL(128, BinaryFile_read(miyoo_save, read_miyoo, sizeof(read_miyoo)));
	TEST_ASSERT_EQUAL(128, BinaryFile_read(rg35_save, read_rg35, sizeof(read_rg35)));

	TEST_ASSERT_EQUAL(0xAA, read_miyoo[0]);
	TEST_ASSERT_EQUAL(0xBB, read_rg35[0]);
}

/**
 * Integration test: M3U with individual CUE files per disc
 *
 * Workflow:
 * 1. Create multi-disc game where each disc has its own CUE
 * 2. Verify M3U parsing works
 * 3. Verify CUE detection works alongside M3U
 */
void test_m3u_with_multiple_cues(void) {
	char path[512];

	// Create game with CUE per disc
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc1.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc1.cue", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc2.bin", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc2.cue", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Create M3U
	const char* discs[] = {"Game/disc1.bin", "Game/disc2.bin"};
	char m3u_path[512];
	snprintf(m3u_path, sizeof(m3u_path), "%s/Roms/PS1/Game.m3u", test_dir);
	TEST_ASSERT_TRUE(create_test_m3u(m3u_path, discs, 2));

	// Also create directory-level CUE
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/Game.cue", test_dir);
	TEST_ASSERT_TRUE(create_test_rom(path));

	// Verify M3U parsing works
	int disc_count = 0;
	M3U_Disc** m3u_discs = M3U_getAllDiscs(m3u_path, &disc_count);
	TEST_ASSERT_EQUAL(2, disc_count);

	// Verify CUE detection (directory-level)
	char dir_path[512];
	snprintf(dir_path, sizeof(dir_path), "%s/Roms/PS1/Game", test_dir);
	char cue_path[512];
	TEST_ASSERT_TRUE(Launcher_hasCue(dir_path, cue_path));

	// Verify individual CUE files exist
	snprintf(path, sizeof(path), "%s/Roms/PS1/Game/disc1.cue", test_dir);
	FILE* f = fopen(path, "r");
	TEST_ASSERT_NOT_NULL(f);
	fclose(f);

	M3U_freeDiscs(m3u_discs, disc_count);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Multi-disc workflows
	RUN_TEST(test_multi_disc_game_complete_workflow);
	RUN_TEST(test_multi_disc_detection);
	RUN_TEST(test_m3u_first_vs_all_consistency);
	RUN_TEST(test_m3u_with_multiple_cues);
	RUN_TEST(test_nested_directories);

	// Collection workflows
	RUN_TEST(test_collection_with_aliases);
	RUN_TEST(test_collection_with_m3u_games);
	RUN_TEST(test_multi_system_collection_workflow);
	RUN_TEST(test_empty_directory_collection);

	// Recent games workflows
	RUN_TEST(test_recent_games_roundtrip);
	RUN_TEST(test_recent_with_save_states);

	// Player save file workflows
	RUN_TEST(test_player_save_state_workflow);
	RUN_TEST(test_player_sram_rtc_workflow);
	RUN_TEST(test_all_save_slots);
	RUN_TEST(test_auto_resume_slot_9);

	// Config file workflows
	RUN_TEST(test_player_config_file_integration);
	RUN_TEST(test_config_device_tags);

	// File detection integration
	RUN_TEST(test_file_detection_integration);

	// Hidden ROM workflows
	RUN_TEST(test_hidden_roms_in_map);

	// Cross-module comprehensive workflows
	RUN_TEST(test_rom_with_all_features);
	RUN_TEST(test_multi_platform_save_isolation);

	// Error handling
	RUN_TEST(test_error_handling_integration);

	return UNITY_END();
}
