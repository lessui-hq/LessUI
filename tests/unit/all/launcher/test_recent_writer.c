/**
 * test_recent_writer.c - Tests for Recent_save() function
 *
 * Tests writing recent.txt files using real temp files.
 * Compiled WITHOUT --wrap flags to use real file I/O.
 */

#include "unity.h"
#include "recent_file.h"
#include "test_temp.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {
	// Nothing to set up - temp files created as needed
}

void tearDown(void) {
	test_temp_cleanup(); // Clean up any temp files created during test
}

void test_Recent_save_single_entry_no_alias(void) {
	const char* temp_path = test_temp_file(NULL);
	TEST_ASSERT_NOT_NULL(temp_path);

	// Create entry
	Recent_Entry entry1 = {.path = "/Roms/GB/mario.gb", .alias = NULL};
	Recent_Entry* entries[] = {&entry1};

	// Save
	int result = Recent_save(temp_path, entries, 1);
	TEST_ASSERT_TRUE(result);

	// Read back and verify
	FILE* f = fopen(temp_path, "r");
	TEST_ASSERT_NOT_NULL(f);
	char line[256];
	fgets(line, sizeof(line), f);
	fclose(f);

	TEST_ASSERT_EQUAL_STRING("/Roms/GB/mario.gb\n", line);
}

void test_Recent_save_single_entry_with_alias(void) {
	const char* temp_path = test_temp_file(NULL);
	TEST_ASSERT_NOT_NULL(temp_path);

	Recent_Entry entry1 = {.path = "/Roms/GB/mario.gb", .alias = "Super Mario"};
	Recent_Entry* entries[] = {&entry1};

	int result = Recent_save(temp_path, entries, 1);
	TEST_ASSERT_TRUE(result);

	FILE* f = fopen(temp_path, "r");
	TEST_ASSERT_NOT_NULL(f);
	char line[256];
	fgets(line, sizeof(line), f);
	fclose(f);

	TEST_ASSERT_EQUAL_STRING("/Roms/GB/mario.gb\tSuper Mario\n", line);
}

void test_Recent_save_multiple_entries_mixed(void) {
	const char* temp_path = test_temp_file(NULL);
	TEST_ASSERT_NOT_NULL(temp_path);

	Recent_Entry entry1 = {.path = "/Roms/GB/mario.gb", .alias = "Super Mario"};
	Recent_Entry entry2 = {.path = "/Roms/NES/zelda.nes", .alias = NULL};
	Recent_Entry entry3 = {.path = "/Roms/SNES/metroid.smc", .alias = "Metroid"};
	Recent_Entry* entries[] = {&entry1, &entry2, &entry3};

	int result = Recent_save(temp_path, entries, 3);
	TEST_ASSERT_TRUE(result);

	FILE* f = fopen(temp_path, "r");
	TEST_ASSERT_NOT_NULL(f);
	char line1[256], line2[256], line3[256];
	fgets(line1, sizeof(line1), f);
	fgets(line2, sizeof(line2), f);
	fgets(line3, sizeof(line3), f);
	fclose(f);

	TEST_ASSERT_EQUAL_STRING("/Roms/GB/mario.gb\tSuper Mario\n", line1);
	TEST_ASSERT_EQUAL_STRING("/Roms/NES/zelda.nes\n", line2);
	TEST_ASSERT_EQUAL_STRING("/Roms/SNES/metroid.smc\tMetroid\n", line3);
}

void test_Recent_save_empty_array(void) {
	const char* temp_path = test_temp_file(NULL);
	TEST_ASSERT_NOT_NULL(temp_path);

	// Save empty array
	int result = Recent_save(temp_path, NULL, 0);
	TEST_ASSERT_TRUE(result);

	// File should exist but be empty
	FILE* f = fopen(temp_path, "r");
	TEST_ASSERT_NOT_NULL(f);
	char line[256];
	char* res = fgets(line, sizeof(line), f);
	fclose(f);

	TEST_ASSERT_NULL(res); // Empty file
}

void test_Recent_save_file_open_failure(void) {
	// Try to save to invalid path
	Recent_Entry entry1 = {.path = "/Roms/GB/mario.gb", .alias = NULL};
	Recent_Entry* entries[] = {&entry1};

	int result = Recent_save("/nonexistent/path/recent.txt", entries, 1);
	TEST_ASSERT_FALSE(result);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_Recent_save_single_entry_no_alias);
	RUN_TEST(test_Recent_save_single_entry_with_alias);
	RUN_TEST(test_Recent_save_multiple_entries_mixed);
	RUN_TEST(test_Recent_save_empty_array);
	RUN_TEST(test_Recent_save_file_open_failure);

	return UNITY_END();
}
