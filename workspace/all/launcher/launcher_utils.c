/**
 * launcher_utils.c - Helper utilities for Launcher launcher
 *
 * Pure logic functions extracted from launcher.c.
 */

#include "launcher_utils.h"
#include "utils.h"
#include <ctype.h>
#include <string.h>

/**
 * Gets alphabetical index character for a string.
 *
 * Returns index 1-26 for strings starting with a-z (case-insensitive).
 * Returns 0 for strings starting with non-letters.
 *
 * Note: When used for L1/R1 navigation indexing, pass entry->sort_key
 * (which has leading articles stripped) to match sort order.
 *
 * @param str String to get index for (typically a sort_key)
 * @return Index 0-26 (0=non-letter, 1=A, 2=B, ..., 26=Z)
 */
int Launcher_getIndexChar(char* str) {
	char i = 0;
	char c = tolower(str[0]);
	if (c >= 'a' && c <= 'z')
		i = (c - 'a') + 1;
	return i;
}

/**
 * Checks if a path is a top-level console directory.
 *
 * A console directory is one whose parent equals roms_path.
 *
 * @param path Path to check
 * @param roms_path The ROMS_PATH constant (e.g., "/mnt/SDCARD/Roms")
 * @return 1 if path is a console directory, 0 otherwise
 */
int Launcher_isConsoleDir(char* path, const char* roms_path) {
	char* tmp;
	char parent_dir[256];
	SAFE_STRCPY(parent_dir, path);
	tmp = strrchr(parent_dir, '/');
	if (!tmp)
		return 0;
	tmp[0] = '\0';

	return exactMatch(parent_dir, roms_path);
}
