/**
 * m3u_parser.c - M3U playlist parser for multi-disc games
 *
 * Extracted from minui.c for testability.
 */

#define _POSIX_C_SOURCE 200809L // Required for strdup()

#include "m3u_parser.h"
#include "utils.h"
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
	strcpy(base_path, m3u_path);
	char* tmp = strrchr(base_path, '/');
	if (tmp) {
		tmp += 1;
		tmp[0] = '\0'; // Terminate at the slash, keeping directory
	}

	// Open and parse M3U file
	FILE* file = fopen(m3u_path, "r");
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// Construct full disc path (M3U paths are relative to M3U location)
			sprintf(disc_path, "%s%s", base_path, line);

			// Verify disc exists
			if (exists(disc_path))
				found = 1;
			break; // Only need first disc
		}
		fclose(file);
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

	// Extract base directory from M3U path
	char base_path[256];
	strcpy(base_path, m3u_path);
	char* tmp = strrchr(base_path, '/');
	if (tmp) {
		tmp += 1;
		tmp[0] = '\0';
	}

	// Read M3U file
	FILE* file = fopen(m3u_path, "r");
	if (file) {
		char line[256];
		int disc_num = 0;

		while (fgets(line, 256, file) != NULL && *disc_count < 10) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// Construct full disc path
			char disc_path[256];
			sprintf(disc_path, "%s%s", base_path, line);

			// Only include discs that exist
			if (exists(disc_path)) {
				disc_num++;

				M3U_Disc* disc = malloc(sizeof(M3U_Disc));
				disc->path = strdup(disc_path);

				char name[16];
				sprintf(name, "Disc %i", disc_num);
				disc->name = strdup(name);
				disc->disc_number = disc_num;

				discs[*disc_count] = disc;
				(*disc_count)++;
			}
		}
		fclose(file);
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
