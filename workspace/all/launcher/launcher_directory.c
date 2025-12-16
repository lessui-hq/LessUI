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
		if (suffixMatch(".pak", filename)) {
			return ENTRY_PAK;
		}
		return ENTRY_DIR;
	}

	// Not a directory - check if we're in collections
	if (collections_path && parent_path && prefixMatch(collections_path, parent_path)) {
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
	getEmuName(dir_name, emu_name);

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

	return prefixMatch(collation_prefix, path);
}

/**
 * Frees a scan result and all its contents.
 */
void LauncherDirScanResult_free(LauncherDirScanResult result) {
	if (!result) {
		return;
	}

	for (int i = 0; i < arrlen(result); i++) {
		free(result[i].path);
	}
	arrfree(result);
}

/**
 * Adds an entry to scan results.
 */
int LauncherDirScanResult_add(LauncherDirScanResult* result_ptr, const char* path, int is_dir) {
	if (!result_ptr || !path) {
		return 0;
	}

	char* path_copy = strdup(path);
	if (!path_copy) {
		return 0;
	}

	LauncherDirScanEntry entry = {.path = path_copy, .is_dir = is_dir};
	arrpush(*result_ptr, entry);

	return 1;
}

/**
 * Scans a directory and returns non-hidden entries.
 */
LauncherDirScanResult LauncherDir_scan(const char* dir_path) {
	if (!dir_path) {
		return NULL;
	}

	DIR* dh = opendir(dir_path);
	if (!dh) {
		return NULL;
	}

	LauncherDirScanResult result = NULL;

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

		if (!LauncherDirScanResult_add(&result, full_path, is_dir)) {
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
LauncherDirScanResult LauncherDir_scanCollated(const char* roms_path,
                                               const char* collation_prefix) {
	if (!roms_path || !collation_prefix || collation_prefix[0] == '\0') {
		return NULL;
	}

	DIR* dh = opendir(roms_path);
	if (!dh) {
		return NULL;
	}

	LauncherDirScanResult result = NULL;

	char full_path[LAUNCHER_DIR_MAX_PATH];
	struct dirent* dp;

	// First pass: find all matching console directories (using stb_ds)
	char** matching_dirs = NULL;

	while ((dp = readdir(dh)) != NULL) {
		if (hide(dp->d_name)) {
			continue;
		}
		if (dp->d_type != DT_DIR) {
			continue;
		}

		(void)snprintf(full_path, sizeof(full_path), "%s/%s", roms_path, dp->d_name);

		if (!prefixMatch(collation_prefix, full_path)) {
			continue;
		}

		// This directory matches our collation
		char* dir_copy = strdup(full_path);
		if (dir_copy) {
			arrpush(matching_dirs, dir_copy);
		}
	}

	closedir(dh);

	// Second pass: scan each matching directory and add entries
	for (int i = 0; i < arrlen(matching_dirs); i++) {
		LauncherDirScanResult sub_result = LauncherDir_scan(matching_dirs[i]);
		if (sub_result) {
			for (int j = 0; j < arrlen(sub_result); j++) {
				LauncherDirScanResult_add(&result, sub_result[j].path, sub_result[j].is_dir);
			}
			LauncherDirScanResult_free(sub_result);
		}
		free(matching_dirs[i]);
	}

	arrfree(matching_dirs);
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
