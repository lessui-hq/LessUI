/**
 * player_archive.h - Archive extraction utilities using 7z
 *
 * Provides functions to extract files from ZIP and 7z archives by
 * shelling out to the 7z binary (available in PATH at runtime).
 *
 * Replaces the custom ZIP parsing that was in player_zip.c.
 */

#ifndef __PLAYER_ARCHIVE_H__
#define __PLAYER_ARCHIVE_H__

#include <stdbool.h>
#include <stddef.h>

/**
 * Maximum number of extensions to match when extracting.
 */
#define PLAYER_ARCHIVE_MAX_EXTENSIONS 32

/**
 * Return codes for archive operations.
 */
#define PLAYER_ARCHIVE_OK 0
#define PLAYER_ARCHIVE_ERR_OPEN -1
#define PLAYER_ARCHIVE_ERR_LIST -2
#define PLAYER_ARCHIVE_ERR_NO_MATCH -3
#define PLAYER_ARCHIVE_ERR_EXTRACT -4
#define PLAYER_ARCHIVE_ERR_TMPDIR -5

/**
 * Checks if a file path has an archive extension (.zip or .7z).
 *
 * @param path File path to check
 * @return true if path ends with .zip or .7z (case-insensitive)
 */
bool PlayerArchive_isArchive(const char* path);

/**
 * Extracts the first matching file from an archive.
 *
 * Shells out to the 7z binary to:
 * 1. List archive contents
 * 2. Find first file matching any of the given extensions
 * 3. Extract that file to a temp directory
 *
 * The temp directory is created under /tmp/player-XXXXXX/.
 * Caller is responsible for cleaning up the extracted file.
 *
 * @param archive_path Path to the .zip or .7z file
 * @param extensions NULL-terminated array of extensions to match (without dots)
 * @param out_extracted_path Buffer to receive path of extracted file
 * @param path_size Size of out_extracted_path buffer
 * @return PLAYER_ARCHIVE_OK on success, error code on failure
 *
 * @example
 *   char* exts[] = {"gb", "gbc", NULL};
 *   char extracted[512];
 *   int result = PlayerArchive_extract("game.zip", exts, extracted, sizeof(extracted));
 *   if (result == PLAYER_ARCHIVE_OK) {
 *       // Use extracted file, then clean up
 *       remove(extracted);
 *   }
 */
int PlayerArchive_extract(const char* archive_path, char* const* extensions,
                          char* out_extracted_path, size_t path_size);

/**
 * Lists files in an archive and finds the first matching extension.
 *
 * This is a lower-level function exposed for testing. Most callers
 * should use PlayerArchive_extract() instead.
 *
 * @param archive_path Path to the archive
 * @param extensions NULL-terminated array of extensions to match
 * @param out_filename Buffer to receive matching filename
 * @param filename_size Size of out_filename buffer
 * @return PLAYER_ARCHIVE_OK if match found, error code otherwise
 */
int PlayerArchive_findMatch(const char* archive_path, char* const* extensions, char* out_filename,
                            size_t filename_size);

#endif // __PLAYER_ARCHIVE_H__
