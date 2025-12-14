/**
 * minarch_game.h - Game file loading utilities
 *
 * Provides functions for game file handling including:
 * - Archive entry detection and extension matching
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
 * Maximum number of extensions to parse from a pipe-delimited string.
 */
#define MINARCH_MAX_EXTENSIONS 32

/**
 * Parses a pipe-delimited extension list into an array.
 *
 * The input string is modified (tokenized) and pointers into it are stored
 * in the extensions array. The caller must ensure the input string remains
 * valid for the lifetime of the extensions array.
 *
 * @param extensions_str Pipe-delimited extension string (e.g., "gb|gbc|dmg")
 *                       Modified in place (strtok)
 * @param out_extensions Array to receive extension pointers (NULL-terminated)
 * @param max_extensions Maximum number of extensions to store
 * @return Number of extensions parsed
 *
 * @example
 *   char exts[] = "gb|gbc|zip";
 *   char* ext_array[32];
 *   int count = MinArchGame_parseExtensions(exts, ext_array, 32);
 *   // count=3, ext_array={"gb","gbc","zip",NULL}
 */
int MinArchGame_parseExtensions(char* extensions_str, char** out_extensions, int max_extensions);

/**
 * Checks if a filename matches any of the given extensions.
 *
 * Extracts the file extension from the filename and compares it
 * against the provided extensions array (case-insensitive).
 *
 * @param filename Filename to check (can include path)
 * @param extensions NULL-terminated array of extensions to match (without dots)
 * @return true if the filename matches any extension, false otherwise
 */
bool MinArchGame_matchesExtension(const char* filename, char* const* extensions);

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
