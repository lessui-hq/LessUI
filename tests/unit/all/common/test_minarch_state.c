/**
 * test_minarch_state.c - Unit tests for save state system
 *
 * Tests save state read/write functions using mock core callbacks
 * and real temp files for I/O.
 *
 * Test coverage:
 * - State read/write with mock core
 * - Auto-save to slot 9
 * - Resume from specified slot
 * - Error handling
 */

#include "../../../support/unity/unity.h"
#include "minarch_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// Test temp directory and file path
static char test_dir[256];
static char test_path[256];

// Mock state buffer
#define MOCK_STATE_SIZE 4096
static uint8_t mock_state_buffer[MOCK_STATE_SIZE];
static size_t mock_state_size = MOCK_STATE_SIZE;
static int serialize_fail = 0;
static int unserialize_fail = 0;

// Track what was passed to serialize/unserialize
static uint8_t last_serialized[MOCK_STATE_SIZE];
static uint8_t last_unserialized[MOCK_STATE_SIZE];
static size_t last_serialize_size = 0;
static size_t last_unserialize_size = 0;

///////////////////////////////
// Mock Core Callbacks
///////////////////////////////

static size_t mock_serialize_size(void) {
	return mock_state_size;
}

static bool mock_serialize(void* data, size_t size) {
	if (serialize_fail)
		return false;

	// Copy mock state to output buffer
	memcpy(data, mock_state_buffer, size);
	memcpy(last_serialized, mock_state_buffer, size);
	last_serialize_size = size;
	return true;
}

static bool mock_unserialize(const void* data, size_t size) {
	if (unserialize_fail)
		return false;

	// Copy input to mock state and tracking buffer
	memcpy(mock_state_buffer, data, size);
	memcpy(last_unserialized, data, size);
	last_unserialize_size = size;
	return true;
}

static MinArchStateCore test_core = {.serialize_size = mock_serialize_size,
                                     .serialize = mock_serialize,
                                     .unserialize = mock_unserialize};

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	// Create temp directory
	strcpy(test_dir, "/tmp/test_state_XXXXXX");
	char* dir = mkdtemp(test_dir);
	TEST_ASSERT_NOT_NULL(dir);

	// Create temp file path
	strcpy(test_path, test_dir);
	strcat(test_path, "/test.st0");

	// Reset mock state
	memset(mock_state_buffer, 0, sizeof(mock_state_buffer));
	memset(last_serialized, 0, sizeof(last_serialized));
	memset(last_unserialized, 0, sizeof(last_unserialized));
	mock_state_size = MOCK_STATE_SIZE;
	serialize_fail = 0;
	unserialize_fail = 0;
	last_serialize_size = 0;
	last_unserialize_size = 0;
}

void tearDown(void) {
	// Remove test files
	unlink(test_path);

	// Remove other slot files that might have been created
	char slot_path[512];
	for (int i = 0; i <= 9; i++) {
		snprintf(slot_path, sizeof(slot_path), "%s/TestGame.st%d", test_dir, i);
		unlink(slot_path);
	}

	rmdir(test_dir);
}

///////////////////////////////
// Helper Functions
///////////////////////////////

static void write_test_file(const char* path, const void* data, size_t size) {
	FILE* f = fopen(path, "wb");
	if (f) {
		fwrite(data, 1, size, f);
		fclose(f);
	}
}

static size_t read_test_file(const char* path, void* buffer, size_t max_size) {
	FILE* f = fopen(path, "rb");
	if (!f)
		return 0;
	size_t read = fread(buffer, 1, max_size, f);
	fclose(f);
	return read;
}

///////////////////////////////
// State Write Tests
///////////////////////////////

void test_writeState_writes_to_file(void) {
	// Fill mock state with test pattern
	for (size_t i = 0; i < mock_state_size; i++) {
		mock_state_buffer[i] = (uint8_t)(i & 0xFF);
	}

	MinArchStateResult result = MinArchState_write(test_path, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	// Verify file contents
	uint8_t read_buffer[MOCK_STATE_SIZE];
	size_t read = read_test_file(test_path, read_buffer, sizeof(read_buffer));
	TEST_ASSERT_EQUAL(mock_state_size, read);
	TEST_ASSERT_EQUAL_MEMORY(mock_state_buffer, read_buffer, mock_state_size);
}

void test_writeState_returns_no_support_when_size_zero(void) {
	mock_state_size = 0;

	MinArchStateResult result = MinArchState_write(test_path, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_NO_SUPPORT, result);
}

void test_writeState_returns_serialize_error_on_fail(void) {
	serialize_fail = 1;

	MinArchStateResult result = MinArchState_write(test_path, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_SERIALIZE_ERROR, result);
}

void test_writeState_writes_small_state(void) {
	mock_state_size = 64;
	memset(mock_state_buffer, 0xCD, mock_state_size);

	MinArchStateResult result = MinArchState_write(test_path, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	uint8_t read_buffer[64];
	size_t read = read_test_file(test_path, read_buffer, sizeof(read_buffer));
	TEST_ASSERT_EQUAL(64, read);
	TEST_ASSERT_EACH_EQUAL_UINT8(0xCD, read_buffer, 64);
}

///////////////////////////////
// State Read Tests
///////////////////////////////

void test_readState_loads_from_file(void) {
	// Create test file with known content
	uint8_t test_data[MOCK_STATE_SIZE];
	for (size_t i = 0; i < sizeof(test_data); i++) {
		test_data[i] = (uint8_t)((i * 5) & 0xFF);
	}
	write_test_file(test_path, test_data, sizeof(test_data));

	MinArchStateResult result = MinArchState_read(test_path, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);
	TEST_ASSERT_EQUAL_MEMORY(test_data, mock_state_buffer, mock_state_size);
	TEST_ASSERT_EQUAL(mock_state_size, last_unserialize_size);
}

void test_readState_returns_file_not_found(void) {
	MinArchStateResult result = MinArchState_read("/nonexistent/path.st0", &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_FILE_NOT_FOUND, result);
}

void test_readState_returns_no_support_when_size_zero(void) {
	mock_state_size = 0;

	MinArchStateResult result = MinArchState_read(test_path, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_NO_SUPPORT, result);
}

void test_readState_returns_serialize_error_on_fail(void) {
	// Create a valid file
	uint8_t test_data[MOCK_STATE_SIZE];
	memset(test_data, 0xAA, sizeof(test_data));
	write_test_file(test_path, test_data, sizeof(test_data));

	unserialize_fail = 1;

	MinArchStateResult result = MinArchState_read(test_path, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_SERIALIZE_ERROR, result);
}

///////////////////////////////
// Auto-Save Tests
///////////////////////////////

void test_autoSave_saves_to_slot_9(void) {
	// Fill mock state with test pattern
	for (size_t i = 0; i < mock_state_size; i++) {
		mock_state_buffer[i] = (uint8_t)(i ^ 0x55);
	}

	MinArchStateResult result = MinArchState_autoSave(test_dir, "TestGame", &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	// Verify file was created with correct name
	char expected_path[512];
	snprintf(expected_path, sizeof(expected_path), "%s/TestGame.st%d", test_dir,
	         MINARCH_AUTO_RESUME_SLOT);

	uint8_t read_buffer[MOCK_STATE_SIZE];
	size_t read = read_test_file(expected_path, read_buffer, sizeof(read_buffer));
	TEST_ASSERT_EQUAL(mock_state_size, read);
	TEST_ASSERT_EQUAL_MEMORY(mock_state_buffer, read_buffer, mock_state_size);
}

void test_autoSave_uses_correct_slot_number(void) {
	mock_state_size = 32;
	memset(mock_state_buffer, 0xBB, mock_state_size);

	MinArchStateResult result = MinArchState_autoSave(test_dir, "Game", &test_core);
	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	// Verify the auto-resume slot is 9
	TEST_ASSERT_EQUAL(9, MINARCH_AUTO_RESUME_SLOT);

	// Check file exists
	char path[512];
	snprintf(path, sizeof(path), "%s/Game.st9", test_dir);
	FILE* f = fopen(path, "rb");
	TEST_ASSERT_NOT_NULL(f);
	fclose(f);
}

///////////////////////////////
// Resume State Tests
///////////////////////////////

void test_resumeState_loads_from_specified_slot(void) {
	// Create state file in slot 3
	uint8_t test_data[MOCK_STATE_SIZE];
	for (size_t i = 0; i < sizeof(test_data); i++) {
		test_data[i] = (uint8_t)(i * 3);
	}

	char slot_path[512];
	snprintf(slot_path, sizeof(slot_path), "%s/TestGame.st3", test_dir);
	write_test_file(slot_path, test_data, sizeof(test_data));

	MinArchStateResult result = MinArchState_resume(test_dir, "TestGame", 3, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);
	TEST_ASSERT_EQUAL_MEMORY(test_data, mock_state_buffer, mock_state_size);
}

void test_resumeState_returns_file_not_found_for_empty_slot(void) {
	// Don't create any file - slot 5 doesn't exist
	MinArchStateResult result = MinArchState_resume(test_dir, "TestGame", 5, &test_core);

	TEST_ASSERT_EQUAL(MINARCH_STATE_FILE_NOT_FOUND, result);
}

void test_resumeState_handles_all_slots(void) {
	// Test that we can load from any slot 0-9
	for (int slot = 0; slot <= 9; slot++) {
		// Create state file for this slot
		uint8_t test_data[32];
		mock_state_size = 32;
		memset(test_data, (uint8_t)slot, sizeof(test_data));

		char slot_path[512];
		snprintf(slot_path, sizeof(slot_path), "%s/Game.st%d", test_dir, slot);
		write_test_file(slot_path, test_data, sizeof(test_data));

		MinArchStateResult result = MinArchState_resume(test_dir, "Game", slot, &test_core);
		TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

		// Verify correct data was loaded
		TEST_ASSERT_EACH_EQUAL_UINT8((uint8_t)slot, mock_state_buffer, mock_state_size);
	}
}

///////////////////////////////
// Result String Tests
///////////////////////////////

void test_stateResultString_returns_descriptions(void) {
	TEST_ASSERT_EQUAL_STRING("Success", MinArchState_resultString(MINARCH_STATE_OK));
	TEST_ASSERT_EQUAL_STRING("Core does not support save states",
	                         MinArchState_resultString(MINARCH_STATE_NO_SUPPORT));
	TEST_ASSERT_EQUAL_STRING("State file not found",
	                         MinArchState_resultString(MINARCH_STATE_FILE_NOT_FOUND));
	TEST_ASSERT_EQUAL_STRING("File I/O error", MinArchState_resultString(MINARCH_STATE_FILE_ERROR));
	TEST_ASSERT_EQUAL_STRING("Memory allocation failed",
	                         MinArchState_resultString(MINARCH_STATE_ALLOC_ERROR));
	TEST_ASSERT_EQUAL_STRING("Core serialization failed",
	                         MinArchState_resultString(MINARCH_STATE_SERIALIZE_ERROR));
}

///////////////////////////////
// Round-trip Tests
///////////////////////////////

void test_state_write_then_read_roundtrip(void) {
	// Write with pattern
	for (size_t i = 0; i < mock_state_size; i++) {
		mock_state_buffer[i] = (uint8_t)(i ^ 0x99);
	}

	MinArchStateResult result = MinArchState_write(test_path, &test_core);
	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	// Clear buffer
	uint8_t original[MOCK_STATE_SIZE];
	memcpy(original, mock_state_buffer, mock_state_size);
	memset(mock_state_buffer, 0, mock_state_size);

	// Read back
	result = MinArchState_read(test_path, &test_core);
	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	// Should match original
	TEST_ASSERT_EQUAL_MEMORY(original, mock_state_buffer, mock_state_size);
}

void test_autosave_then_resume_roundtrip(void) {
	// Auto-save
	mock_state_size = 128;
	for (size_t i = 0; i < mock_state_size; i++) {
		mock_state_buffer[i] = (uint8_t)(0xFF - i);
	}

	MinArchStateResult result = MinArchState_autoSave(test_dir, "MyGame", &test_core);
	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	// Clear buffer and resume
	uint8_t original[128];
	memcpy(original, mock_state_buffer, mock_state_size);
	memset(mock_state_buffer, 0, mock_state_size);

	result = MinArchState_resume(test_dir, "MyGame", MINARCH_AUTO_RESUME_SLOT, &test_core);
	TEST_ASSERT_EQUAL(MINARCH_STATE_OK, result);

	TEST_ASSERT_EQUAL_MEMORY(original, mock_state_buffer, mock_state_size);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// State write tests
	RUN_TEST(test_writeState_writes_to_file);
	RUN_TEST(test_writeState_returns_no_support_when_size_zero);
	RUN_TEST(test_writeState_returns_serialize_error_on_fail);
	RUN_TEST(test_writeState_writes_small_state);

	// State read tests
	RUN_TEST(test_readState_loads_from_file);
	RUN_TEST(test_readState_returns_file_not_found);
	RUN_TEST(test_readState_returns_no_support_when_size_zero);
	RUN_TEST(test_readState_returns_serialize_error_on_fail);

	// Auto-save tests
	RUN_TEST(test_autoSave_saves_to_slot_9);
	RUN_TEST(test_autoSave_uses_correct_slot_number);

	// Resume state tests
	RUN_TEST(test_resumeState_loads_from_specified_slot);
	RUN_TEST(test_resumeState_returns_file_not_found_for_empty_slot);
	RUN_TEST(test_resumeState_handles_all_slots);

	// Result string tests
	RUN_TEST(test_stateResultString_returns_descriptions);

	// Round-trip tests
	RUN_TEST(test_state_write_then_read_roundtrip);
	RUN_TEST(test_autosave_then_resume_roundtrip);

	return UNITY_END();
}
