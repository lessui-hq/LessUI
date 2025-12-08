/**
 * test_minarch_zip.c - Unit tests for ZIP extraction utilities
 *
 * Tests the ZIP extraction functions using real temp files.
 * These are pure functions operating on FILE* handles.
 *
 * Test coverage:
 * - MinArch_zipCopy: Uncompressed file extraction
 * - MinArch_zipInflate: Deflate-compressed extraction
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/minarch_zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

// Test temp file paths
static char src_path[256];
static char dst_path[256];

void setUp(void) {
	// Create temp file paths
	strcpy(src_path, "/tmp/test_zip_src_XXXXXX");
	strcpy(dst_path, "/tmp/test_zip_dst_XXXXXX");

	int src_fd = mkstemp(src_path);
	int dst_fd = mkstemp(dst_path);

	if (src_fd >= 0)
		close(src_fd);
	if (dst_fd >= 0)
		close(dst_fd);
}

void tearDown(void) {
	unlink(src_path);
	unlink(dst_path);
}

///////////////////////////////
// Helper Functions
///////////////////////////////

static void write_test_data(const char* path, const void* data, size_t size) {
	FILE* f = fopen(path, "wb");
	TEST_ASSERT_NOT_NULL(f);
	TEST_ASSERT_EQUAL(size, fwrite(data, 1, size, f));
	fclose(f);
}

static void verify_file_contents(const char* path, const void* expected, size_t size) {
	FILE* f = fopen(path, "rb");
	TEST_ASSERT_NOT_NULL(f);

	void* buffer = malloc(size);
	TEST_ASSERT_NOT_NULL(buffer);

	size_t read = fread(buffer, 1, size, f);
	fclose(f);

	TEST_ASSERT_EQUAL(size, read);
	TEST_ASSERT_EQUAL_MEMORY(expected, buffer, size);

	free(buffer);
}

///////////////////////////////
// MinArch_zipCopy Tests
///////////////////////////////

void test_zipCopy_copies_small_file(void) {
	const char* test_data = "Hello, World!";
	size_t data_len = strlen(test_data);

	write_test_data(src_path, test_data, data_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipCopy(src, dst, data_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(0, result);
	verify_file_contents(dst_path, test_data, data_len);
}

void test_zipCopy_copies_exact_chunk_size(void) {
	// Test with exactly one chunk (16KB)
	size_t data_len = MINARCH_ZIP_CHUNK_SIZE;
	uint8_t* test_data = malloc(data_len);
	TEST_ASSERT_NOT_NULL(test_data);

	// Fill with pattern
	for (size_t i = 0; i < data_len; i++) {
		test_data[i] = (uint8_t)(i & 0xFF);
	}

	write_test_data(src_path, test_data, data_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipCopy(src, dst, data_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(0, result);
	verify_file_contents(dst_path, test_data, data_len);

	free(test_data);
}

void test_zipCopy_copies_multiple_chunks(void) {
	// Test with multiple chunks (3 * 16KB + some extra)
	size_t data_len = MINARCH_ZIP_CHUNK_SIZE * 3 + 1000;
	uint8_t* test_data = malloc(data_len);
	TEST_ASSERT_NOT_NULL(test_data);

	// Fill with pattern
	for (size_t i = 0; i < data_len; i++) {
		test_data[i] = (uint8_t)((i * 7) & 0xFF);
	}

	write_test_data(src_path, test_data, data_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipCopy(src, dst, data_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(0, result);
	verify_file_contents(dst_path, test_data, data_len);

	free(test_data);
}

void test_zipCopy_copies_empty_data(void) {
	// Create empty source file
	FILE* src = fopen(src_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	fclose(src);

	src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipCopy(src, dst, 0);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(0, result);

	// Verify empty output
	FILE* verify = fopen(dst_path, "rb");
	TEST_ASSERT_NOT_NULL(verify);
	fseek(verify, 0, SEEK_END);
	TEST_ASSERT_EQUAL(0, ftell(verify));
	fclose(verify);
}

void test_zipCopy_fails_on_short_read(void) {
	const char* test_data = "Short";
	write_test_data(src_path, test_data, strlen(test_data));

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	// Try to read more than available
	int result = MinArch_zipCopy(src, dst, 100);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(-1, result);
}

void test_zipCopy_copies_partial_chunk(void) {
	// Test with less than one chunk
	const char* test_data = "Partial chunk data for testing.";
	size_t data_len = strlen(test_data);

	write_test_data(src_path, test_data, data_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipCopy(src, dst, data_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(0, result);
	verify_file_contents(dst_path, test_data, data_len);
}

void test_zipCopy_copies_binary_data(void) {
	// Test with binary data including null bytes
	uint8_t test_data[] = {0x00, 0x01, 0xFF, 0xFE, 0x00, 0x7F, 0x80, 0x00};
	size_t data_len = sizeof(test_data);

	write_test_data(src_path, test_data, data_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipCopy(src, dst, data_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(0, result);
	verify_file_contents(dst_path, test_data, data_len);
}

///////////////////////////////
// MinArch_zipInflate Tests
///////////////////////////////

// Helper to create deflate-compressed data
static size_t create_deflated_data(const void* input, size_t input_len, void* output,
                                   size_t output_capacity) {
	z_stream stream = {0};

	if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
	                 Z_DEFAULT_STRATEGY) != Z_OK) {
		return 0;
	}

	stream.next_in = (Bytef*)input;
	stream.avail_in = input_len;
	stream.next_out = output;
	stream.avail_out = output_capacity;

	if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
		deflateEnd(&stream);
		return 0;
	}

	size_t compressed_size = stream.total_out;
	deflateEnd(&stream);

	return compressed_size;
}

void test_zipInflate_decompresses_simple_text(void) {
	const char* original = "Hello, this is a test of deflate compression!";
	size_t original_len = strlen(original);

	// Compress the data
	uint8_t compressed[1024];
	size_t compressed_len = create_deflated_data(original, original_len, compressed, sizeof(compressed));
	TEST_ASSERT_GREATER_THAN(0, compressed_len);

	// Write compressed data to source file
	write_test_data(src_path, compressed, compressed_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipInflate(src, dst, compressed_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(Z_OK, result);
	verify_file_contents(dst_path, original, original_len);
}

void test_zipInflate_decompresses_binary_data(void) {
	// Create binary pattern
	uint8_t original[256];
	for (int i = 0; i < 256; i++) {
		original[i] = (uint8_t)i;
	}

	// Compress the data
	uint8_t compressed[1024];
	size_t compressed_len = create_deflated_data(original, sizeof(original), compressed, sizeof(compressed));
	TEST_ASSERT_GREATER_THAN(0, compressed_len);

	// Write compressed data to source file
	write_test_data(src_path, compressed, compressed_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipInflate(src, dst, compressed_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(Z_OK, result);
	verify_file_contents(dst_path, original, sizeof(original));
}

void test_zipInflate_decompresses_highly_compressible_data(void) {
	// Create highly compressible data (all same byte)
	size_t original_len = 10000;
	uint8_t* original = malloc(original_len);
	TEST_ASSERT_NOT_NULL(original);
	memset(original, 'A', original_len);

	// Compress the data (should be very small)
	uint8_t compressed[1024];
	size_t compressed_len = create_deflated_data(original, original_len, compressed, sizeof(compressed));
	TEST_ASSERT_GREATER_THAN(0, compressed_len);
	TEST_ASSERT_LESS_THAN(original_len / 10, compressed_len); // Should compress well

	// Write compressed data to source file
	write_test_data(src_path, compressed, compressed_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipInflate(src, dst, compressed_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(Z_OK, result);
	verify_file_contents(dst_path, original, original_len);

	free(original);
}

void test_zipInflate_decompresses_larger_data(void) {
	// Create data larger than chunk size
	size_t original_len = MINARCH_ZIP_CHUNK_SIZE * 2 + 500;
	uint8_t* original = malloc(original_len);
	TEST_ASSERT_NOT_NULL(original);

	// Fill with pattern
	for (size_t i = 0; i < original_len; i++) {
		original[i] = (uint8_t)((i * 13) & 0xFF);
	}

	// Compress the data
	size_t compressed_capacity = original_len + 1024; // Extra space for incompressible data
	uint8_t* compressed = malloc(compressed_capacity);
	TEST_ASSERT_NOT_NULL(compressed);

	size_t compressed_len = create_deflated_data(original, original_len, compressed, compressed_capacity);
	TEST_ASSERT_GREATER_THAN(0, compressed_len);

	// Write compressed data to source file
	write_test_data(src_path, compressed, compressed_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipInflate(src, dst, compressed_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(Z_OK, result);
	verify_file_contents(dst_path, original, original_len);

	free(original);
	free(compressed);
}

void test_zipInflate_fails_on_invalid_data(void) {
	// Write invalid compressed data
	uint8_t garbage[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	write_test_data(src_path, garbage, sizeof(garbage));

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipInflate(src, dst, sizeof(garbage));

	fclose(src);
	fclose(dst);

	// Should return a zlib error code
	TEST_ASSERT_NOT_EQUAL(Z_OK, result);
}

void test_zipInflate_handles_empty_compressed_stream(void) {
	// Create empty compressed stream
	uint8_t compressed[1024];
	size_t compressed_len = create_deflated_data("", 0, compressed, sizeof(compressed));
	TEST_ASSERT_GREATER_THAN(0, compressed_len);

	write_test_data(src_path, compressed, compressed_len);

	FILE* src = fopen(src_path, "rb");
	FILE* dst = fopen(dst_path, "wb");
	TEST_ASSERT_NOT_NULL(src);
	TEST_ASSERT_NOT_NULL(dst);

	int result = MinArch_zipInflate(src, dst, compressed_len);

	fclose(src);
	fclose(dst);

	TEST_ASSERT_EQUAL(Z_OK, result);

	// Verify empty output
	FILE* verify = fopen(dst_path, "rb");
	TEST_ASSERT_NOT_NULL(verify);
	fseek(verify, 0, SEEK_END);
	TEST_ASSERT_EQUAL(0, ftell(verify));
	fclose(verify);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// MinArch_zipCopy tests
	RUN_TEST(test_zipCopy_copies_small_file);
	RUN_TEST(test_zipCopy_copies_exact_chunk_size);
	RUN_TEST(test_zipCopy_copies_multiple_chunks);
	RUN_TEST(test_zipCopy_copies_empty_data);
	RUN_TEST(test_zipCopy_fails_on_short_read);
	RUN_TEST(test_zipCopy_copies_partial_chunk);
	RUN_TEST(test_zipCopy_copies_binary_data);

	// MinArch_zipInflate tests
	RUN_TEST(test_zipInflate_decompresses_simple_text);
	RUN_TEST(test_zipInflate_decompresses_binary_data);
	RUN_TEST(test_zipInflate_decompresses_highly_compressible_data);
	RUN_TEST(test_zipInflate_decompresses_larger_data);
	RUN_TEST(test_zipInflate_fails_on_invalid_data);
	RUN_TEST(test_zipInflate_handles_empty_compressed_stream);

	return UNITY_END();
}
