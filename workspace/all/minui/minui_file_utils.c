/**
 * minui_file_utils.c - File and directory checking utilities for MinUI
 *
 * Extracted from minui.c for testability.
 */

#include "minui_file_utils.h"
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
int MinUI_hasEmu(char* emu_name, const char* paks_path, const char* sdcard_path,
                 const char* platform) {
	char pak_path[256];

	// Try shared location first
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", paks_path, emu_name);
	if (exists(pak_path))
		return 1;

	// Try platform-specific location
	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", sdcard_path, platform, emu_name);
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
int MinUI_hasCue(char* dir_path, char* cue_path) {
	char* tmp = strrchr(dir_path, '/');
	if (!tmp)
		return 0;

	tmp += 1; // Move past the slash to get folder name
	sprintf(cue_path, "%s/%s.cue", dir_path, tmp);
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
int MinUI_hasM3u(char* rom_path, char* m3u_path) {
	char* tmp;

	// Start with rom_path: /Roms/PS1/FF7/disc1.bin
	strcpy(m3u_path, rom_path);

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
	strcpy(dir_name, tmp); // dir_name = "/FF7"

	// Remove the game directory: /Roms/PS1
	tmp[0] = '\0';

	// Append game directory name: /Roms/PS1/FF7
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, dir_name);

	// Add extension: /Roms/PS1/FF7.m3u
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, ".m3u");

	return exists(m3u_path);
}

/**
 * Checks if a directory contains any non-hidden files.
 *
 * @param dir_path Full path to directory
 * @return 1 if directory exists and contains non-hidden files, 0 otherwise
 */
int MinUI_hasNonHiddenFiles(const char* dir_path) {
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
