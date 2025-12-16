/**
 * directory_index.h - Directory indexing for Launcher file browser
 *
 * Provides functions to index directory entries:
 * - Apply map.txt aliases
 * - Filter hidden entries
 * - Detect and mark duplicate names
 * - Build alphabetical navigation index
 */

#ifndef DIRECTORY_INDEX_H
#define DIRECTORY_INDEX_H

#include "launcher_entry.h"
#include "stringmap.h"

/**
 * Gets the alphabetical index for a sort key.
 *
 * Used to group entries by first letter for L1/R1 shoulder button navigation.
 *
 * @param sort_key Sort key string (articles already stripped)
 * @return 0 for non-alphabetic, 1-26 for A-Z (case-insensitive)
 */
int DirectoryIndex_getAlphaChar(const char* sort_key);

/**
 * Generates a unique disambiguation string for an entry.
 *
 * Appends the emulator name in parentheses to disambiguate entries
 * with identical display names but from different systems.
 *
 * Example: "Tetris" with path "/Roms/GB/Tetris.gb" -> "Tetris (GB)"
 *
 * @param entry_name Entry's display name
 * @param entry_path Entry's full path
 * @param out_name Output buffer for unique name (min 256 bytes)
 */
void DirectoryIndex_getUniqueName(const char* entry_name, const char* entry_path, char* out_name);

/**
 * Applies map.txt aliases to entries.
 *
 * For each entry, looks up its filename in the map. If found,
 * updates the entry's name to the alias value.
 *
 * @param entries Array of Entry pointers (modified in place)
 * @param map StringMap mapping filenames to display names
 * @return 1 if any aliases were applied (entries need resorting), 0 otherwise
 */
int DirectoryIndex_applyAliases(Array* entries, StringMap* map);

/**
 * Removes hidden entries from an array.
 *
 * An entry is hidden if its name starts with '.' or ends with ".disabled".
 * Hidden entries are freed and removed from the array.
 *
 * @param entries Array of Entry pointers (modified in place)
 * @return New array with hidden entries removed, or NULL on error
 *
 * @note Caller must free returned array with Array_free() (not EntryArray_free
 *       since entries were moved, not copied)
 * @note Original array should be freed with Array_free() after this call
 */
Array* DirectoryIndex_filterHidden(Array* entries);

/**
 * Marks entries with duplicate display names for disambiguation.
 *
 * When two consecutive entries (after sorting) have the same display name:
 * - If their filenames differ, sets unique to the filename
 * - If filenames are identical (cross-platform ROM), sets unique to emulator name
 *
 * @param entries Sorted array of Entry pointers (modified in place)
 */
void DirectoryIndex_markDuplicates(Array* entries);

/**
 * Builds alphabetical navigation index.
 *
 * Creates an index mapping letter groups to entry positions for L1/R1
 * shoulder button navigation. Also sets each entry's alpha field.
 *
 * @param entries Array of Entry pointers (modified in place to set alpha)
 * @param alphas IntArray to populate with letter group positions
 */
void DirectoryIndex_buildAlphaIndex(Array* entries, IntArray* alphas);

/**
 * Performs full directory indexing.
 *
 * Convenience function that applies aliases, filters hidden entries,
 * marks duplicates, and builds the alpha index.
 *
 * @param entries Array of Entry pointers (may be replaced with new array)
 * @param alphas IntArray to populate with letter group positions
 * @param map Optional StringMap of filename->alias mappings (NULL to skip aliasing)
 * @param skip_alpha_index If true, skip building alphabetical index
 * @return Updated entries array (may be different from input if filtering occurred)
 */
Array* DirectoryIndex_index(Array* entries, IntArray* alphas, StringMap* map, int skip_alpha_index);

#endif // DIRECTORY_INDEX_H
