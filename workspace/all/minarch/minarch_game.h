/**
 * minarch_game.h - Game file loading utilities
 *
 * Provides functions for game file handling including:
 * - ZIP archive entry detection and extension matching
 * - M3U playlist detection for multi-disc games
 * - Extension list parsing
 *
 * Extracted from minarch.c for testability.
 */

#ifndef __MINARCH_GAME_H__
#define __MINARCH_GAME_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * ZIP local file header size (fixed portion).
 */
#define MINARCH_ZIP_HEADER_SIZE 30

/**
 * Maximum number of extensions to parse from a pipe-delimited string.
 */
#define MINARCH_MAX_EXTENSIONS 32

/**
 * Read 16-bit little-endian value from buffer.
 */
#define MINARCH_ZIP_LE_READ16(buf) ((uint16_t)(((uint8_t*)(buf))[1] << 8 | ((uint8_t*)(buf))[0]))

/**
 * Read 32-bit little-endian value from buffer.
 */
#define MINARCH_ZIP_LE_READ32(buf)                                                                 \
	((uint32_t)(((uint8_t*)(buf))[3] << 24 | ((uint8_t*)(buf))[2] << 16 |                          \
	            ((uint8_t*)(buf))[1] << 8 | ((uint8_t*)(buf))[0]))

/**
 * Information about a found ZIP entry.
 */
typedef struct {
	char filename[512]; /**< Filename within ZIP */
	uint32_t compressed_size; /**< Size of compressed data */
	uint16_t compression_method; /**< 0=stored, 8=deflate */
	long data_offset; /**< File offset to compressed data start */
	bool found; /**< Whether a matching entry was found */
} MinArchZipEntryInfo;

/**
 * Parses a pipe-delimited extension list into an array.
 *
 * The input string is modified (tokenized) and pointers into it are stored
 * in the extensions array. The caller must ensure the input string remains
 * valid for the lifetime of the extensions array.
 *
 * @param extensions_str Pipe-delimited extension string (e.g., "gb|gbc|dmg")
 *                       Modified in place (strtok)
 * @param out_extensions Array to receive extension pointers
 * @param max_extensions Maximum number of extensions to store
 * @param out_supports_zip Set to true if "zip" extension found
 * @return Number of extensions parsed
 *
 * @example
 *   char exts[] = "gb|gbc|zip";
 *   char* ext_array[32];
 *   bool supports_zip;
 *   int count = MinArchGame_parseExtensions(exts, ext_array, 32, &supports_zip);
 *   // count=3, ext_array={"gb","gbc","zip"}, supports_zip=true
 */
int MinArchGame_parseExtensions(char* extensions_str, char** out_extensions, int max_extensions,
                                bool* out_supports_zip);

/**
 * Finds the first ZIP entry matching any of the given extensions.
 *
 * Searches through ZIP local file headers to find the first entry whose
 * filename ends with one of the provided extensions.
 *
 * @param zip_header 30-byte ZIP local file header
 * @param filename Filename extracted from the header
 * @param extensions NULL-terminated array of extensions to match (without dots)
 * @return true if the filename matches any extension, false otherwise
 *
 * @note The caller is responsible for iterating through ZIP entries
 */
bool MinArchGame_matchesExtension(const char* filename, char* const* extensions);

/**
 * Parses a ZIP local file header.
 *
 * Extracts compression method, filename length, compressed size, and extra
 * field length from the header bytes.
 *
 * @param header 30-byte header buffer
 * @param out_compression_method Compression method (0=stored, 8=deflate)
 * @param out_filename_len Length of filename field
 * @param out_compressed_size Size of compressed data
 * @param out_extra_len Length of extra field
 * @return true if header appears valid, false if data descriptor bit is set
 */
bool MinArchGame_parseZipHeader(const uint8_t* header, uint16_t* out_compression_method,
                                uint16_t* out_filename_len, uint32_t* out_compressed_size,
                                uint16_t* out_extra_len);

/**
 * Detects if an M3U playlist exists for a ROM path.
 *
 * For a ROM at "/path/to/Game (Disc 1)/image.cue", checks if
 * "/path/to/Game (Disc 1).m3u" exists.
 *
 * @param rom_path Full path to the ROM file
 * @param out_m3u_path Buffer to receive M3U path if found
 * @param m3u_path_size Size of out_m3u_path buffer
 * @return true if M3U file exists, false otherwise
 *
 * @note Uses exists() from utils.h to check for file
 */
bool MinArchGame_detectM3uPath(const char* rom_path, char* out_m3u_path, size_t m3u_path_size);

/**
 * Builds the M3U path for a given ROM path without checking existence.
 *
 * Pure string manipulation - takes a ROM path and constructs what the
 * corresponding M3U path would be.
 *
 * For "/path/to/Game (Disc 1)/image.cue":
 * Returns "/path/to/Game (Disc 1).m3u"
 *
 * @param rom_path Full path to the ROM file
 * @param out_m3u_path Buffer to receive constructed M3U path
 * @param m3u_path_size Size of out_m3u_path buffer
 * @return true if path was constructed successfully, false if invalid input
 */
bool MinArchGame_buildM3uPath(const char* rom_path, char* out_m3u_path, size_t m3u_path_size);

#endif // __MINARCH_GAME_H__
