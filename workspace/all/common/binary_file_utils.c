/**
 * binary_file_utils.c - Binary file I/O utilities
 *
 * Extracted from minarch.c for testability.
 */

#include "binary_file_utils.h"
#include <stdio.h>

/**
 * Reads binary data from a file.
 */
size_t BinaryFile_read(const char* filepath, void* buffer, size_t size) {
	if (!filepath || !buffer || size == 0) {
		return 0;
	}

	FILE* file = fopen(filepath, "rb");
	if (!file) {
		return 0;
	}

	size_t bytes_read = fread(buffer, 1, size, file);
	fclose(file);

	return bytes_read;
}

/**
 * Writes binary data to a file.
 */
size_t BinaryFile_write(const char* filepath, const void* buffer, size_t size) {
	if (!filepath || !buffer || size == 0) {
		return 0;
	}

	FILE* file = fopen(filepath, "wb");
	if (!file) {
		return 0;
	}

	size_t bytes_written = fwrite(buffer, 1, size, file);
	fclose(file);

	return bytes_written;
}
