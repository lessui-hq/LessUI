/**
 * launcher_launcher.c - ROM and PAK launcher utilities
 *
 * Provides functions to construct shell commands for launching
 * ROMs with emulators and PAKs (application packages).
 *
 * Extracted from launcher.c for testability.
 */

#include "launcher_launcher.h"
#include <stdio.h>
#include <string.h>

int Launcher_replaceString(char* line, const char* search, const char* replace) {
	char* sp; // start of pattern
	if ((sp = strstr(line, search)) == NULL) {
		return 0;
	}
	int count = 1;
	size_t sLen = strlen(search);
	size_t rLen = strlen(replace);

	if (sLen > rLen) {
		// move from right to left
		char* src = sp + sLen;
		char* dst = sp + rLen;
		while ((*dst = *src) != '\0') {
			dst++;
			src++;
		}
	} else if (sLen < rLen) {
		// move from left to right
		size_t tLen = strlen(sp) - sLen;
		char* stop = sp + rLen;
		char* src = sp + sLen + tLen;
		char* dst = sp + rLen + tLen;
		while (dst >= stop) {
			*dst = *src;
			dst--;
			src--;
		}
	}
	memcpy(sp, replace, rLen);
	count += Launcher_replaceString(sp + rLen, search, replace);
	return count;
}

char* Launcher_escapeSingleQuotes(char* str) {
	Launcher_replaceString(str, "'", "'\\''");
	return str;
}

int Launcher_buildPakCommand(char* cmd, size_t cmd_size, char* pak_path) {
	if (!cmd || !pak_path || cmd_size == 0) {
		return 0;
	}

	// Escape quotes in the path
	Launcher_escapeSingleQuotes(pak_path);

	// Build command: '<pak_path>/launch.sh'
	int len = snprintf(cmd, cmd_size, "'%s/launch.sh'", pak_path);

	if (len < 0 || (size_t)len >= cmd_size) {
		return 0;
	}

	return len;
}

int Launcher_buildRomCommand(char* cmd, size_t cmd_size, char* emu_path, char* rom_path) {
	if (!cmd || !emu_path || !rom_path || cmd_size == 0) {
		return 0;
	}

	// Escape quotes in both paths
	Launcher_escapeSingleQuotes(emu_path);
	Launcher_escapeSingleQuotes(rom_path);

	// Build command: '<emu_path>' '<rom_path>'
	int len = snprintf(cmd, cmd_size, "'%s' '%s'", emu_path, rom_path);

	if (len < 0 || (size_t)len >= cmd_size) {
		return 0;
	}

	return len;
}

int Launcher_queueCommand(const char* filepath, const char* cmd) {
	if (!filepath || !cmd) {
		return -1;
	}

	FILE* f = fopen(filepath, "w");
	if (!f) {
		return -1;
	}

	int result = fputs(cmd, f);
	(void)fclose(f); // File was opened for writing, safe to ignore close result

	return (result >= 0) ? 0 : -1;
}

int Launcher_isRomsPath(const char* path, const char* roms_path) {
	if (!path || !roms_path) {
		return 0;
	}

	size_t roms_len = strlen(roms_path);
	if (strncmp(path, roms_path, roms_len) != 0) {
		return 0;
	}

	// Path must be exactly roms_path or have a '/' after it
	return (path[roms_len] == '\0' || path[roms_len] == '/');
}
