/**
 * player_utils.c - Pure utility functions extracted from player.c
 *
 * These functions have no external dependencies and can be tested in isolation.
 *
 * For option-related functions, see player_options.c
 * For CPU frequency functions, see player_cpu.c
 */

#include "player_utils.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

// Use libgen.h basename on POSIX systems, or provide fallback
#ifdef _WIN32
static char* basename_impl(const char* path) {
	const char* base = strrchr(path, '/');
	if (!base)
		base = strrchr(path, '\\');
	return (char*)(base ? base + 1 : path);
}
#define basename basename_impl
#else
#include <libgen.h>
#endif

void PlayerUtils_getCoreName(const char* in_name, char* out_name) {
	// Copy basename to output (handles paths like "/path/to/core_libretro.so")
	char temp[256];
	strncpy(temp, in_name, sizeof(temp) - 1);
	temp[sizeof(temp) - 1] = '\0';

	char* base = basename(temp);
	safe_strcpy(out_name, base, 256); // Assume out_name is at least 256 bytes like temp

	// Find the last underscore and truncate there
	// "fceumm_libretro.so" -> "fceumm"
	char* underscore = strrchr(out_name, '_');
	if (underscore) {
		*underscore = '\0';
	}
}

int PlayerUtils_replaceString(char* line, const char* search, const char* replace) {
	char* sp; // start of pattern

	if ((sp = strstr(line, search)) == NULL) {
		return 0;
	}

	int count = 1;
	int sLen = strlen(search);
	int rLen = strlen(replace);

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
		int tLen = strlen(sp) - sLen;
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
	count += PlayerUtils_replaceString(sp + rLen, search, replace);

	return count;
}

char* PlayerUtils_escapeSingleQuotes(char* str) {
	PlayerUtils_replaceString(str, "'", "'\\''");
	return str;
}
