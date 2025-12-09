/**
 * minarch_zip.h - ZIP archive extraction utilities
 *
 * Provides functions to extract files from ZIP archives, supporting
 * both uncompressed (stored) and deflate-compressed files.
 *
 * Extracted from minarch.c for testability.
 */

#ifndef __MINARCH_ZIP_H__
#define __MINARCH_ZIP_H__

#include <stddef.h>
#include <stdio.h>

/**
 * Chunk size for ZIP extraction operations.
 * Matches minarch.c internal buffer size.
 */
#define MINARCH_ZIP_CHUNK_SIZE 0x4000

/**
 * Copies uncompressed data from ZIP archive to destination file.
 *
 * Used for ZIP files with compression method 0 (stored).
 * Reads from the current position in the ZIP file.
 *
 * @param zip Source ZIP file (positioned at data start)
 * @param dst Destination file for extracted data
 * @param size Number of bytes to copy
 * @return 0 on success, -1 on error
 */
int MinArchZip_copy(FILE* zip, FILE* dst, size_t size);

/**
 * Extracts and decompresses deflate-compressed data from ZIP archive.
 *
 * Used for ZIP files with compression method 8 (deflate).
 * Uses zlib for decompression.
 *
 * @param zip Source ZIP file (positioned at compressed data start)
 * @param dst Destination file for decompressed data
 * @param size Number of compressed bytes to read
 * @return 0 on success, non-zero error code on failure
 *
 * Error codes are zlib Z_* codes:
 * - Z_OK (0): Success
 * - Z_ERRNO: File I/O error
 * - Z_DATA_ERROR: Corrupt or invalid compressed data
 * - Z_MEM_ERROR: Memory allocation failure
 */
int MinArchZip_inflate(FILE* zip, FILE* dst, size_t size);

#endif // __MINARCH_ZIP_H__
