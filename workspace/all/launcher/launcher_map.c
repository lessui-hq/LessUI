/**
 * launcher_map.c - ROM display name aliasing via map.txt files
 */

#include "launcher_map.h"
#include "defines.h"
#include "log.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Frees a MapEntry hash map (values and map itself).
 */
void Map_free(MapEntry* map) {
	if (!map)
		return;
	for (ptrdiff_t i = 0; i < shlen(map); i++) {
		free(map[i].value);
	}
	shfree(map);
}

/**
 * Loads a map.txt file into a stb_ds hash map.
 */
MapEntry* Map_load(const char* map_path) {
	if (!exists(map_path))
		return NULL;

	FILE* file = fopen(map_path, "r");
	if (!file) {
		LOG_debug("Could not open map file %s: %s", map_path, strerror(errno));
		return NULL;
	}

	MapEntry* map = NULL;
	sh_new_strdup(map); // Enable key copying

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
			// Check if key already exists (update value)
			ptrdiff_t idx = shgeti(map, key);
			if (idx >= 0) {
				char* value_copy = strdup(value);
				if (value_copy) {
					free(map[idx].value);
					map[idx].value = value_copy;
				}
			} else {
				char* value_copy = strdup(value);
				if (value_copy) {
					shput(map, key, value_copy);
				}
			}
		}
	}
	(void)fclose(file);

	return map;
}

/**
 * Finds the pak map.txt path for a given emulator name.
 */
static int getPakMapPathForEmu(const char* emu_name, char* pak_map_path) {
	char relative_path[512];
	(void)sprintf(relative_path, "paks/Emus/%s.pak/map.txt", emu_name);
	return findSystemFile(relative_path, pak_map_path);
}

/**
 * Finds the map.txt path in the associated pak for a ROM.
 */
static int getPakMapPath(const char* rom_path, char* pak_map_path) {
	char emu_name[256];
	getEmuName(rom_path, emu_name);
	return getPakMapPathForEmu(emu_name, pak_map_path);
}

/**
 * Loads merged maps for a ROM directory (pak-bundled + user overrides).
 */
MapEntry* Map_loadForDirectory(const char* dir_path) {
	char user_map_path[512];
	char pak_map_path[512];
	char emu_name[256];

	// Get emulator name from directory path
	getEmuName(dir_path, emu_name);

	// Build user map path
	(void)snprintf(user_map_path, sizeof(user_map_path), "%s/map.txt", dir_path);

	// Check what maps exist
	int has_pak_map = getPakMapPathForEmu(emu_name, pak_map_path);
	int has_user_map = exists(user_map_path);

	if (!has_pak_map && !has_user_map)
		return NULL;

	// If only one map exists, just load it directly
	if (has_pak_map && !has_user_map)
		return Map_load(pak_map_path);
	if (has_user_map && !has_pak_map)
		return Map_load(user_map_path);

	// Both exist - load pak first, then apply user overrides
	MapEntry* merged = Map_load(pak_map_path);
	if (!merged) {
		// Pak load failed, try user map
		return Map_load(user_map_path);
	}

	// Load user map and merge (user entries override pak entries)
	FILE* file = fopen(user_map_path, "r");
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue;
			char* tmp = strchr(line, '\t');
			if (tmp) {
				tmp[0] = '\0';
				char* key = line;
				char* value = tmp + 1;
				// Update or insert
				ptrdiff_t idx = shgeti(merged, key);
				if (idx >= 0) {
					char* value_copy = strdup(value);
					if (value_copy) {
						free(merged[idx].value);
						merged[idx].value = value_copy;
					}
				} else {
					char* value_copy = strdup(value);
					if (value_copy) {
						shput(merged, key, value_copy);
					}
				}
			}
		}
		(void)fclose(file);
	}

	return merged;
}

/**
 * Looks up the display alias for a ROM file from map.txt.
 */
char* Map_getAlias(const char* path, char* alias) {
	if (!path || !alias)
		return alias;

	char* tmp;
	char user_map_path[256];
	char pak_map_path[256];

	// Extract filename from path
	char* file_name = strrchr(path, '/');
	if (!file_name)
		return alias;
	file_name += 1;

	// Build user map path (ROM directory)
	SAFE_STRCPY(user_map_path, path);
	tmp = strrchr(user_map_path, '/');
	if (tmp) {
		tmp += 1;
		safe_strcpy(tmp, "map.txt", sizeof(user_map_path) - (tmp - user_map_path));
	}

	// Try user map first (highest priority)
	MapEntry* user_map = Map_load(user_map_path);
	if (user_map) {
		char* found = shget(user_map, file_name);
		if (found) {
			safe_strcpy(alias, found, MAX_PATH);
			Map_free(user_map);
			return alias;
		}
		Map_free(user_map);
	}

	// Fall back to pak-bundled map
	if (getPakMapPath(path, pak_map_path)) {
		MapEntry* pak_map = Map_load(pak_map_path);
		if (pak_map) {
			char* found = shget(pak_map, file_name);
			if (found) {
				safe_strcpy(alias, found, MAX_PATH);
			}
			Map_free(pak_map);
		}
	}

	return alias;
}
