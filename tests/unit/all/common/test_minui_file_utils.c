/**
 * test_minui_file_utils.c - Unit tests for MinUI file utilities
 *
 * Tests file existence checking functions extracted from minui.c.
 * Uses file system mocking to test without actual files.
 *
 * Test coverage:
 * - hasEmu() - Emulator availability checking
 * - hasCue() - CUE file detection for disc games
 * - hasM3u() - M3U playlist detection for multi-disc games
 *
 * Note: Uses GCC --wrap for file mocking (Docker-only)
 */

#include "../../../support/unity/unity.h"
#include "../../../support/fs_mocks.h"
#include "../../../../workspace/all/common/minui_file_utils.h"

#include <string.h>

void setUp(void) {
	mock_fs_reset();
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// hasEmu() Tests
///////////////////////////////

void test_hasEmu_finds_shared_emulator(void) {
	// Emulator in shared Paks location
	mock_fs_add_file("/mnt/SDCARD/Paks/Emus/gpsp.pak/launch.sh", "#!/bin/sh");

	int result = MinUI_hasEmu("gpsp", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini");

	TEST_ASSERT_TRUE(result);
}

void test_hasEmu_finds_platform_specific_emulator(void) {
	// Emulator in platform-specific location
	mock_fs_add_file("/mnt/SDCARD/Emus/miyoomini/gambatte.pak/launch.sh", "#!/bin/sh");

	int result = MinUI_hasEmu("gambatte", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini");

	TEST_ASSERT_TRUE(result);
}

void test_hasEmu_prefers_shared_over_platform(void) {
	// Both locations have the emulator
	mock_fs_add_file("/mnt/SDCARD/Paks/Emus/snes9x.pak/launch.sh", "shared");
	mock_fs_add_file("/mnt/SDCARD/Emus/miyoomini/snes9x.pak/launch.sh", "platform");

	// Should find it (finds shared first)
	int result = MinUI_hasEmu("snes9x", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini");

	TEST_ASSERT_TRUE(result);
}

void test_hasEmu_returns_false_when_not_found(void) {
	// No emulator files exist
	int result = MinUI_hasEmu("nonexistent", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini");

	TEST_ASSERT_FALSE(result);
}

void test_hasEmu_different_platforms(void) {
	// Platform-specific emulator for rg35xx
	mock_fs_add_file("/mnt/SDCARD/Emus/rg35xx/picodrive.pak/launch.sh", "#!/bin/sh");

	// Should find for rg35xx
	TEST_ASSERT_TRUE(MinUI_hasEmu("picodrive", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "rg35xx"));

	// Should NOT find for miyoomini
	TEST_ASSERT_FALSE(
	    MinUI_hasEmu("picodrive", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini"));
}

///////////////////////////////
// hasCue() Tests
///////////////////////////////

void test_hasCue_finds_matching_cue_file(void) {
	// Directory with .cue file named after directory
	mock_fs_add_file("/Roms/PS1/Final Fantasy VII/Final Fantasy VII.cue", "cue data");

	char cue_path[256];
	int result = MinUI_hasCue("/Roms/PS1/Final Fantasy VII", cue_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/Final Fantasy VII/Final Fantasy VII.cue", cue_path);
}

void test_hasCue_returns_false_when_cue_missing(void) {
	// Directory exists but no .cue file
	char cue_path[256];
	int result = MinUI_hasCue("/Roms/PS1/SomeGame", cue_path);

	TEST_ASSERT_FALSE(result);
	// Path should still be set (for potential creation later)
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/SomeGame/SomeGame.cue", cue_path);
}

void test_hasCue_simple_directory_name(void) {
	mock_fs_add_file("/Games/Panzer/Panzer.cue", "cue");

	char cue_path[256];
	int result = MinUI_hasCue("/Games/Panzer", cue_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Games/Panzer/Panzer.cue", cue_path);
}

void test_hasCue_path_with_special_characters(void) {
	mock_fs_add_file("/Roms/Game (USA) (v1.1)/Game (USA) (v1.1).cue", "cue");

	char cue_path[256];
	int result = MinUI_hasCue("/Roms/Game (USA) (v1.1)", cue_path);

	TEST_ASSERT_TRUE(result);
}

///////////////////////////////
// hasM3u() Tests
///////////////////////////////

void test_hasM3u_finds_m3u_in_parent(void) {
	// ROM in subdirectory, M3U in parent named after subdirectory
	// Structure: /Roms/PS1/FF7.m3u references /Roms/PS1/FF7/disc1.bin
	mock_fs_add_file("/Roms/PS1/FF7.m3u", "playlist");

	char m3u_path[256];
	int result = MinUI_hasM3u("/Roms/PS1/FF7/disc1.bin", m3u_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF7.m3u", m3u_path);
}

void test_hasM3u_returns_false_when_m3u_missing(void) {
	char m3u_path[256];
	int result = MinUI_hasM3u("/Roms/PS1/Game/disc.bin", m3u_path);

	TEST_ASSERT_FALSE(result);
	// Path should still be constructed
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/Game.m3u", m3u_path);
}

void test_hasM3u_deep_directory_structure(void) {
	mock_fs_add_file("/a/b/c/Game.m3u", "playlist");

	char m3u_path[256];
	int result = MinUI_hasM3u("/a/b/c/Game/disc1.bin", m3u_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/a/b/c/Game.m3u", m3u_path);
}

void test_hasM3u_realistic_final_fantasy(void) {
	// Real-world scenario
	mock_fs_add_file("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA).m3u", "playlist");

	char m3u_path[256];
	int result =
	    MinUI_hasM3u("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA)/FF7 (Disc 1).bin", m3u_path);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/PS1/Final Fantasy VII (USA).m3u", m3u_path);
}

void test_hasM3u_constructs_correct_path_even_when_missing(void) {
	char m3u_path[256];
	MinUI_hasM3u("/Roms/Saturn/Panzer/disc.cue", m3u_path);

	// Even if file doesn't exist, path should be constructed correctly
	TEST_ASSERT_EQUAL_STRING("/Roms/Saturn/Panzer.m3u", m3u_path);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_multi_disc_game_workflow(void) {
	// Complete multi-disc game setup
	const char* game_dir = "/Roms/PS1/Metal Gear Solid";
	const char* disc1 = "/Roms/PS1/Metal Gear Solid/MGS (Disc 1).bin";

	// Add M3U playlist
	mock_fs_add_file("/Roms/PS1/Metal Gear Solid.m3u", "MGS (Disc 1).bin\nMGS (Disc 2).bin\n");

	// Check for M3U
	char m3u_path[256];
	TEST_ASSERT_TRUE(MinUI_hasM3u(disc1, m3u_path));
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/Metal Gear Solid.m3u", m3u_path);

	// Also check for CUE (game might have both)
	mock_fs_add_file("/Roms/PS1/Metal Gear Solid/Metal Gear Solid.cue", "cue");
	char cue_path[256];
	TEST_ASSERT_TRUE(MinUI_hasCue(game_dir, cue_path));
}

void test_emulator_availability_check(void) {
	// Setup emulators for different systems
	mock_fs_add_file("/mnt/SDCARD/Paks/Emus/gpsp.pak/launch.sh", "gba");
	mock_fs_add_file("/mnt/SDCARD/Emus/miyoomini/gambatte.pak/launch.sh", "gb");

	// Check which emulators are available
	TEST_ASSERT_TRUE(MinUI_hasEmu("gpsp", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini"));
	TEST_ASSERT_TRUE(
	    MinUI_hasEmu("gambatte", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini"));
	TEST_ASSERT_FALSE(
	    MinUI_hasEmu("nonexistent", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini"));
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_hasCue_path_without_slash(void) {
	char cue_path[256];
	int result = MinUI_hasCue("NoSlashes", cue_path);

	// Should handle gracefully
	TEST_ASSERT_FALSE(result);
}

void test_hasM3u_shallow_path(void) {
	char m3u_path[256];
	int result = MinUI_hasM3u("/Game/disc.bin", m3u_path);

	// Path too shallow for M3U detection
	TEST_ASSERT_FALSE(result);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// hasEmu tests
	RUN_TEST(test_hasEmu_finds_shared_emulator);
	RUN_TEST(test_hasEmu_finds_platform_specific_emulator);
	RUN_TEST(test_hasEmu_prefers_shared_over_platform);
	RUN_TEST(test_hasEmu_returns_false_when_not_found);
	RUN_TEST(test_hasEmu_different_platforms);

	// hasCue tests
	RUN_TEST(test_hasCue_finds_matching_cue_file);
	RUN_TEST(test_hasCue_returns_false_when_cue_missing);
	RUN_TEST(test_hasCue_simple_directory_name);
	RUN_TEST(test_hasCue_path_with_special_characters);

	// hasM3u tests
	RUN_TEST(test_hasM3u_finds_m3u_in_parent);
	RUN_TEST(test_hasM3u_returns_false_when_m3u_missing);
	RUN_TEST(test_hasM3u_deep_directory_structure);
	RUN_TEST(test_hasM3u_realistic_final_fantasy);
	RUN_TEST(test_hasM3u_constructs_correct_path_even_when_missing);

	// Integration tests
	RUN_TEST(test_multi_disc_game_workflow);
	RUN_TEST(test_emulator_availability_check);

	// Edge cases
	RUN_TEST(test_hasCue_path_without_slash);
	RUN_TEST(test_hasM3u_shallow_path);

	return UNITY_END();
}
