/**
 * minui_map.c - ROM display name aliasing via map.txt files
 */

#include "minui_map.h"
#include "defines.h"
#include "log.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Loads a map.txt file into a StringMap.
 */
StringMap* Map_load(const char* map_path) {
	if (!exists((char*)map_path))
		return NULL;

	FILE* file = fopen(map_path, "r");
	if (!file) {
		LOG_debug("Could not open map file %s: %s", map_path, strerror(errno));
		return NULL;
	}

	StringMap* map = StringMap_new();
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
			StringMap_set(map, key, value);
		}
	}
	fclose(file);

	return map;
}

/**
 * Finds the pak map.txt path for a given emulator name.
 *
 * @param emu_name Emulator name (e.g., "MAME")
 * @param pak_map_path Output buffer (min 512 bytes)
 * @return 1 if found, 0 otherwise
 */
static int getPakMapPathForEmu(const char* emu_name, char* pak_map_path) {
	char relative_path[512];
	sprintf(relative_path, "paks/Emus/%s.pak/map.txt", emu_name);
	return findSystemFile(relative_path, pak_map_path);
}

/**
 * Finds the map.txt path in the associated pak for a ROM.
 *
 * Uses the new generic system file finder to check:
 * 1. Platform-specific: /.system/{platform}/paks/Emus/{emu}.pak/map.txt
 * 2. Shared common:     /.system/common/paks/Emus/{emu}.pak/map.txt
 *
 * @param rom_path Full path to ROM file
 * @param pak_map_path Output buffer for pak's map.txt path (min 256 bytes)
 * @return 1 if pak map.txt exists, 0 otherwise
 */
static int getPakMapPath(const char* rom_path, char* pak_map_path) {
	char emu_name[256];
	getEmuName(rom_path, emu_name);
	return getPakMapPathForEmu(emu_name, pak_map_path);
}

/**
 * Loads merged maps for a ROM directory (pak-bundled + user overrides).
 */
StringMap* Map_loadForDirectory(const char* dir_path) {
	char user_map_path[512];
	char pak_map_path[512];
	char emu_name[256];

	// Get emulator name from directory path
	getEmuName(dir_path, emu_name);

	// Build user map path
	snprintf(user_map_path, sizeof(user_map_path), "%s/map.txt", dir_path);

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
	StringMap* merged = Map_load(pak_map_path);
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
				StringMap_set(merged, line, tmp + 1); // User entry overrides
			}
		}
		fclose(file);
	}

	return merged;
}

/**
 * Looks up the display alias for a ROM file from map.txt.
 *
 * Searches for map.txt in two locations with precedence:
 * 1. ROM directory (user's custom map) - highest priority
 * 2. Pak directory (pak-bundled map) - fallback
 */
char* Map_getAlias(char* path, char* alias) {
	char* tmp;
	char user_map_path[256];
	char pak_map_path[256];

	// Extract filename from path
	char* file_name = strrchr(path, '/');
	if (!file_name)
		return alias;
	file_name += 1;

	// Build user map path (ROM directory)
	strcpy(user_map_path, path);
	tmp = strrchr(user_map_path, '/');
	if (tmp) {
		tmp += 1;
		strcpy(tmp, "map.txt");
	}

	// Try user map first (highest priority)
	StringMap* user_map = Map_load(user_map_path);
	if (user_map) {
		char* found = StringMap_get(user_map, file_name);
		if (found) {
			strcpy(alias, found);
			StringMap_free(user_map);
			return alias;
		}
		StringMap_free(user_map);
	}

	// Fall back to pak-bundled map
	if (getPakMapPath(path, pak_map_path)) {
		StringMap* pak_map = Map_load(pak_map_path);
		if (pak_map) {
			char* found = StringMap_get(pak_map, file_name);
			if (found) {
				strcpy(alias, found);
			}
			StringMap_free(pak_map);
		}
	}

	return alias;
}
