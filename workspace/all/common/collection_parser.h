/**
 * collection_parser.h - Collection file parser for custom ROM lists
 *
 * MinUI supports ".txt" collection files that list specific ROMs to display.
 * Format: One ROM path per line (relative to SDCARD_PATH)
 *
 * Example collection file (/Collections/Favorites.txt):
 *   /Roms/GB/mario.gb
 *   /Roms/NES/zelda.nes
 *   /Roms/SNES/metroid.smc
 *
 * Extracted from minui.c for testability.
 */

#ifndef __COLLECTION_PARSER_H__
#define __COLLECTION_PARSER_H__

/**
 * ROM entry from collection file
 */
typedef struct Collection_Entry {
	char* path; // Full path to ROM
	int is_pak; // 1 if .pak file, 0 if ROM
} Collection_Entry;

/**
 * Parses a collection file and returns valid ROM entries.
 *
 * Reads the collection .txt file, validates each ROM exists, and
 * creates entries for valid ROMs only.
 *
 * Example:
 *   File: "/Collections/Action.txt" containing "/Roms/GB/mario.gb"
 *   Returns: Array with one entry for mario.gb (if it exists)
 *
 * @param collection_path Full path to collection .txt file
 * @param sdcard_path SDCARD_PATH constant (e.g., "/mnt/SDCARD")
 * @param entry_count Output: number of entries found
 * @return Array of Collection_Entry* (caller must free with Collection_freeEntries)
 *
 * @note Only includes ROMs that exist on filesystem
 * @note Skips empty lines
 * @note Paths in collection file are relative to sdcard_path
 */
Collection_Entry** Collection_parse(char* collection_path, const char* sdcard_path,
                                    int* entry_count);

/**
 * Frees entry array returned by Collection_parse()
 *
 * @param entries Array of entry pointers
 * @param count Number of entries in array
 */
void Collection_freeEntries(Collection_Entry** entries, int count);

#endif // __COLLECTION_PARSER_H__
