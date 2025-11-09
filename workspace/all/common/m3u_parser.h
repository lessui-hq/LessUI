/**
 * m3u_parser.h - M3U playlist parser for multi-disc games
 *
 * M3U files list disc images for multi-disc games (e.g., Final Fantasy VII).
 * Format: Plain text file with one disc path per line.
 *
 * Example .m3u file:
 *   FF7 (Disc 1).bin
 *   FF7 (Disc 2).bin
 *   FF7 (Disc 3).bin
 *
 * Extracted from minui.c for testability.
 */

#ifndef __M3U_PARSER_H__
#define __M3U_PARSER_H__

/**
 * Gets the path to the first disc in an M3U playlist.
 *
 * Reads the M3U file, finds the first non-empty line, constructs
 * the full path to the disc image, and verifies it exists.
 *
 * Example:
 *   M3U file: /Roms/PS1/FF7.m3u containing "FF7 (Disc 1).bin"
 *   Returns: /Roms/PS1/FF7 (Disc 1).bin
 *
 * @param m3u_path Full path to the .m3u file
 * @param disc_path Output buffer for first disc path (min 256 bytes)
 * @return 1 if first disc found and exists, 0 otherwise
 *
 * @note Skips empty lines in the M3U file
 * @note Disc paths in M3U are relative to the M3U file's directory
 */
int M3U_getFirstDisc(char* m3u_path, char* disc_path);

/**
 * Disc entry structure for M3U disc lists
 */
typedef struct M3U_Disc {
	char* path; // Full path to disc file
	char* name; // Display name (e.g., "Disc 1", "Disc 2")
	int disc_number; // 1-based disc number
} M3U_Disc;

/**
 * Gets all discs from an M3U playlist.
 *
 * Reads the M3U file and creates a list of all valid disc entries.
 * Each disc is numbered sequentially (Disc 1, Disc 2, etc.).
 *
 * Example:
 *   M3U: "disc1.bin\ndisc2.bin\ndisc3.bin"
 *   Returns array with 3 discs named "Disc 1", "Disc 2", "Disc 3"
 *
 * @param m3u_path Full path to .m3u file
 * @param disc_count Output: number of discs found
 * @return Array of M3U_Disc* (caller must free with M3U_freeDiscs)
 *
 * @note Only includes discs that exist on filesystem
 * @note Caller must free returned array and disc structures
 */
M3U_Disc** M3U_getAllDiscs(char* m3u_path, int* disc_count);

/**
 * Frees disc array returned by M3U_getAllDiscs()
 *
 * @param discs Array of disc pointers
 * @param count Number of discs in array
 */
void M3U_freeDiscs(M3U_Disc** discs, int count);

#endif // __M3U_PARSER_H__
