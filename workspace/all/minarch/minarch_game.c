/**
 * minarch_game.c - Game file loading utilities
 *
 * Provides functions for game file handling including:
 * - ZIP archive entry detection and extension matching
 * - M3U playlist detection for multi-disc games
 * - Extension list parsing
 *
 * Extracted from minarch.c for testability.
 */

#include "minarch_game.h"

#include <string.h>

// For exists() function - only needed for detectM3uPath
#ifndef MINARCH_GAME_TEST
#include "utils.h"
#endif

int MinArchGame_parseExtensions(char* extensions_str, char** out_extensions, int max_extensions) {
	if (!extensions_str || !out_extensions || max_extensions <= 0)
		return 0;

	int count = 0;
	char* ext = strtok(extensions_str, "|");
	while (ext && count < max_extensions) {
		out_extensions[count++] = ext;
		ext = strtok(NULL, "|");
	}

	// NULL-terminate the array
	if (count < max_extensions)
		out_extensions[count] = NULL;

	return count;
}

bool MinArchGame_matchesExtension(const char* filename, char* const* extensions) {
	if (!filename || !extensions)
		return false;

	// Get the last dot in the filename
	const char* dot = strrchr(filename, '.');
	if (!dot || dot == filename)
		return false;

	// Skip the dot
	const char* file_ext = dot + 1;

	// Check against each extension
	for (int i = 0; extensions[i] != NULL; i++) {
		if (strcasecmp(file_ext, extensions[i]) == 0) {
			return true;
		}
	}

	return false;
}


bool MinArchGame_buildM3uPath(const char* rom_path, char* out_m3u_path, size_t m3u_path_size) {
	if (!rom_path || !out_m3u_path || m3u_path_size == 0)
		return false;

	// Copy path
	size_t path_len = strlen(rom_path);
	if (path_len >= m3u_path_size)
		return false;

	strcpy(out_m3u_path, rom_path);

	// Find the filename part and remove it
	// "/path/to/Game (Disc 1)/image.cue" -> "/path/to/Game (Disc 1)/"
	char* last_slash = strrchr(out_m3u_path, '/');
	if (!last_slash)
		return false;

	// File directly in root (e.g., "/file.bin") - no parent directory possible
	if (last_slash == out_m3u_path)
		return false;

	last_slash[0] = '\0'; // Remove filename: "/path/to/Game (Disc 1)"

	// Find the parent directory
	// "/path/to/Game (Disc 1)" -> need to find "/" before "Game (Disc 1)"
	char* second_last_slash = strrchr(out_m3u_path, '/');
	if (!second_last_slash)
		return false;

	// File in a directory directly under root (e.g., "/folder/file.bin")
	// Would result in M3U at root level which doesn't make sense
	if (second_last_slash == out_m3u_path)
		return false;

	// Get the directory name (will be used for m3u filename)
	// "Game (Disc 1)"
	char dir_name[256];
	size_t dir_name_len = strlen(second_last_slash);
	if (dir_name_len >= sizeof(dir_name))
		return false;

	strcpy(dir_name, second_last_slash); // Includes leading slash: "/Game (Disc 1)"

	// Truncate to parent: "/path/to"
	second_last_slash[0] = '\0';

	// Build final path: "/path/to" + "/Game (Disc 1)" + ".m3u"
	size_t base_len = strlen(out_m3u_path);
	size_t needed = base_len + dir_name_len + 4 + 1; // +4 for ".m3u", +1 for null
	if (needed > m3u_path_size)
		return false;

	strcat(out_m3u_path, dir_name);
	strcat(out_m3u_path, ".m3u");

	return true;
}

#ifndef MINARCH_GAME_TEST
bool MinArchGame_detectM3uPath(const char* rom_path, char* out_m3u_path, size_t m3u_path_size) {
	if (!MinArchGame_buildM3uPath(rom_path, out_m3u_path, m3u_path_size))
		return false;

	return exists(out_m3u_path) != 0;
}
#endif
