/**
 * launcher_file_utils.c - File and directory checking utilities for Launcher
 *
 * Extracted from launcher.c for testability.
 */

#include "launcher_file_utils.h"
#include "defines.h"
#include "utils.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

/**
 * Checks if an emulator exists for a given system.
 *
 * Searches for emulator .pak directories in two locations:
 * 1. {paks_path}/Emus/{emu_name}.pak/launch.sh (shared location)
 * 2. {sdcard_path}/Emus/{platform}/{emu_name}.pak/launch.sh (platform-specific)
 *
 * @param emu_name Emulator name (e.g., "gpsp", "gambatte")
 * @param paks_path PAKS_PATH constant
 * @param sdcard_path SDCARD_PATH constant
 * @param platform PLATFORM constant
 * @return 1 if emulator exists, 0 otherwise
 */
int Launcher_hasEmu(char* emu_name, const char* paks_path, const char* sdcard_path,
                    const char* platform) {
	char pak_path[256];

	// Try shared location first
	(void)sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", paks_path, emu_name);
	if (exists(pak_path))
		return 1;

	// Try platform-specific location
	(void)sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", sdcard_path, platform, emu_name);
	return exists(pak_path);
}

/**
 * Checks if a directory contains a .cue file for disc-based games.
 *
 * The .cue file must be named after the directory itself.
 *
 * @param dir_path Full path to directory
 * @param cue_path Output buffer for .cue file path
 * @return 1 if .cue file exists, 0 otherwise
 */
int Launcher_hasCue(char* dir_path, char* cue_path) {
	char* tmp = strrchr(dir_path, '/');
	if (!tmp)
		return 0;

	tmp += 1; // Move past the slash to get folder name
	(void)sprintf(cue_path, "%s/%s.cue", dir_path, tmp);
	return exists(cue_path);
}

/**
 * Checks if a ROM has an associated .m3u playlist for multi-disc games.
 *
 * The .m3u file must be in the parent directory and named after the game directory.
 *
 * @param rom_path Full path to ROM file
 * @param m3u_path Output buffer for .m3u file path
 * @return 1 if .m3u file exists, 0 otherwise
 */
int Launcher_hasM3u(char* rom_path, char* m3u_path) {
	char* tmp;

	// Start with rom_path: /Roms/PS1/FF7/disc1.bin
	safe_strcpy(m3u_path, rom_path, MAX_PATH);

	// Remove filename to get directory: /Roms/PS1/FF7/
	tmp = strrchr(m3u_path, '/');
	if (!tmp)
		return 0;
	tmp += 1;
	tmp[0] = '\0';

	// Remove trailing slash: /Roms/PS1/FF7
	tmp = strrchr(m3u_path, '/');
	if (!tmp)
		return 0;
	tmp[0] = '\0';

	// Get the game directory name before removing it
	char dir_name[256];
	tmp = strrchr(m3u_path, '/');
	if (!tmp)
		return 0;
	SAFE_STRCPY(dir_name, tmp); // dir_name = "/FF7"

	// Remove the game directory: /Roms/PS1
	tmp[0] = '\0';

	// Append game directory name: /Roms/PS1/FF7
	tmp = m3u_path + strlen(m3u_path);
	safe_strcpy(tmp, dir_name, MAX_PATH - (tmp - m3u_path));

	// Add extension: /Roms/PS1/FF7.m3u
	tmp = m3u_path + strlen(m3u_path);
	safe_strcpy(tmp, ".m3u", MAX_PATH - (tmp - m3u_path));

	return exists(m3u_path);
}

/**
 * Builds a thumbnail resource path for an entry.
 *
 * @param entry_path Full path to the entry
 * @param out_path Output buffer for thumbnail path (min MAX_PATH bytes)
 * @return 1 if path was built successfully, 0 on failure
 */
int Launcher_buildThumbPath(const char* entry_path, char* out_path) {
	out_path[0] = '\0';

	if (!entry_path) {
		return 0;
	}

	const char* last_slash = strrchr(entry_path, '/');
	if (!last_slash || last_slash[1] == '\0') {
		return 0;
	}

	int dir_len = (int)(last_slash - entry_path);
	if (dir_len < 0 || dir_len >= MAX_PATH - 32) {
		return 0;
	}

	// For root-level files (dir_len == 0), use "/" as directory
	if (dir_len == 0) {
		(void)snprintf(out_path, MAX_PATH, "/.res/%s.png", last_slash + 1);
	} else {
		(void)snprintf(out_path, MAX_PATH, "%.*s/.res/%s.png", dir_len, entry_path, last_slash + 1);
	}
	return 1;
}

/**
 * Checks if a directory contains any non-hidden files.
 *
 * @param dir_path Full path to directory
 * @return 1 if directory exists and contains non-hidden files, 0 otherwise
 */
int Launcher_hasNonHiddenFiles(const char* dir_path) {
	if (!exists((char*)dir_path)) {
		return 0;
	}

	DIR* dh = opendir(dir_path);
	if (!dh) {
		return 0;
	}

	struct dirent* dp;
	while ((dp = readdir(dh)) != NULL) {
		if (hide(dp->d_name)) {
			continue;
		}
		closedir(dh);
		return 1;
	}

	closedir(dh);
	return 0;
}
