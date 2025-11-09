/**
 * map_parser.c - ROM display name aliasing via map.txt files
 *
 * Extracted from minarch.c for testability.
 */

#include "map_parser.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

/**
 * Looks up the display alias for a ROM file from map.txt.
 *
 * Searches for map.txt in the same directory as the ROM file.
 * If found, looks up the ROM's filename in the map and returns the alias.
 *
 * @param path Full path to ROM file
 * @param alias Output buffer for alias (min 256 bytes)
 * @return Pointer to alias buffer (even if no alias found)
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

	// Read map.txt if it exists
	if (exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			char line[256];
			while (fgets(line, 256, file) != NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line) == 0)
					continue; // skip empty lines

				// Parse tab-delimited format: filename<TAB>alias
				tmp = strchr(line, '\t');
				if (tmp) {
					tmp[0] = '\0';
					char* key = line;
					char* value = tmp + 1;

					// Found matching ROM?
					if (exactMatch(file_name, key)) {
						strcpy(alias, value);
						break;
					}
				}
			}
			fclose(file);
		}
	}

	return alias;
}
