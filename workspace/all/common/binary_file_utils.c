/**
 * binary_file_utils.c - Binary file I/O utilities
 *
 * Extracted from player.c for testability.
 */

#include "binary_file_utils.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Reads binary data from a file.
 */
size_t BinaryFile_read(const char* filepath, void* buffer, size_t size) {
	if (!filepath || !buffer || size == 0) {
		LOG_error("Invalid parameters for BinaryFile_read");
		return 0;
	}

	FILE* file = fopen(filepath, "rb");
	if (!file) {
		LOG_errno("Failed to open file for reading: %s", filepath);
		return 0;
	}

	size_t bytes_read = fread(buffer, 1, size, file);
	if (bytes_read < size && ferror(file)) {
		LOG_error("Failed to read %zu bytes from %s: only read %zu bytes", size, filepath,
		          bytes_read);
	}
	(void)fclose(file); // Binary file opened for reading

	return bytes_read;
}

/**
 * Writes binary data to a file.
 */
size_t BinaryFile_write(const char* filepath, const void* buffer, size_t size) {
	if (!filepath || !buffer || size == 0) {
		LOG_error("Invalid parameters for BinaryFile_write");
		return 0;
	}

	FILE* file = fopen(filepath, "wb");
	if (!file) {
		LOG_errno("Failed to open file for writing: %s", filepath);
		return 0;
	}

	size_t bytes_written = fwrite(buffer, 1, size, file);
	if (bytes_written < size) {
		LOG_error("Failed to write %zu bytes to %s: only wrote %zu bytes", size, filepath,
		          bytes_written);
	}
	(void)fclose(file); // Binary file opened for writing

	return bytes_written;
}
