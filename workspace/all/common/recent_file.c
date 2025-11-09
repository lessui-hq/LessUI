/**
 * recent_file.c - Recently played games file I/O
 *
 * Extracted from minui.c for testability.
 */

#define _POSIX_C_SOURCE 200809L // Required for strdup()

#include "recent_file.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Parses recent.txt and returns all valid entries.
 *
 * Reads the recent.txt file, validates each ROM exists, and creates
 * entries for valid ROMs only.
 *
 * Format: path<TAB>alias (alias is optional)
 *
 * @param recent_path Full path to recent.txt file
 * @param sdcard_path SDCARD_PATH constant
 * @param entry_count Output: number of entries found
 * @return Array of Recent_Entry* (caller must free with Recent_freeEntries)
 */
Recent_Entry** Recent_parse(char* recent_path, const char* sdcard_path, int* entry_count) {
	*entry_count = 0;

	// Allocate space for up to 50 recents
	Recent_Entry** entries = malloc(sizeof(Recent_Entry*) * 50);

	FILE* file = fopen(recent_path, "r");
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL && *entry_count < 50) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// Parse tab-delimited format: path<TAB>alias
			char* path = line;
			char* alias = NULL;
			char* tmp = strchr(line, '\t');
			if (tmp) {
				tmp[0] = '\0';
				alias = tmp + 1;
			}

			// Construct full SD card path
			char sd_path[256];
			sprintf(sd_path, "%s%s", sdcard_path, path);

			// Only include ROMs that exist
			if (exists(sd_path)) {
				Recent_Entry* entry = malloc(sizeof(Recent_Entry));
				entry->path = strdup(path); // Store relative path
				entry->alias = alias ? strdup(alias) : NULL;

				entries[*entry_count] = entry;
				(*entry_count)++;
			}
		}
		fclose(file);
	}

	return entries;
}

/**
 * Saves recent entries to recent.txt file
 */
int Recent_save(char* recent_path, Recent_Entry** entries, int count) {
	FILE* file = fopen(recent_path, "w");
	if (!file) {
		return 0;
	}

	for (int i = 0; i < count; i++) {
		Recent_Entry* entry = entries[i];
		fputs(entry->path, file);
		if (entry->alias) {
			fputs("\t", file);
			fputs(entry->alias, file);
		}
		putc('\n', file);
	}

	fclose(file);
	return 1;
}

/**
 * Frees entry array returned by Recent_parse()
 */
void Recent_freeEntries(Recent_Entry** entries, int count) {
	for (int i = 0; i < count; i++) {
		free(entries[i]->path);
		if (entries[i]->alias)
			free(entries[i]->alias);
		free(entries[i]);
	}
	free(entries);
}
