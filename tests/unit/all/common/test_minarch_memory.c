/**
 * test_minarch_memory.c - Unit tests for SRAM/RTC persistence
 *
 * Tests memory persistence functions using mock core callbacks
 * and real temp files for I/O.
 *
 * Test coverage:
 * - SRAM read/write with mock core
 * - RTC read/write with mock core
 * - Error handling (no support, file errors, null pointers)
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/minarch_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Test temp file path
static char test_path[256];

// Mock memory buffers
static uint8_t mock_sram_buffer[8192];
static uint8_t mock_rtc_buffer[64];
static size_t mock_sram_size = 0;
static size_t mock_rtc_size = 0;

///////////////////////////////
// Mock Core Callbacks
///////////////////////////////

static size_t mock_get_memory_size(unsigned type) {
	switch (type) {
	case MINARCH_MEMORY_SAVE_RAM:
		return mock_sram_size;
	case MINARCH_MEMORY_RTC:
		return mock_rtc_size;
	default:
		return 0;
	}
}

static void* mock_get_memory_data(unsigned type) {
	switch (type) {
	case MINARCH_MEMORY_SAVE_RAM:
		return mock_sram_size > 0 ? mock_sram_buffer : NULL;
	case MINARCH_MEMORY_RTC:
		return mock_rtc_size > 0 ? mock_rtc_buffer : NULL;
	default:
		return NULL;
	}
}

// Mock that returns NULL pointer
static void* mock_get_memory_data_null(unsigned type) {
	(void)type;
	return NULL;
}

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	// Create temp file path
	strcpy(test_path, "/tmp/test_memory_XXXXXX");
	int fd = mkstemp(test_path);
	if (fd >= 0)
		close(fd);

	// Reset mock state
	memset(mock_sram_buffer, 0, sizeof(mock_sram_buffer));
	memset(mock_rtc_buffer, 0, sizeof(mock_rtc_buffer));
	mock_sram_size = 8192;
	mock_rtc_size = 64;
}

void tearDown(void) {
	unlink(test_path);
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
// SRAM Write Tests
///////////////////////////////

void test_writeSRAM_writes_to_file(void) {
	// Fill mock SRAM with test pattern
	for (size_t i = 0; i < mock_sram_size; i++) {
		mock_sram_buffer[i] = (uint8_t)(i & 0xFF);
	}

	MinArchMemoryResult result =
	    MinArch_writeSRAM(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	// Verify file contents
	uint8_t read_buffer[8192];
	size_t read = read_test_file(test_path, read_buffer, sizeof(read_buffer));
	TEST_ASSERT_EQUAL(mock_sram_size, read);
	TEST_ASSERT_EQUAL_MEMORY(mock_sram_buffer, read_buffer, mock_sram_size);
}

void test_writeSRAM_returns_no_support_when_size_zero(void) {
	mock_sram_size = 0;

	MinArchMemoryResult result =
	    MinArch_writeSRAM(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_NO_SUPPORT, result);
}

void test_writeSRAM_returns_null_pointer_when_data_null(void) {
	MinArchMemoryResult result =
	    MinArch_writeSRAM(test_path, mock_get_memory_size, mock_get_memory_data_null);

	TEST_ASSERT_EQUAL(MINARCH_MEM_NULL_POINTER, result);
}

void test_writeSRAM_writes_small_sram(void) {
	mock_sram_size = 32;
	memset(mock_sram_buffer, 0xAB, mock_sram_size);

	MinArchMemoryResult result =
	    MinArch_writeSRAM(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	uint8_t read_buffer[32];
	size_t read = read_test_file(test_path, read_buffer, sizeof(read_buffer));
	TEST_ASSERT_EQUAL(32, read);
	TEST_ASSERT_EACH_EQUAL_UINT8(0xAB, read_buffer, 32);
}

///////////////////////////////
// SRAM Read Tests
///////////////////////////////

void test_readSRAM_loads_from_file(void) {
	// Create test file with known content
	uint8_t test_data[8192];
	for (size_t i = 0; i < sizeof(test_data); i++) {
		test_data[i] = (uint8_t)((i * 3) & 0xFF);
	}
	write_test_file(test_path, test_data, sizeof(test_data));

	MinArchMemoryResult result =
	    MinArch_readSRAM(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);
	TEST_ASSERT_EQUAL_MEMORY(test_data, mock_sram_buffer, mock_sram_size);
}

void test_readSRAM_returns_file_not_found(void) {
	unlink(test_path); // Ensure file doesn't exist

	MinArchMemoryResult result =
	    MinArch_readSRAM(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_FILE_NOT_FOUND, result);
}

void test_readSRAM_returns_no_support_when_size_zero(void) {
	mock_sram_size = 0;

	MinArchMemoryResult result =
	    MinArch_readSRAM(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_NO_SUPPORT, result);
}

void test_readSRAM_returns_null_pointer_when_data_null(void) {
	// Create a file so we get past file check
	write_test_file(test_path, "test", 4);

	MinArchMemoryResult result =
	    MinArch_readSRAM(test_path, mock_get_memory_size, mock_get_memory_data_null);

	TEST_ASSERT_EQUAL(MINARCH_MEM_NULL_POINTER, result);
}

void test_readSRAM_handles_partial_file(void) {
	// Create file smaller than expected
	uint8_t small_data[100];
	memset(small_data, 0x55, sizeof(small_data));
	write_test_file(test_path, small_data, sizeof(small_data));

	// Clear mock buffer
	memset(mock_sram_buffer, 0, mock_sram_size);

	MinArchMemoryResult result =
	    MinArch_readSRAM(test_path, mock_get_memory_size, mock_get_memory_data);

	// Should succeed (partial reads allowed)
	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	// First 100 bytes should match
	TEST_ASSERT_EACH_EQUAL_UINT8(0x55, mock_sram_buffer, 100);
}

///////////////////////////////
// RTC Write Tests
///////////////////////////////

void test_writeRTC_writes_to_file(void) {
	// Fill mock RTC with test pattern
	for (size_t i = 0; i < mock_rtc_size; i++) {
		mock_rtc_buffer[i] = (uint8_t)(0xFF - i);
	}

	MinArchMemoryResult result =
	    MinArch_writeRTC(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	// Verify file contents
	uint8_t read_buffer[64];
	size_t read = read_test_file(test_path, read_buffer, sizeof(read_buffer));
	TEST_ASSERT_EQUAL(mock_rtc_size, read);
	TEST_ASSERT_EQUAL_MEMORY(mock_rtc_buffer, read_buffer, mock_rtc_size);
}

void test_writeRTC_returns_no_support_when_size_zero(void) {
	mock_rtc_size = 0;

	MinArchMemoryResult result =
	    MinArch_writeRTC(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_NO_SUPPORT, result);
}

///////////////////////////////
// RTC Read Tests
///////////////////////////////

void test_readRTC_loads_from_file(void) {
	// Create test file with known content
	uint8_t test_data[64];
	for (size_t i = 0; i < sizeof(test_data); i++) {
		test_data[i] = (uint8_t)(i * 4);
	}
	write_test_file(test_path, test_data, sizeof(test_data));

	MinArchMemoryResult result =
	    MinArch_readRTC(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);
	TEST_ASSERT_EQUAL_MEMORY(test_data, mock_rtc_buffer, mock_rtc_size);
}

void test_readRTC_returns_file_not_found(void) {
	unlink(test_path);

	MinArchMemoryResult result =
	    MinArch_readRTC(test_path, mock_get_memory_size, mock_get_memory_data);

	TEST_ASSERT_EQUAL(MINARCH_MEM_FILE_NOT_FOUND, result);
}

///////////////////////////////
// Result String Tests
///////////////////////////////

void test_memoryResultString_returns_descriptions(void) {
	TEST_ASSERT_EQUAL_STRING("Success", MinArch_memoryResultString(MINARCH_MEM_OK));
	TEST_ASSERT_EQUAL_STRING("Core does not support this memory type",
	                         MinArch_memoryResultString(MINARCH_MEM_NO_SUPPORT));
	TEST_ASSERT_EQUAL_STRING("File not found", MinArch_memoryResultString(MINARCH_MEM_FILE_NOT_FOUND));
	TEST_ASSERT_EQUAL_STRING("File I/O error", MinArch_memoryResultString(MINARCH_MEM_FILE_ERROR));
	TEST_ASSERT_EQUAL_STRING("Core returned null memory pointer",
	                         MinArch_memoryResultString(MINARCH_MEM_NULL_POINTER));
}

///////////////////////////////
// Round-trip Tests
///////////////////////////////

void test_sram_write_then_read_roundtrip(void) {
	// Write with pattern
	for (size_t i = 0; i < mock_sram_size; i++) {
		mock_sram_buffer[i] = (uint8_t)(i ^ 0xAA);
	}

	MinArchMemoryResult result =
	    MinArch_writeSRAM(test_path, mock_get_memory_size, mock_get_memory_data);
	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	// Clear buffer
	uint8_t original[8192];
	memcpy(original, mock_sram_buffer, mock_sram_size);
	memset(mock_sram_buffer, 0, mock_sram_size);

	// Read back
	result = MinArch_readSRAM(test_path, mock_get_memory_size, mock_get_memory_data);
	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	// Should match original
	TEST_ASSERT_EQUAL_MEMORY(original, mock_sram_buffer, mock_sram_size);
}

void test_rtc_write_then_read_roundtrip(void) {
	// Write with pattern
	for (size_t i = 0; i < mock_rtc_size; i++) {
		mock_rtc_buffer[i] = (uint8_t)(i * 7);
	}

	MinArchMemoryResult result =
	    MinArch_writeRTC(test_path, mock_get_memory_size, mock_get_memory_data);
	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	// Clear buffer
	uint8_t original[64];
	memcpy(original, mock_rtc_buffer, mock_rtc_size);
	memset(mock_rtc_buffer, 0, mock_rtc_size);

	// Read back
	result = MinArch_readRTC(test_path, mock_get_memory_size, mock_get_memory_data);
	TEST_ASSERT_EQUAL(MINARCH_MEM_OK, result);

	// Should match original
	TEST_ASSERT_EQUAL_MEMORY(original, mock_rtc_buffer, mock_rtc_size);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// SRAM write tests
	RUN_TEST(test_writeSRAM_writes_to_file);
	RUN_TEST(test_writeSRAM_returns_no_support_when_size_zero);
	RUN_TEST(test_writeSRAM_returns_null_pointer_when_data_null);
	RUN_TEST(test_writeSRAM_writes_small_sram);

	// SRAM read tests
	RUN_TEST(test_readSRAM_loads_from_file);
	RUN_TEST(test_readSRAM_returns_file_not_found);
	RUN_TEST(test_readSRAM_returns_no_support_when_size_zero);
	RUN_TEST(test_readSRAM_returns_null_pointer_when_data_null);
	RUN_TEST(test_readSRAM_handles_partial_file);

	// RTC write tests
	RUN_TEST(test_writeRTC_writes_to_file);
	RUN_TEST(test_writeRTC_returns_no_support_when_size_zero);

	// RTC read tests
	RUN_TEST(test_readRTC_loads_from_file);
	RUN_TEST(test_readRTC_returns_file_not_found);

	// Result string tests
	RUN_TEST(test_memoryResultString_returns_descriptions);

	// Round-trip tests
	RUN_TEST(test_sram_write_then_read_roundtrip);
	RUN_TEST(test_rtc_write_then_read_roundtrip);

	return UNITY_END();
}
