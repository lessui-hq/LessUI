/**
 * directory_index.c - Directory indexing for Launcher file browser
 *
 * Implements directory entry indexing: aliasing, filtering, duplicate
 * detection, and alphabetical navigation.
 */

#include "directory_index.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Gets the alphabetical index for a sort key.
 */
int DirectoryIndex_getAlphaChar(const char* sort_key) {
	if (!sort_key || !sort_key[0])
		return 0;
	char c = tolower(sort_key[0]);
	if (c >= 'a' && c <= 'z')
		return (c - 'a') + 1;
	return 0;
}

/**
 * Generates a unique disambiguation string for an entry.
 */
void DirectoryIndex_getUniqueName(const char* entry_name, const char* entry_path, char* out_name) {
	char emu_tag[256];
	getEmuName(entry_path, emu_tag);

	// Format: "name (emu_tag)"
	(void)snprintf(out_name, 256, "%s (%s)", entry_name, emu_tag);
}

/**
 * Applies map.txt aliases to entries.
 */
int DirectoryIndex_applyAliases(Array* entries, MapEntry* map) {
	if (!entries || !map)
		return 0;

	int resort = 0;
	for (int i = 0; i < entries->count; i++) {
		Entry* entry = entries->items[i];
		char* filename = strrchr(entry->path, '/');
		if (!filename)
			continue;
		filename++; // Skip the '/'

		char* alias = shget(map, filename);
		if (alias) {
			if (Entry_setName(entry, alias))
				resort = 1;
		}
	}

	return resort;
}

/**
 * Removes hidden entries from an array.
 */
Array* DirectoryIndex_filterHidden(Array* entries) {
	if (!entries)
		return NULL;

	Array* result = Array_new();
	if (!result)
		return NULL;

	for (int i = 0; i < entries->count; i++) {
		Entry* entry = entries->items[i];
		if (hide(entry->name)) {
			Entry_free(entry);
		} else {
			Array_push(result, entry);
		}
	}

	return result;
}

/**
 * Checks if any entry has a hidden name.
 */
static int hasHiddenEntries(Array* entries) {
	for (int i = 0; i < entries->count; i++) {
		Entry* entry = entries->items[i];
		if (hide(entry->name))
			return 1;
	}
	return 0;
}

/**
 * Marks entries with duplicate display names for disambiguation.
 */
void DirectoryIndex_markDuplicates(Array* entries) {
	if (!entries || entries->count < 2)
		return;

	Entry* prior = NULL;
	for (int i = 0; i < entries->count; i++) {
		Entry* entry = entries->items[i];

		// Detect duplicate display names
		if (prior != NULL && exactMatch(prior->name, entry->name)) {
			// Free any existing unique strings
			if (prior->unique) {
				free(prior->unique);
				prior->unique = NULL;
			}
			if (entry->unique) {
				free(entry->unique);
				entry->unique = NULL;
			}

			const char* prior_filename = strrchr(prior->path, '/');
			const char* entry_filename = strrchr(entry->path, '/');

			// Handle edge case of no slash in path
			prior_filename = prior_filename ? prior_filename + 1 : prior->path;
			entry_filename = entry_filename ? entry_filename + 1 : entry->path;

			if (exactMatch(prior_filename, entry_filename)) {
				// Same filename (cross-platform ROM) - use emulator name
				char prior_unique[256];
				char entry_unique[256];
				DirectoryIndex_getUniqueName(prior->name, prior->path, prior_unique);
				DirectoryIndex_getUniqueName(entry->name, entry->path, entry_unique);

				prior->unique = strdup(prior_unique);
				entry->unique = strdup(entry_unique);
			} else {
				// Different filenames - show them
				prior->unique = strdup(prior_filename);
				entry->unique = strdup(entry_filename);
			}
		}

		prior = entry;
	}
}

/**
 * Builds alphabetical navigation index.
 */
void DirectoryIndex_buildAlphaIndex(Array* entries, IntArray* alphas) {
	if (!entries || !alphas)
		return;

	int alpha = -1;
	int index = 0;

	for (int i = 0; i < entries->count; i++) {
		Entry* entry = entries->items[i];

		int a = DirectoryIndex_getAlphaChar(entry->sort_key);
		if (a != alpha) {
			index = alphas->count;
			IntArray_push(alphas, i);
			alpha = a;
		}
		entry->alpha = index;
	}
}

/**
 * Performs full directory indexing.
 */
Array* DirectoryIndex_index(Array* entries, IntArray* alphas, MapEntry* map,
                            int skip_alpha_index) {
	if (!entries)
		return NULL;

	Array* result = entries;

	// Apply aliases from map
	if (map) {
		int resort = DirectoryIndex_applyAliases(result, map);

		// Filter hidden entries if any aliases created hidden names
		if (hasHiddenEntries(result)) {
			Array* filtered = DirectoryIndex_filterHidden(result);
			if (filtered) {
				Array_free(result); // Don't use EntryArray_free - entries were moved
				result = filtered;
			}
		}

		// Re-sort if any names changed
		if (resort)
			EntryArray_sort(result);
	}

	// Mark duplicates
	DirectoryIndex_markDuplicates(result);

	// Build alphabetical index
	if (!skip_alpha_index && alphas)
		DirectoryIndex_buildAlphaIndex(result, alphas);

	return result;
}
