/**
 * test_binary_file_utils.c - Tests for binary file I/O operations
 *
 * Tests fread/fwrite operations using real temp files.
 * Demonstrates binary data handling patterns used in minarch.c (SRAM, RTC, save states).
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/binary_file_utils.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Basic Binary I/O Tests
///////////////////////////////

void test_BinaryFile_write_read_small_buffer(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	// Write small binary buffer
	uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	size_t written = BinaryFile_write(temp_path, write_data, 5);
	TEST_ASSERT_EQUAL_UINT(5, written);

	// Read back
	uint8_t read_data[5] = {0};
	size_t read_bytes = BinaryFile_read(temp_path, read_data, 5);
	TEST_ASSERT_EQUAL_UINT(5, read_bytes);

	// Verify data matches
	TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, 5);

	unlink(temp_path);
}

void test_BinaryFile_write_read_large_buffer(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	// Write 1KB buffer
	uint8_t write_data[1024];
	for (int i = 0; i < 1024; i++) {
		write_data[i] = (uint8_t)(i % 256);
	}

	size_t written = BinaryFile_write(temp_path, write_data, 1024);
	TEST_ASSERT_EQUAL_UINT(1024, written);

	// Read back
	uint8_t read_data[1024] = {0};
	size_t read_bytes = BinaryFile_read(temp_path, read_data, 1024);
	TEST_ASSERT_EQUAL_UINT(1024, read_bytes);

	// Verify
	TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, 1024);

	unlink(temp_path);
}

void test_BinaryFile_write_read_sram_like_data(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	// Simulate SRAM data (32KB like Game Boy saves)
	const size_t sram_size = 32 * 1024;
	uint8_t* sram = malloc(sram_size);
	TEST_ASSERT_NOT_NULL(sram);

	// Fill with pattern
	for (size_t i = 0; i < sram_size; i++) {
		sram[i] = (uint8_t)((i * 7) % 256);
	}

	// Write
	size_t written = BinaryFile_write(temp_path, sram, sram_size);
	TEST_ASSERT_EQUAL_UINT(sram_size, written);

	// Read back
	uint8_t* sram_read = malloc(sram_size);
	TEST_ASSERT_NOT_NULL(sram_read);
	memset(sram_read, 0, sram_size);

	size_t read_bytes = BinaryFile_read(temp_path, sram_read, sram_size);
	TEST_ASSERT_EQUAL_UINT(sram_size, read_bytes);

	// Verify
	TEST_ASSERT_EQUAL_MEMORY(sram, sram_read, sram_size);

	free(sram);
	free(sram_read);
	unlink(temp_path);
}

void test_BinaryFile_write_read_rtc_like_data(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	// Simulate RTC data (small, like Game Boy RTC - few bytes)
	uint8_t rtc_data[] = {
	    0x12, 0x34, 0x56, 0x78, // Seconds, minutes, hours, days
	    0xAB, 0xCD, 0xEF, 0x01  // Day carry, halt flag, etc.
	};

	size_t written = BinaryFile_write(temp_path, rtc_data, sizeof(rtc_data));
	TEST_ASSERT_EQUAL_UINT(sizeof(rtc_data), written);

	uint8_t rtc_read[sizeof(rtc_data)] = {0};
	size_t read_bytes = BinaryFile_read(temp_path, rtc_read, sizeof(rtc_read));
	TEST_ASSERT_EQUAL_UINT(sizeof(rtc_data), read_bytes);

	TEST_ASSERT_EQUAL_MEMORY(rtc_data, rtc_read, sizeof(rtc_data));

	unlink(temp_path);
}

///////////////////////////////
// Error Cases
///////////////////////////////

void test_BinaryFile_read_nonexistent_file(void) {
	uint8_t buffer[100];
	size_t read_bytes = BinaryFile_read("/nonexistent/file.bin", buffer, 100);
	TEST_ASSERT_EQUAL_UINT(0, read_bytes);
}

void test_BinaryFile_write_invalid_path(void) {
	uint8_t data[] = {0x01, 0x02, 0x03};
	size_t written = BinaryFile_write("/nonexistent/dir/file.bin", data, 3);
	TEST_ASSERT_EQUAL_UINT(0, written);
}

void test_BinaryFile_read_null_buffer(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	close(fd);

	size_t read_bytes = BinaryFile_read(temp_path, NULL, 100);
	TEST_ASSERT_EQUAL_UINT(0, read_bytes);

	unlink(temp_path);
}

void test_BinaryFile_write_null_buffer(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	close(fd);

	size_t written = BinaryFile_write(temp_path, NULL, 100);
	TEST_ASSERT_EQUAL_UINT(0, written);

	unlink(temp_path);
}

void test_BinaryFile_read_zero_size(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	close(fd);

	uint8_t buffer[100];
	size_t read_bytes = BinaryFile_read(temp_path, buffer, 0);
	TEST_ASSERT_EQUAL_UINT(0, read_bytes);

	unlink(temp_path);
}

void test_BinaryFile_write_zero_size(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	close(fd);

	uint8_t data[] = {0x01};
	size_t written = BinaryFile_write(temp_path, data, 0);
	TEST_ASSERT_EQUAL_UINT(0, written);

	unlink(temp_path);
}

///////////////////////////////
// Special Cases
///////////////////////////////

void test_BinaryFile_write_overwrites_existing(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	close(fd);

	// Write initial data
	uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	BinaryFile_write(temp_path, data1, 5);

	// Overwrite with smaller data
	uint8_t data2[] = {0xFF, 0xEE};
	size_t written = BinaryFile_write(temp_path, data2, 2);
	TEST_ASSERT_EQUAL_UINT(2, written);

	// Read back - should only have 2 bytes now
	uint8_t read_data[5] = {0};
	size_t read_bytes = BinaryFile_read(temp_path, read_data, 5);
	TEST_ASSERT_EQUAL_UINT(2, read_bytes);
	TEST_ASSERT_EQUAL_UINT8(0xFF, read_data[0]);
	TEST_ASSERT_EQUAL_UINT8(0xEE, read_data[1]);

	unlink(temp_path);
}

void test_BinaryFile_read_partial(void) {
	char temp_path[] = "/tmp/bintest_XXXXXX";
	int fd = mkstemp(temp_path);
	close(fd);

	// Write 10 bytes
	uint8_t write_data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	BinaryFile_write(temp_path, write_data, 10);

	// Read only first 5 bytes
	uint8_t read_data[5] = {0};
	size_t read_bytes = BinaryFile_read(temp_path, read_data, 5);
	TEST_ASSERT_EQUAL_UINT(5, read_bytes);
	TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, 5);

	unlink(temp_path);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic I/O
	RUN_TEST(test_BinaryFile_write_read_small_buffer);
	RUN_TEST(test_BinaryFile_write_read_large_buffer);
	RUN_TEST(test_BinaryFile_write_read_sram_like_data);
	RUN_TEST(test_BinaryFile_write_read_rtc_like_data);

	// Error cases
	RUN_TEST(test_BinaryFile_read_nonexistent_file);
	RUN_TEST(test_BinaryFile_write_invalid_path);
	RUN_TEST(test_BinaryFile_read_null_buffer);
	RUN_TEST(test_BinaryFile_write_null_buffer);
	RUN_TEST(test_BinaryFile_read_zero_size);
	RUN_TEST(test_BinaryFile_write_zero_size);

	// Special cases
	RUN_TEST(test_BinaryFile_write_overwrites_existing);
	RUN_TEST(test_BinaryFile_read_partial);

	return UNITY_END();
}
