/**
 * test_minarch_paths.c - Unit tests for MinArch path generation
 *
 * Tests path generation functions extracted from minarch.c.
 * These are pure sprintf logic with no file system dependencies.
 *
 * Test coverage:
 * - SRAM path generation (.sav files)
 * - RTC path generation (.rtc files)
 * - Save state path generation (.st0-.st9 files)
 * - Config path generation (.cfg files)
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/minarch_paths.h"

#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// SRAM Path Tests
///////////////////////////////

void test_getSRAMPath_generates_correct_path(void) {
	char path[512];

	MinArch_getSRAMPath(path, "/mnt/SDCARD/.userdata/miyoomini/gpsp", "Pokemon Red");

	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/.userdata/miyoomini/gpsp/Pokemon Red.sav", path);
}

void test_getSRAMPath_handles_special_characters(void) {
	char path[512];

	MinArch_getSRAMPath(path, "/saves", "Game (USA) (Rev 1)");

	TEST_ASSERT_EQUAL_STRING("/saves/Game (USA) (Rev 1).sav", path);
}

void test_getSRAMPath_handles_short_names(void) {
	char path[512];

	MinArch_getSRAMPath(path, "/data", "A");

	TEST_ASSERT_EQUAL_STRING("/data/A.sav", path);
}

///////////////////////////////
// RTC Path Tests
///////////////////////////////

void test_getRTCPath_generates_correct_path(void) {
	char path[512];

	MinArch_getRTCPath(path, "/mnt/SDCARD/.userdata/miyoomini/gpsp", "Pokemon Gold");

	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/.userdata/miyoomini/gpsp/Pokemon Gold.rtc", path);
}

void test_getRTCPath_different_from_SRAM(void) {
	char sram_path[512];
	char rtc_path[512];

	MinArch_getSRAMPath(sram_path, "/saves", "Game");
	MinArch_getRTCPath(rtc_path, "/saves", "Game");

	// Should differ only in extension
	TEST_ASSERT_NOT_EQUAL(0, strcmp(sram_path, rtc_path));
	TEST_ASSERT_EQUAL_STRING("/saves/Game.sav", sram_path);
	TEST_ASSERT_EQUAL_STRING("/saves/Game.rtc", rtc_path);
}

///////////////////////////////
// Save State Path Tests
///////////////////////////////

void test_getStatePath_generates_path_for_slot_0(void) {
	char path[512];

	MinArch_getStatePath(path, "/states", "Super Mario", 0);

	TEST_ASSERT_EQUAL_STRING("/states/Super Mario.st0", path);
}

void test_getStatePath_generates_path_for_slot_9(void) {
	char path[512];

	// Slot 9 is the auto-resume slot
	MinArch_getStatePath(path, "/states", "Zelda", 9);

	TEST_ASSERT_EQUAL_STRING("/states/Zelda.st9", path);
}

void test_getStatePath_all_slots_unique(void) {
	char paths[10][512];

	// Generate paths for all 10 slots
	for (int i = 0; i < 10; i++) {
		MinArch_getStatePath(paths[i], "/s", "Game", i);
	}

	// Verify each slot has unique path
	for (int i = 0; i < 10; i++) {
		for (int j = i + 1; j < 10; j++) {
			TEST_ASSERT_NOT_EQUAL(0, strcmp(paths[i], paths[j]));
		}
	}

	// Spot check
	TEST_ASSERT_EQUAL_STRING("/s/Game.st0", paths[0]);
	TEST_ASSERT_EQUAL_STRING("/s/Game.st5", paths[5]);
	TEST_ASSERT_EQUAL_STRING("/s/Game.st9", paths[9]);
}

void test_getStatePath_handles_long_game_names(void) {
	char path[512];

	MinArch_getStatePath(path, "/data", "The Legend of Zelda - A Link to the Past", 3);

	TEST_ASSERT_EQUAL_STRING("/data/The Legend of Zelda - A Link to the Past.st3", path);
}

///////////////////////////////
// Config Path Tests
///////////////////////////////

void test_getConfigPath_global_no_device_tag(void) {
	char path[512];

	MinArch_getConfigPath(path, "/config", NULL, NULL);

	TEST_ASSERT_EQUAL_STRING("/config/minarch.cfg", path);
}

void test_getConfigPath_global_with_device_tag(void) {
	char path[512];

	MinArch_getConfigPath(path, "/config", NULL, "rg35xx");

	TEST_ASSERT_EQUAL_STRING("/config/minarch-rg35xx.cfg", path);
}

void test_getConfigPath_game_specific_no_device_tag(void) {
	char path[512];

	MinArch_getConfigPath(path, "/config", "Super Mario Bros", NULL);

	TEST_ASSERT_EQUAL_STRING("/config/Super Mario Bros.cfg", path);
}

void test_getConfigPath_game_specific_with_device_tag(void) {
	char path[512];

	MinArch_getConfigPath(path, "/config", "Metroid", "miyoomini");

	TEST_ASSERT_EQUAL_STRING("/config/Metroid-miyoomini.cfg", path);
}

void test_getConfigPath_empty_device_tag_treated_as_null(void) {
	char path1[512];
	char path2[512];

	MinArch_getConfigPath(path1, "/cfg", "Game", NULL);
	MinArch_getConfigPath(path2, "/cfg", "Game", "");

	// Empty string should be treated same as NULL
	TEST_ASSERT_EQUAL_STRING(path1, path2);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_all_save_files_in_same_directory(void) {
	char sram_path[512];
	char rtc_path[512];
	char state_path[512];

	const char* saves_dir = "/mnt/SDCARD/.userdata/miyoomini/gambatte";
	const char* game = "Pokemon Crystal";

	MinArch_getSRAMPath(sram_path, saves_dir, game);
	MinArch_getRTCPath(rtc_path, saves_dir, game);
	MinArch_getStatePath(state_path, saves_dir, game, 0);

	// All should be in the same directory
	TEST_ASSERT_TRUE(strstr(sram_path, saves_dir) == sram_path);
	TEST_ASSERT_TRUE(strstr(rtc_path, saves_dir) == rtc_path);
	TEST_ASSERT_TRUE(strstr(state_path, saves_dir) == state_path);

	// All should have different extensions
	TEST_ASSERT_TRUE(strstr(sram_path, ".sav") != NULL);
	TEST_ASSERT_TRUE(strstr(rtc_path, ".rtc") != NULL);
	TEST_ASSERT_TRUE(strstr(state_path, ".st0") != NULL);
}

void test_config_paths_distinguish_game_and_global(void) {
	char game_cfg[512];
	char global_cfg[512];

	MinArch_getConfigPath(game_cfg, "/cfg", "MyGame", NULL);
	MinArch_getConfigPath(global_cfg, "/cfg", NULL, NULL);

	// Should be different paths
	TEST_ASSERT_NOT_EQUAL(0, strcmp(game_cfg, global_cfg));

	// Game config has game name
	TEST_ASSERT_TRUE(strstr(game_cfg, "MyGame") != NULL);

	// Global config has "minarch"
	TEST_ASSERT_TRUE(strstr(global_cfg, "minarch") != NULL);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// SRAM paths
	RUN_TEST(test_getSRAMPath_generates_correct_path);
	RUN_TEST(test_getSRAMPath_handles_special_characters);
	RUN_TEST(test_getSRAMPath_handles_short_names);

	// RTC paths
	RUN_TEST(test_getRTCPath_generates_correct_path);
	RUN_TEST(test_getRTCPath_different_from_SRAM);

	// Save state paths
	RUN_TEST(test_getStatePath_generates_path_for_slot_0);
	RUN_TEST(test_getStatePath_generates_path_for_slot_9);
	RUN_TEST(test_getStatePath_all_slots_unique);
	RUN_TEST(test_getStatePath_handles_long_game_names);

	// Config paths
	RUN_TEST(test_getConfigPath_global_no_device_tag);
	RUN_TEST(test_getConfigPath_global_with_device_tag);
	RUN_TEST(test_getConfigPath_game_specific_no_device_tag);
	RUN_TEST(test_getConfigPath_game_specific_with_device_tag);
	RUN_TEST(test_getConfigPath_empty_device_tag_treated_as_null);

	// Integration tests
	RUN_TEST(test_all_save_files_in_same_directory);
	RUN_TEST(test_config_paths_distinguish_game_and_global);

	return UNITY_END();
}
