/**
 * minarch_utils.c - Pure utility functions extracted from minarch.c
 *
 * These functions have no external dependencies and can be tested in isolation.
 */

#include "minarch_utils.h"

#include <stdlib.h>
#include <string.h>

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

void MinArch_getCoreName(const char* in_name, char* out_name) {
	// Copy basename to output (handles paths like "/path/to/core_libretro.so")
	char temp[256];
	strncpy(temp, in_name, sizeof(temp) - 1);
	temp[sizeof(temp) - 1] = '\0';

	char* base = basename(temp);
	strcpy(out_name, base);

	// Find the last underscore and truncate there
	// "fceumm_libretro.so" -> "fceumm"
	char* underscore = strrchr(out_name, '_');
	if (underscore) {
		*underscore = '\0';
	}
}

int MinArch_getOptionValueIndex(const MinArchOption* opt, const char* value) {
	if (!value || !opt || !opt->values) {
		return 0;
	}

	for (int i = 0; i < opt->count; i++) {
		if (opt->values[i] && strcmp(opt->values[i], value) == 0) {
			return i;
		}
	}

	return 0;
}

int MinArch_findNearestFrequency(const int* frequencies, int count, int target_khz) {
	if (count <= 0 || !frequencies) {
		return 0;
	}

	int best_idx = 0;
	int best_diff = abs(frequencies[0] - target_khz);

	for (int i = 1; i < count; i++) {
		int diff = abs(frequencies[i] - target_khz);
		if (diff < best_diff) {
			best_diff = diff;
			best_idx = i;
		}
	}

	return best_idx;
}

int MinArch_replaceString(char* line, const char* search, const char* replace) {
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
	count += MinArch_replaceString(sp + rLen, search, replace);

	return count;
}

char* MinArch_escapeSingleQuotes(char* str) {
	MinArch_replaceString(str, "'", "'\\''");
	return str;
}
