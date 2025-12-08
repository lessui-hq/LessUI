/**
 * minui_root.h - Root directory generation utilities
 *
 * Provides functions for building the launcher's root directory,
 * including system deduplication, alias application, and entry ordering.
 *
 * Designed for testability with explicit parameters.
 * Extracted from minui.c.
 */

#ifndef __MINUI_ROOT_H__
#define __MINUI_ROOT_H__

#include <stdbool.h>

/**
 * Maximum path/name length.
 */
#define MINUI_ROOT_MAX_PATH 512
#define MINUI_ROOT_MAX_NAME 256

/**
 * Maximum entries in root directory.
 */
#define MINUI_ROOT_MAX_ENTRIES 128

/**
 * Root entry info (lightweight version for testing).
 */
typedef struct {
	char path[MINUI_ROOT_MAX_PATH];
	char name[MINUI_ROOT_MAX_NAME];
	int type; // 0=dir, 1=rom, 2=pak
	int visible;
} MinUIRootEntry;

/**
 * Root directory configuration.
 */
typedef struct {
	const char* roms_path;
	const char* collections_path;
	const char* tools_path;
	const char* recents_path;
	int simple_mode;
} MinUIRootConfig;

/**
 * Alias entry for map.txt parsing.
 */
typedef struct {
	char key[MINUI_ROOT_MAX_NAME];
	char value[MINUI_ROOT_MAX_NAME];
} MinUIAlias;

/**
 * Alias list.
 */
typedef struct {
	MinUIAlias* items;
	int count;
	int capacity;
} MinUIAliasList;

/**
 * Creates a new alias list.
 *
 * @param capacity Initial capacity
 * @return New list, or NULL on error
 */
MinUIAliasList* MinUIAliasList_new(int capacity);

/**
 * Frees an alias list.
 */
void MinUIAliasList_free(MinUIAliasList* list);

/**
 * Adds an alias to the list.
 *
 * @param list List to add to
 * @param key Filename key
 * @param value Display name value
 * @return true on success
 */
bool MinUIAliasList_add(MinUIAliasList* list, const char* key, const char* value);

/**
 * Looks up an alias by key.
 *
 * @param list List to search
 * @param key Filename to look up
 * @return Alias value, or NULL if not found
 */
const char* MinUIAliasList_get(const MinUIAliasList* list, const char* key);

/**
 * Parses a map.txt format line.
 *
 * Format: "filename<TAB>display name"
 *
 * @param line Line to parse
 * @param out_key Buffer for filename (at least MINUI_ROOT_MAX_NAME)
 * @param out_value Buffer for display name (at least MINUI_ROOT_MAX_NAME)
 * @return true if line was valid and parsed
 */
bool MinUIRoot_parseMapLine(const char* line, char* out_key, char* out_value);

/**
 * Checks if a filename should be hidden.
 *
 * Hidden files start with '.' or are '.' or '..'.
 *
 * @param filename Filename to check
 * @return true if file should be hidden
 */
bool MinUIRoot_isHidden(const char* filename);

/**
 * Checks if two entries have the same display name (for deduplication).
 *
 * @param name1 First display name
 * @param name2 Second display name
 * @return true if names match
 */
bool MinUIRoot_namesMatch(const char* name1, const char* name2);

/**
 * Extracts the display name from a ROM folder name.
 *
 * Strips the tag prefix if present (e.g., "001) Game Boy" -> "Game Boy").
 *
 * @param folder_name Folder name
 * @param out_name Buffer for display name
 */
void MinUIRoot_getDisplayName(const char* folder_name, char* out_name);

/**
 * Extracts the filename from a path.
 *
 * @param path Full path
 * @param out_filename Buffer for filename
 */
void MinUIRoot_extractFilename(const char* path, char* out_filename);

/**
 * Removes duplicate entries with the same display name.
 *
 * Entries are assumed to be sorted by name. Duplicates are marked
 * as invisible by setting visible=0.
 *
 * @param entries Array of entries
 * @param count Number of entries
 * @return Number of visible entries remaining
 */
int MinUIRoot_deduplicateEntries(MinUIRootEntry* entries, int count);

/**
 * Applies aliases to entries.
 *
 * @param entries Array of entries
 * @param count Number of entries
 * @param aliases Alias list to apply
 * @return Number of entries that were renamed
 */
int MinUIRoot_applyAliases(MinUIRootEntry* entries, int count, const MinUIAliasList* aliases);

/**
 * Checks if a system directory name indicates a valid ROM system.
 *
 * Valid system names have the format "TAG (System Name)" or just "System Name".
 *
 * @param dir_name Directory name
 * @return true if it looks like a valid system directory
 */
bool MinUIRoot_isValidSystemDir(const char* dir_name);

/**
 * Compares two entries for sorting (by display name).
 *
 * @param a First entry
 * @param b Second entry
 * @return Negative if a<b, 0 if equal, positive if a>b
 */
int MinUIRoot_compareEntries(const MinUIRootEntry* a, const MinUIRootEntry* b);

/**
 * Sorts entries by display name (case-insensitive).
 *
 * @param entries Array of entries
 * @param count Number of entries
 */
void MinUIRoot_sortEntries(MinUIRootEntry* entries, int count);

/**
 * Counts visible entries in array.
 *
 * @param entries Array of entries
 * @param count Total entries
 * @return Number with visible=1
 */
int MinUIRoot_countVisible(const MinUIRootEntry* entries, int count);

#endif // __MINUI_ROOT_H__
