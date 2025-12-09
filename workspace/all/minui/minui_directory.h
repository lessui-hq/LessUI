/**
 * minui_directory.h - Directory building utilities for MinUI
 *
 * Provides testable functions for building directory entry lists:
 * - Console directory detection
 * - Entry type determination (ROM, PAK, directory)
 * - ROM system availability checking
 * - Directory collation for multi-region systems
 *
 * Extracted from minui.c for testability with explicit path parameters.
 */

#ifndef __MINUI_DIRECTORY_H__
#define __MINUI_DIRECTORY_H__

#include "minui_entry.h"

/**
 * Maximum path length for directory operations.
 */
#define MINUI_DIR_MAX_PATH 512

/**
 * Checks if a path is a top-level console directory.
 *
 * A console directory is one whose parent is the Roms directory.
 * Example: /mnt/SDCARD/Roms/GB is a console dir
 *          /mnt/SDCARD/Roms/GB/subfolder is not
 *
 * @param path Full path to check
 * @param roms_path ROMS_PATH constant (e.g., "/mnt/SDCARD/Roms")
 * @return 1 if path is a console directory, 0 otherwise
 */
int MinUIDir_isConsoleDir(const char* path, const char* roms_path);

/**
 * Determines the entry type for a directory entry.
 *
 * Type determination rules:
 * - If is_dir and filename ends with ".pak": ENTRY_PAK
 * - If is_dir and not .pak: ENTRY_DIR
 * - If not is_dir and in collections path: ENTRY_DIR (collections are pseudo-directories)
 * - If not is_dir and elsewhere: ENTRY_ROM
 *
 * @param filename Entry filename (not full path)
 * @param is_dir 1 if entry is a directory, 0 if file
 * @param parent_path Full path of parent directory
 * @param collections_path COLLECTIONS_PATH constant
 * @return Entry type (ENTRY_DIR, ENTRY_PAK, or ENTRY_ROM)
 */
int MinUIDir_determineEntryType(const char* filename, int is_dir, const char* parent_path,
                                const char* collections_path);

/**
 * Checks if a ROM system directory has any playable ROMs.
 *
 * A system is considered to have ROMs if:
 * 1. The emulator .pak exists (checked via MinUI_hasEmu)
 * 2. The directory contains at least one non-hidden file
 *
 * @param dir_name Name of ROM directory (e.g., "GB (Game Boy)")
 * @param roms_path ROMS_PATH constant
 * @param paks_path PAKS_PATH constant
 * @param sdcard_path SDCARD_PATH constant
 * @param platform PLATFORM constant
 * @return 1 if system has playable ROMs, 0 otherwise
 */
int MinUIDir_hasRoms(const char* dir_name, const char* roms_path, const char* paks_path,
                     const char* sdcard_path, const char* platform);

/**
 * Builds a collation prefix for matching related console directories.
 *
 * Console directories often have region suffixes like "(USA)" or "(Japan)".
 * This function extracts a prefix that can match all regions.
 *
 * Example: "/Roms/Game Boy (USA)" -> "/Roms/Game Boy ("
 * This prefix matches both "Game Boy (USA)" and "Game Boy (Japan)".
 *
 * @param path Full path to console directory
 * @param out_prefix Buffer to receive prefix (at least MINUI_DIR_MAX_PATH bytes)
 * @return 1 if prefix was extracted, 0 if path has no region suffix
 */
int MinUIDir_buildCollationPrefix(const char* path, char* out_prefix);

/**
 * Checks if a path matches a collation prefix.
 *
 * Used to determine if two console directories should be collated together.
 *
 * @param path Full path to check
 * @param collation_prefix Prefix from MinUIDir_buildCollationPrefix
 * @return 1 if path matches the collation prefix, 0 otherwise
 */
int MinUIDir_matchesCollation(const char* path, const char* collation_prefix);

/**
 * Result structure for directory scanning.
 */
typedef struct {
	char** paths; // Array of full paths (caller must free each + array)
	int* is_dirs; // Array of is_directory flags
	int count; // Number of entries
	int capacity; // Allocated capacity
} MinUIDirScanResult;

/**
 * Creates a new scan result structure.
 *
 * @param initial_capacity Initial capacity for entries
 * @return New result structure, or NULL on allocation failure
 */
MinUIDirScanResult* MinUIDirScanResult_new(int initial_capacity);

/**
 * Frees a scan result structure and all its contents.
 *
 * @param result Result to free
 */
void MinUIDirScanResult_free(MinUIDirScanResult* result);

/**
 * Adds an entry to scan results.
 *
 * @param result Result structure to add to
 * @param path Full path (copied)
 * @param is_dir 1 if directory, 0 if file
 * @return 1 on success, 0 on failure
 */
int MinUIDirScanResult_add(MinUIDirScanResult* result, const char* path, int is_dir);

/**
 * Scans a directory and returns non-hidden entries.
 *
 * Does not recurse into subdirectories.
 * Filters out entries starting with '.' (hidden files).
 *
 * @param dir_path Directory to scan
 * @return Scan result (caller must free), or NULL on error
 */
MinUIDirScanResult* MinUIDir_scan(const char* dir_path);

/**
 * Scans multiple directories with collation support.
 *
 * Used for console directories that may be split across regions.
 * For example, "GB (USA)" and "GB (Japan)" are collated together.
 *
 * @param roms_path ROMS_PATH constant
 * @param collation_prefix Collation prefix from MinUIDir_buildCollationPrefix
 * @return Combined scan result (caller must free), or NULL on error
 */
MinUIDirScanResult* MinUIDir_scanCollated(const char* roms_path, const char* collation_prefix);

///////////////////////////////
// Directory structure
///////////////////////////////

/**
 * Represents a directory in the file browser.
 *
 * Maintains list of entries, alphabetical index, and rendering state
 * (selected item, visible window start/end).
 */
typedef struct Directory {
	char* path; // Full path to directory
	char* name; // Display name
	Array* entries; // Array of Entry pointers
	IntArray* alphas; // Alphabetical index for L1/R1 navigation
	// Rendering state
	int selected; // Currently selected entry index
	int start; // First visible entry index
	int end; // One past last visible entry index
} Directory;

/**
 * Creates a new directory from a path.
 *
 * Automatically determines which type of directory this is and
 * populates its entries accordingly:
 * - Root (SDCARD_PATH): Shows systems, recents, collections, tools
 * - Recently played (FAUX_RECENT_PATH): Shows recent games
 * - Collection (.txt file): Loads games from text file
 * - Multi-disc (.m3u file): Shows disc list
 * - Regular directory: Shows files and subdirectories
 *
 * Note: This is a forward declaration. The actual implementation
 * is in minui.c because it depends on global state (simple_mode, recents array).
 *
 * @param path Full path to directory
 * @param selected Initial selected index
 * @return Pointer to allocated Directory
 *
 * @warning Caller must free with Directory_free()
 */
Directory* Directory_new(char* path, int selected);

/**
 * Frees a directory and all its contents.
 *
 * @param self Directory to free
 */
void Directory_free(Directory* self);

/**
 * Indexes a directory's entries and applies name aliasing.
 *
 * This function performs several important tasks:
 * 1. Loads map.txt (if present) to alias display names
 * 2. Filters out entries marked as hidden via map.txt
 * 3. Re-sorts entries if any names were aliased
 * 4. Detects duplicate display names and generates unique names
 * 5. Builds alphabetical index for L1/R1 navigation
 *
 * Map.txt format: Each line is "filename<TAB>display name"
 * - If display name starts with '.', the entry is hidden
 * - Collections use a shared map.txt in COLLECTIONS_PATH
 *
 * Duplicate handling:
 * - If two entries have the same display name but different filenames,
 *   shows the filename to disambiguate
 * - If filenames are also identical (cross-platform ROMs), appends
 *   the emulator name in parentheses
 *
 * Note: This is a forward declaration. The actual implementation
 * is in minui.c because it depends on directory_index module.
 *
 * @param self Directory to index (modified in place)
 */
void Directory_index(Directory* self);

/**
 * Pops and frees the top directory from a directory array.
 *
 * @param self Array of Directory pointers
 */
void DirectoryArray_pop(Array* self);

/**
 * Frees a directory array and all directories it contains.
 *
 * @param self Array to free
 */
void DirectoryArray_free(Array* self);

#endif // __MINUI_DIRECTORY_H__
