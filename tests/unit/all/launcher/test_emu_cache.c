/**
 * Test suite for workspace/all/launcher/launcher_emu_cache.c
 *
 * Tests the emulator availability cache that eliminates repeated
 * filesystem checks during root menu generation.
 */

#include "launcher_emu_cache.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Test fixture paths
static char test_dir[512];
static char paks_path[512];
static char sdcard_path[512];

void setUp(void) {
	// Create unique temp directory for each test
	snprintf(test_dir, sizeof(test_dir), "/tmp/emu_cache_test_XXXXXX");
	char* result = mkdtemp(test_dir);
	TEST_ASSERT_NOT_NULL_MESSAGE(result, "Failed to create temp directory");

	// Set up paths
	snprintf(paks_path, sizeof(paks_path), "%s/paks", test_dir);
	snprintf(sdcard_path, sizeof(sdcard_path), "%s/sdcard", test_dir);
}

void tearDown(void) {
	// Clean up cache
	EmuCache_free();

	// Remove test directory and contents
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
	int ret = system(cmd);
	(void)ret;
}

// Helper to create a pak directory with launch.sh
static void createPak(const char* base_path, const char* emu_name) {
	char pak_dir[512];
	char launch_path[512];

	snprintf(pak_dir, sizeof(pak_dir), "%s/Emus/%s.pak", base_path, emu_name);

	// Create directories recursively
	char mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", pak_dir);
	int ret = system(mkdir_cmd);
	(void)ret;

	// Create launch.sh
	snprintf(launch_path, sizeof(launch_path), "%s/launch.sh", pak_dir);
	FILE* f = fopen(launch_path, "w");
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to create launch.sh");
	fprintf(f, "#!/bin/sh\necho test\n");
	fclose(f);
}

// Helper to create platform-specific pak
static void createPlatformPak(const char* sdcard, const char* platform, const char* emu_name) {
	char pak_dir[512];
	char launch_path[512];

	snprintf(pak_dir, sizeof(pak_dir), "%s/Emus/%s/%s.pak", sdcard, platform, emu_name);

	char mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", pak_dir);
	int ret = system(mkdir_cmd);
	(void)ret;

	snprintf(launch_path, sizeof(launch_path), "%s/launch.sh", pak_dir);
	FILE* f = fopen(launch_path, "w");
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to create launch.sh");
	fprintf(f, "#!/bin/sh\necho test\n");
	fclose(f);
}

///////////////////////////////
// Basic functionality tests
///////////////////////////////

void test_cache_not_initialized(void) {
	// Without init, hasEmu should return 0
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu("gpsp"));
	TEST_ASSERT_EQUAL_INT(0, EmuCache_count());
}

void test_cache_init_empty_dirs(void) {
	// Create empty Emus directories
	char mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s/Emus %s/Emus/testplatform",
	         paks_path, sdcard_path);
	int ret = system(mkdir_cmd);
	(void)ret;

	int count = EmuCache_init(paks_path, sdcard_path, "testplatform");
	TEST_ASSERT_EQUAL_INT(0, count);
	TEST_ASSERT_EQUAL_INT(0, EmuCache_count());
}

void test_cache_init_nonexistent_dirs(void) {
	// Init with directories that don't exist should work (return 0)
	int count = EmuCache_init("/nonexistent/path", "/also/nonexistent", "platform");
	TEST_ASSERT_EQUAL_INT(0, count);
}

void test_cache_finds_shared_emu(void) {
	createPak(paks_path, "gpsp");

	int count = EmuCache_init(paks_path, sdcard_path, "testplatform");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("gpsp"));
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu("gambatte"));
}

void test_cache_finds_platform_emu(void) {
	createPlatformPak(sdcard_path, "miyoomini", "picodrive");

	int count = EmuCache_init(paks_path, sdcard_path, "miyoomini");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("picodrive"));
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu("gpsp"));
}

void test_cache_finds_both_locations(void) {
	createPak(paks_path, "gpsp");
	createPak(paks_path, "gambatte");
	createPlatformPak(sdcard_path, "miyoomini", "picodrive");
	createPlatformPak(sdcard_path, "miyoomini", "snes9x");

	int count = EmuCache_init(paks_path, sdcard_path, "miyoomini");
	TEST_ASSERT_EQUAL_INT(4, count);
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("gpsp"));
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("gambatte"));
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("picodrive"));
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("snes9x"));
}

void test_cache_ignores_wrong_platform(void) {
	createPlatformPak(sdcard_path, "miyoomini", "picodrive");
	createPlatformPak(sdcard_path, "trimuismart", "snes9x");

	// Init for miyoomini should only find picodrive
	int count = EmuCache_init(paks_path, sdcard_path, "miyoomini");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("picodrive"));
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu("snes9x"));
}

///////////////////////////////
// Edge cases
///////////////////////////////

void test_cache_ignores_pak_without_launch_sh(void) {
	// Create pak directory without launch.sh
	char pak_dir[512];
	snprintf(pak_dir, sizeof(pak_dir), "%s/Emus/broken.pak", paks_path);
	char mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", pak_dir);
	int ret = system(mkdir_cmd);
	(void)ret;

	// Also create a valid pak for comparison
	createPak(paks_path, "valid");

	int count = EmuCache_init(paks_path, sdcard_path, "testplatform");
	TEST_ASSERT_EQUAL_INT(1, count);  // Only valid pak counted
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu("broken"));
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("valid"));
}

void test_cache_ignores_hidden_directories(void) {
	createPak(paks_path, ".hidden");
	createPak(paks_path, "visible");

	int count = EmuCache_init(paks_path, sdcard_path, "testplatform");
	TEST_ASSERT_EQUAL_INT(1, count);  // Only visible
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu(".hidden"));
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("visible"));
}

void test_cache_ignores_non_pak_directories(void) {
	// Create a directory without .pak suffix
	char dir[512];
	snprintf(dir, sizeof(dir), "%s/Emus/notapak", paks_path);
	char mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", dir);
	int ret = system(mkdir_cmd);
	(void)ret;

	// Create launch.sh inside (to prove it checks suffix, not just launch.sh)
	char launch_path[512];
	snprintf(launch_path, sizeof(launch_path), "%s/launch.sh", dir);
	FILE* f = fopen(launch_path, "w");
	fprintf(f, "#!/bin/sh\n");
	fclose(f);

	createPak(paks_path, "realpak");

	int count = EmuCache_init(paks_path, sdcard_path, "testplatform");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu("notapak"));
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("realpak"));
}

void test_cache_null_emu_name(void) {
	createPak(paks_path, "gpsp");
	EmuCache_init(paks_path, sdcard_path, "testplatform");

	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu(NULL));
}

void test_cache_free_safe_multiple_calls(void) {
	// Should be safe to call free multiple times
	EmuCache_free();
	EmuCache_free();
	EmuCache_free();

	// Should also be safe before init
	TEST_ASSERT_EQUAL_INT(0, EmuCache_hasEmu("test"));
}

void test_cache_reinit(void) {
	createPak(paks_path, "gpsp");

	int count1 = EmuCache_init(paks_path, sdcard_path, "testplatform");
	TEST_ASSERT_EQUAL_INT(1, count1);
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("gpsp"));

	// Add another pak and reinit
	createPak(paks_path, "gambatte");

	int count2 = EmuCache_init(paks_path, sdcard_path, "testplatform");
	TEST_ASSERT_EQUAL_INT(2, count2);
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("gpsp"));
	TEST_ASSERT_EQUAL_INT(1, EmuCache_hasEmu("gambatte"));
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic functionality
	RUN_TEST(test_cache_not_initialized);
	RUN_TEST(test_cache_init_empty_dirs);
	RUN_TEST(test_cache_init_nonexistent_dirs);
	RUN_TEST(test_cache_finds_shared_emu);
	RUN_TEST(test_cache_finds_platform_emu);
	RUN_TEST(test_cache_finds_both_locations);
	RUN_TEST(test_cache_ignores_wrong_platform);

	// Edge cases
	RUN_TEST(test_cache_ignores_pak_without_launch_sh);
	RUN_TEST(test_cache_ignores_hidden_directories);
	RUN_TEST(test_cache_ignores_non_pak_directories);
	RUN_TEST(test_cache_null_emu_name);
	RUN_TEST(test_cache_free_safe_multiple_calls);
	RUN_TEST(test_cache_reinit);

	return UNITY_END();
}
