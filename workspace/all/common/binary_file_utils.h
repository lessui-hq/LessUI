/**
 * binary_file_utils.h - Binary file I/O utilities
 *
 * Simple utilities for reading/writing binary data to files.
 * Extracted from minarch.c for testability and reusability.
 */

#ifndef __BINARY_FILE_UTILS_H__
#define __BINARY_FILE_UTILS_H__

#include <stddef.h>

/**
 * Reads binary data from a file.
 *
 * @param filepath Path to file
 * @param buffer Buffer to read into
 * @param size Number of bytes to read
 * @return Number of bytes actually read, 0 on error
 */
size_t BinaryFile_read(const char* filepath, void* buffer, size_t size);

/**
 * Writes binary data to a file.
 *
 * @param filepath Path to file
 * @param buffer Buffer to write from
 * @param size Number of bytes to write
 * @return Number of bytes actually written, 0 on error
 */
size_t BinaryFile_write(const char* filepath, const void* buffer, size_t size);

#endif // __BINARY_FILE_UTILS_H__
