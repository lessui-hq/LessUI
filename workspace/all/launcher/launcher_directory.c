/**
 * launcher_directory.c - Directory building utilities for Launcher
 *
 * Provides testable functions for building directory entry lists.
 * Extracted from launcher.c for improved testability.
 */

#include "launcher_directory.h"
#include "launcher_file_utils.h"
#include "utils.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Checks if a path is a top-level console directory.
 *
 * A console directory is one whose parent is the Roms directory.
 */
int LauncherDir_isConsoleDir(const char* path, const char* roms_path) {
	if (!path || !roms_path) {
		return 0;
	}

	char parent_dir[LAUNCHER_DIR_MAX_PATH];
	strncpy(parent_dir, path, sizeof(parent_dir) - 1);
	parent_dir[sizeof(parent_dir) - 1] = '\0';

	char* tmp = strrchr(parent_dir, '/');
	if (!tmp || tmp == parent_dir) {
		return 0;
	}
	tmp[0] = '\0';

	return exactMatch(parent_dir, roms_path);
}

/**
 * Determines the entry type for a directory entry.
 */
int LauncherDir_determineEntryType(const char* filename, int is_dir, const char* parent_path,
                                   const char* collections_path) {
	if (!filename) {
		return ENTRY_ROM; // Default fallback
	}

	if (is_dir) {
		// Check if it's a .pak application
		if (suffixMatch(".pak", (char*)filename)) {
			return ENTRY_PAK;
		}
		return ENTRY_DIR;
	}

	// Not a directory - check if we're in collections
	if (collections_path && parent_path &&
	    prefixMatch((char*)collections_path, (char*)parent_path)) {
		// Collection entries are treated as pseudo-directories
		return ENTRY_DIR;
	}

	return ENTRY_ROM;
}

/**
 * Checks if a ROM system directory has any playable ROMs.
 */
int LauncherDir_hasRoms(const char* dir_name, const char* roms_path, const char* paks_path,
                        const char* sdcard_path, const char* platform) {
	if (!dir_name || !roms_path || !paks_path || !sdcard_path || !platform) {
		return 0;
	}

	// Get emulator name from directory name
	char emu_name[256];
	getEmuName((char*)dir_name, emu_name);

	// Check for emulator pak
	if (!Launcher_hasEmu(emu_name, paks_path, sdcard_path, platform)) {
		return 0;
	}

	// Check for at least one non-hidden file in the ROM directory
	char rom_path[LAUNCHER_DIR_MAX_PATH];
	(void)snprintf(rom_path, sizeof(rom_path), "%s/%s", roms_path, dir_name);

	return Launcher_hasNonHiddenFiles(rom_path);
}

/**
 * Builds a collation prefix for matching related console directories.
 */
int LauncherDir_buildCollationPrefix(const char* path, char* out_prefix) {
	if (!path || !out_prefix) {
		if (out_prefix) {
			out_prefix[0] = '\0';
		}
		return 0;
	}

	strncpy(out_prefix, path, LAUNCHER_DIR_MAX_PATH - 1);
	out_prefix[LAUNCHER_DIR_MAX_PATH - 1] = '\0';

	// Find the opening parenthesis for region suffix
	char* paren = strrchr(out_prefix, '(');
	if (!paren) {
		out_prefix[0] = '\0';
		return 0;
	}

	// Keep the opening paren to avoid matching "Game Boy" with "Game Boy Advance"
	// Example: "Game Boy (USA)" -> "Game Boy ("
	paren[1] = '\0';

	return 1;
}

/**
 * Checks if a path matches a collation prefix.
 */
int LauncherDir_matchesCollation(const char* path, const char* collation_prefix) {
	if (!path || !collation_prefix || collation_prefix[0] == '\0') {
		return 0;
	}

	return prefixMatch((char*)collation_prefix, (char*)path);
}

/**
 * Creates a new scan result structure.
 */
LauncherDirScanResult* LauncherDirScanResult_new(int initial_capacity) {
	if (initial_capacity <= 0) {
		initial_capacity = 16;
	}

	LauncherDirScanResult* result = malloc(sizeof(LauncherDirScanResult));
	if (!result) {
		return NULL;
	}

	result->paths = malloc(sizeof(char*) * initial_capacity);
	result->is_dirs = malloc(sizeof(int) * initial_capacity);

	if (!result->paths || !result->is_dirs) {
		free(result->paths);
		free(result->is_dirs);
		free(result);
		return NULL;
	}

	result->count = 0;
	result->capacity = initial_capacity;
	return result;
}

/**
 * Frees a scan result structure and all its contents.
 */
void LauncherDirScanResult_free(LauncherDirScanResult* result) {
	if (!result) {
		return;
	}

	if (result->paths) {
		for (int i = 0; i < result->count; i++) {
			free(result->paths[i]);
		}
		free(result->paths);
	}

	free(result->is_dirs);
	free(result);
}

/**
 * Adds an entry to scan results.
 */
int LauncherDirScanResult_add(LauncherDirScanResult* result, const char* path, int is_dir) {
	if (!result || !path) {
		return 0;
	}

	// Grow arrays if needed
	if (result->count >= result->capacity) {
		int new_capacity = result->capacity * 2;

		// Realloc paths first, save to temp to avoid losing old pointer on failure
		char** new_paths = realloc(result->paths, sizeof(char*) * new_capacity);
		if (!new_paths) {
			return 0;
		}
		result->paths = new_paths;

		// Realloc is_dirs - if this fails, paths array is already resized (harmless)
		int* new_is_dirs = realloc(result->is_dirs, sizeof(int) * new_capacity);
		if (!new_is_dirs) {
			return 0;
		}
		result->is_dirs = new_is_dirs;
		result->capacity = new_capacity;
	}

	char* path_copy = strdup(path);
	if (!path_copy) {
		return 0;
	}

	result->paths[result->count] = path_copy;
	result->is_dirs[result->count] = is_dir;
	result->count++;

	return 1;
}

/**
 * Scans a directory and returns non-hidden entries.
 */
LauncherDirScanResult* LauncherDir_scan(const char* dir_path) {
	if (!dir_path) {
		return NULL;
	}

	DIR* dh = opendir(dir_path);
	if (!dh) {
		return NULL;
	}

	LauncherDirScanResult* result = LauncherDirScanResult_new(32);
	if (!result) {
		closedir(dh);
		return NULL;
	}

	char full_path[LAUNCHER_DIR_MAX_PATH];
	struct dirent* dp;

	while ((dp = readdir(dh)) != NULL) {
		// Skip hidden entries
		if (hide(dp->d_name)) {
			continue;
		}

		// Build full path
		(void)snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dp->d_name);

		int is_dir = (dp->d_type == DT_DIR);

		if (!LauncherDirScanResult_add(result, full_path, is_dir)) {
			// Allocation failure - return what we have
			break;
		}
	}

	closedir(dh);
	return result;
}

/**
 * Scans multiple directories with collation support.
 */
LauncherDirScanResult* LauncherDir_scanCollated(const char* roms_path,
                                                const char* collation_prefix) {
	if (!roms_path || !collation_prefix || collation_prefix[0] == '\0') {
		return NULL;
	}

	DIR* dh = opendir(roms_path);
	if (!dh) {
		return NULL;
	}

	LauncherDirScanResult* result = LauncherDirScanResult_new(64);
	if (!result) {
		closedir(dh);
		return NULL;
	}

	char full_path[LAUNCHER_DIR_MAX_PATH];
	struct dirent* dp;

	// First pass: find all matching console directories
	char** matching_dirs = NULL;
	int matching_count = 0;
	int matching_capacity = 8;

	matching_dirs = malloc(sizeof(char*) * matching_capacity);
	if (!matching_dirs) {
		closedir(dh);
		LauncherDirScanResult_free(result);
		return NULL;
	}

	while ((dp = readdir(dh)) != NULL) {
		if (hide(dp->d_name)) {
			continue;
		}
		if (dp->d_type != DT_DIR) {
			continue;
		}

		(void)snprintf(full_path, sizeof(full_path), "%s/%s", roms_path, dp->d_name);

		if (!prefixMatch((char*)collation_prefix, full_path)) {
			continue;
		}

		// This directory matches our collation
		if (matching_count >= matching_capacity) {
			matching_capacity *= 2;
			char** new_dirs = realloc(matching_dirs, sizeof(char*) * matching_capacity);
			if (!new_dirs) {
				break;
			}
			matching_dirs = new_dirs;
		}

		matching_dirs[matching_count] = strdup(full_path);
		if (matching_dirs[matching_count]) {
			matching_count++;
		}
	}

	closedir(dh);

	// Second pass: scan each matching directory and add entries
	for (int i = 0; i < matching_count; i++) {
		LauncherDirScanResult* sub_result = LauncherDir_scan(matching_dirs[i]);
		if (sub_result) {
			for (int j = 0; j < sub_result->count; j++) {
				LauncherDirScanResult_add(result, sub_result->paths[j], sub_result->is_dirs[j]);
			}
			LauncherDirScanResult_free(sub_result);
		}
		free(matching_dirs[i]);
	}

	free(matching_dirs);
	return result;
}

///////////////////////////////
// Directory structure operations
///////////////////////////////

void Directory_free(Directory* self) {
	if (!self)
		return;
	free(self->path);
	free(self->name);
	EntryArray_free(self->entries);
	IntArray_free(self->alphas);
	free(self);
}

void DirectoryArray_pop(Directory** self) {
	if (!self || arrlen(self) == 0)
		return;
	Directory_free(arrpop(self));
}

void DirectoryArray_free(Directory** self) {
	if (!self)
		return;
	int count = (int)arrlen(self);
	for (int i = 0; i < count; i++) {
		Directory_free(self[i]);
	}
	arrfree(self);
}
