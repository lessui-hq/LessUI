/**
 * test_recent_runtime.c - Tests for Recent runtime operations
 *
 * Tests Recent struct creation and array operations.
 *
 * Test coverage:
 * - Recent_new - Create Recent entries with emulator availability check
 * - Recent_free - Memory cleanup
 * - RecentArray_indexOf - Search by path
 * - RecentArray_free - Array cleanup
 */

#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "recent_file.h"
#include "stb_ds.h"
#include <stdlib.h>
#include <string.h>

///////////////////////////////
// Test helpers
///////////////////////////////

// Mock hasEmu function
static int mock_has_emu_result = 1;
static char mock_last_emu_name[256] = {0};

static int mock_hasEmu(char* emu_name) {
	strncpy(mock_last_emu_name, emu_name, sizeof(mock_last_emu_name) - 1);
	return mock_has_emu_result;
}

void setUp(void) {
	mock_has_emu_result = 1;
	mock_last_emu_name[0] = '\0';
}

void tearDown(void) {
}

///////////////////////////////
// Recent_new tests
///////////////////////////////

void test_Recent_new_creates_entry(void) {
	Recent* recent = Recent_new("/Roms/GB/Tetris.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	TEST_ASSERT_NOT_NULL(recent);
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/Tetris.gb", recent->path);
	TEST_ASSERT_NULL(recent->alias);
	TEST_ASSERT_EQUAL(1, recent->available);
	Recent_free(recent);
}

void test_Recent_new_stores_alias(void) {
	Recent* recent = Recent_new("/Roms/GB/Tetris.gb", "My Tetris", "/mnt/SDCARD", mock_hasEmu);
	TEST_ASSERT_NOT_NULL(recent);
	TEST_ASSERT_EQUAL_STRING("My Tetris", recent->alias);
	Recent_free(recent);
}

void test_Recent_new_checks_emulator_availability(void) {
	mock_has_emu_result = 0;
	Recent* recent = Recent_new("/Roms/GB/game.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	TEST_ASSERT_NOT_NULL(recent);
	TEST_ASSERT_EQUAL(0, recent->available);
	Recent_free(recent);
}

void test_Recent_new_calls_has_emu(void) {
	Recent* recent = Recent_new("/Roms/GB/game.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	TEST_ASSERT_NOT_NULL(recent);
	// Verify hasEmu was called (emu_name will be extracted by getEmuName)
	TEST_ASSERT_NOT_EQUAL(0, mock_last_emu_name[0]);
	Recent_free(recent);
}

void test_Recent_new_handles_nested_path(void) {
	Recent* recent = Recent_new("/Roms/PS1/subfolder/game.cue", NULL, "/mnt/SDCARD", mock_hasEmu);
	TEST_ASSERT_NOT_NULL(recent);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS1/subfolder/game.cue", recent->path);
	Recent_free(recent);
}

void test_Recent_new_handles_null_has_emu(void) {
	Recent* recent = Recent_new("/Roms/GB/game.gb", NULL, "/mnt/SDCARD", NULL);
	TEST_ASSERT_NOT_NULL(recent);
	TEST_ASSERT_EQUAL(1, recent->available); // Defaults to available
	Recent_free(recent);
}

///////////////////////////////
// Recent_free tests
///////////////////////////////

void test_Recent_free_handles_null(void) {
	// Should not crash
	Recent_free(NULL);
}

void test_Recent_free_cleans_up_alias(void) {
	Recent* recent = Recent_new("/Roms/GB/game.gb", "Test", "/mnt/SDCARD", mock_hasEmu);
	// Just verify it doesn't crash
	Recent_free(recent);
}

///////////////////////////////
// RecentArray_indexOf tests
///////////////////////////////

void test_RecentArray_indexOf_finds_entry(void) {
	Recent* r1 = Recent_new("/Roms/GB/game1.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	Recent* r2 = Recent_new("/Roms/GB/game2.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	Recent* r3 = Recent_new("/Roms/GB/game3.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	Recent** arr = NULL;
	arrpush(arr, r1);
	arrpush(arr, r2);
	arrpush(arr, r3);

	TEST_ASSERT_EQUAL(0, RecentArray_indexOf(arr, "/Roms/GB/game1.gb"));
	TEST_ASSERT_EQUAL(1, RecentArray_indexOf(arr, "/Roms/GB/game2.gb"));
	TEST_ASSERT_EQUAL(2, RecentArray_indexOf(arr, "/Roms/GB/game3.gb"));

	RecentArray_free(arr);
}

void test_RecentArray_indexOf_returns_negative_for_missing(void) {
	Recent* r1 = Recent_new("/Roms/GB/game1.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	Recent** arr = NULL;
	arrpush(arr, r1);

	TEST_ASSERT_EQUAL(-1, RecentArray_indexOf(arr, "/Roms/GB/notfound.gb"));

	RecentArray_free(arr);
}

void test_RecentArray_indexOf_handles_empty_array(void) {
	TEST_ASSERT_EQUAL(-1, RecentArray_indexOf(NULL, "/Roms/GB/game.gb"));
}

///////////////////////////////
// RecentArray_free tests
///////////////////////////////

void test_RecentArray_free_frees_all_entries(void) {
	Recent* r1 = Recent_new("/Roms/GB/game1.gb", NULL, "/mnt/SDCARD", mock_hasEmu);
	Recent* r2 = Recent_new("/Roms/GB/game2.gb", "Test", "/mnt/SDCARD", mock_hasEmu);
	Recent** arr = NULL;
	arrpush(arr, r1);
	arrpush(arr, r2);

	// Should not crash or leak
	RecentArray_free(arr);
}

void test_RecentArray_free_handles_empty(void) {
	// Should not crash
	RecentArray_free(NULL);
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Recent_new tests
	RUN_TEST(test_Recent_new_creates_entry);
	RUN_TEST(test_Recent_new_stores_alias);
	RUN_TEST(test_Recent_new_checks_emulator_availability);
	RUN_TEST(test_Recent_new_calls_has_emu);
	RUN_TEST(test_Recent_new_handles_nested_path);
	RUN_TEST(test_Recent_new_handles_null_has_emu);

	// Recent_free tests
	RUN_TEST(test_Recent_free_handles_null);
	RUN_TEST(test_Recent_free_cleans_up_alias);

	// RecentArray_indexOf tests
	RUN_TEST(test_RecentArray_indexOf_finds_entry);
	RUN_TEST(test_RecentArray_indexOf_returns_negative_for_missing);
	RUN_TEST(test_RecentArray_indexOf_handles_empty_array);

	// RecentArray_free tests
	RUN_TEST(test_RecentArray_free_frees_all_entries);
	RUN_TEST(test_RecentArray_free_handles_empty);

	return UNITY_END();
}
