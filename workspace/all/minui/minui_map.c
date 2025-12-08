/**
 * minui_map.c - ROM display name aliasing via map.txt files
 */

#include "minui_map.h"
#include "log.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Loads a map.txt file into a Hash table.
 */
Hash* Map_load(const char* map_path) {
	if (!exists((char*)map_path))
		return NULL;

	FILE* file = fopen(map_path, "r");
	if (!file) {
		LOG_debug("Could not open map file %s: %s", map_path, strerror(errno));
		return NULL;
	}

	Hash* map = Hash_new();
	if (!map) {
		fclose(file);
		return NULL;
	}

	char line[256];
	while (fgets(line, 256, file) != NULL) {
		normalizeNewline(line);
		trimTrailingNewlines(line);
		if (strlen(line) == 0)
			continue; // skip empty lines

		// Parse tab-delimited format: filename<TAB>alias
		char* tmp = strchr(line, '\t');
		if (tmp) {
			tmp[0] = '\0';
			char* key = line;
			char* value = tmp + 1;
			Hash_set(map, key, value);
		}
	}
	fclose(file);

	return map;
}

/**
 * Looks up the display alias for a ROM file from map.txt.
 *
 * Searches for map.txt in the same directory as the ROM file.
 * If found, looks up the ROM's filename in the map and returns the alias.
 */
char* Map_getAlias(char* path, char* alias) {
	char* tmp;
	char map_path[256];
	strcpy(map_path, path);

	// Replace filename with "map.txt"
	tmp = strrchr(map_path, '/');
	if (tmp) {
		tmp += 1;
		strcpy(tmp, "map.txt");
	}

	// Extract filename from full path
	char* file_name = strrchr(path, '/');
	if (file_name)
		file_name += 1;

	// Load and search map.txt
	Hash* map = Map_load(map_path);
	if (map) {
		char* found = Hash_get(map, file_name);
		if (found) {
			strcpy(alias, found);
		}
		Hash_free(map);
	}

	return alias;
}
