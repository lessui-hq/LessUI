/**
 * collection_parser.c - Collection file parser for custom ROM lists
 *
 * Extracted from minui.c for testability.
 */

#define _POSIX_C_SOURCE 200809L // Required for strdup()

#include "collection_parser.h"
#include "log.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Parses a collection file and returns valid ROM entries.
 *
 * Reads the collection .txt file, validates each ROM exists, and
 * creates entries for valid ROMs only.
 *
 * @param collection_path Full path to collection .txt file
 * @param sdcard_path SDCARD_PATH constant
 * @param entry_count Output: number of entries found
 * @return Array of Collection_Entry* (caller must free with Collection_freeEntries)
 */
Collection_Entry** Collection_parse(char* collection_path, const char* sdcard_path,
                                    int* entry_count) {
	*entry_count = 0;

	// Allocate space for up to 100 entries
	Collection_Entry** entries = malloc(sizeof(Collection_Entry*) * 100);
	if (!entries) {
		LOG_error("Failed to allocate memory for collection entries");
		return NULL;
	}

	FILE* file = fopen(collection_path, "r");
	if (!file) {
		LOG_errno("Failed to open collection file %s", collection_path);
		return entries;
	}

	{
		char line[256];
		while (fgets(line, 256, file) != NULL && *entry_count < 100) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// Construct full path (collection paths are relative to sdcard)
			char sd_path[256];
			sprintf(sd_path, "%s%s", sdcard_path, line);

			// Only include ROMs that exist
			if (exists(sd_path)) {
				Collection_Entry* entry = malloc(sizeof(Collection_Entry));
				if (!entry) {
					LOG_warn("Failed to allocate memory for collection entry");
					continue;
				}

				entry->path = strdup(sd_path);
				if (!entry->path) {
					LOG_warn("Failed to duplicate collection path: %s", sd_path);
					free(entry);
					continue;
				}
				entry->is_pak = suffixMatch(".pak", sd_path);

				entries[*entry_count] = entry;
				(*entry_count)++;
			}
		}
		fclose(file);
	}

	return entries;
}

/**
 * Frees entry array returned by Collection_parse()
 */
void Collection_freeEntries(Collection_Entry** entries, int count) {
	for (int i = 0; i < count; i++) {
		free(entries[i]->path);
		free(entries[i]);
	}
	free(entries);
}
