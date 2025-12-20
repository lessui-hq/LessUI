/**
 * launcher_m3u.c - M3U playlist parser for multi-disc games
 */

#define _POSIX_C_SOURCE 200809L // Required for strdup()

#include "launcher_m3u.h"
#include "defines.h"
#include "log.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Gets the path to the first disc in an M3U playlist.
 *
 * Reads the M3U file, finds the first non-empty line, constructs
 * the full path to the disc image, and verifies it exists.
 *
 * @param m3u_path Full path to the .m3u file
 * @param disc_path Output buffer for first disc path (min 256 bytes)
 * @return 1 if first disc found and exists, 0 otherwise
 */
int M3U_getFirstDisc(char* m3u_path, char* disc_path) {
	int found = 0;

	// Extract base directory from M3U path
	char base_path[256];
	SAFE_STRCPY(base_path, m3u_path);
	char* tmp = strrchr(base_path, '/');
	if (tmp) {
		tmp += 1;
		tmp[0] = '\0'; // Terminate at the slash, keeping directory
	}

	// Open and parse M3U file
	FILE* file = fopen(m3u_path, "r");
	if (!file) {
		LOG_errno("Failed to open M3U file %s", m3u_path);
		return 0;
	}

	{
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// Construct full disc path (M3U paths are relative to M3U location)
			(void)snprintf(disc_path, MAX_PATH, "%s%s", base_path, line);

			// Verify disc exists
			if (exists(disc_path))
				found = 1;
			break; // Only need first disc
		}
		(void)fclose(file); // M3U file opened for reading
	}
	return found;
}

/**
 * Gets all discs from an M3U playlist.
 *
 * Reads the M3U file and creates a list of all valid disc entries.
 * Each disc is numbered sequentially (Disc 1, Disc 2, etc.).
 *
 * @param m3u_path Full path to .m3u file
 * @param disc_count Output: number of discs found
 * @return Array of M3U_Disc* (caller must free with M3U_freeDiscs)
 */
M3U_Disc** M3U_getAllDiscs(char* m3u_path, int* disc_count) {
	*disc_count = 0;

	// Allocate space for up to 10 discs
	M3U_Disc** discs = malloc(sizeof(M3U_Disc*) * 10);
	if (!discs) {
		LOG_error("Failed to allocate memory for M3U discs");
		return NULL;
	}

	// Extract base directory from M3U path
	char base_path[256];
	SAFE_STRCPY(base_path, m3u_path);
	char* tmp = strrchr(base_path, '/');
	if (tmp) {
		tmp += 1;
		tmp[0] = '\0';
	}

	// Read M3U file
	FILE* file = fopen(m3u_path, "r");
	if (!file) {
		LOG_errno("Failed to open M3U file %s", m3u_path);
		return discs;
	}

	{
		char line[MAX_PATH];
		int disc_num = 0;

		while (fgets(line, sizeof(line), file) != NULL && *disc_count < 10) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// Construct full disc path
			char disc_path[MAX_PATH];
			(void)snprintf(disc_path, sizeof(disc_path), "%s%s", base_path, line);

			// Only include discs that exist
			if (exists(disc_path)) {
				disc_num++;

				M3U_Disc* disc = malloc(sizeof(M3U_Disc));
				if (!disc) {
					LOG_warn("Failed to allocate memory for M3U disc");
					continue;
				}

				disc->path = strdup(disc_path);
				if (!disc->path) {
					LOG_warn("Failed to duplicate disc path: %s", disc_path);
					free(disc);
					continue;
				}

				char name[24];
				(void)snprintf(name, sizeof(name), "Disc %i", disc_num);
				disc->name = strdup(name);
				if (!disc->name) {
					LOG_warn("Failed to duplicate disc name: %s", name);
					free(disc->path);
					free(disc);
					continue;
				}

				disc->disc_number = disc_num;

				discs[*disc_count] = disc;
				(*disc_count)++;
			}
		}
		(void)fclose(file); // M3U file opened for reading
	}

	return discs;
}

/**
 * Frees disc array returned by M3U_getAllDiscs()
 */
void M3U_freeDiscs(M3U_Disc** discs, int count) {
	for (int i = 0; i < count; i++) {
		free(discs[i]->path);
		free(discs[i]->name);
		free(discs[i]);
	}
	free(discs);
}
