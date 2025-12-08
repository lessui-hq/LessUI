/**
 * recent_file.h - Recently played games file I/O
 *
 * Manages recent.txt which stores recently played ROMs.
 * Format: Tab-delimited path and optional alias
 *   /Roms/GB/mario.gb<TAB>Super Mario
 *   /Roms/NES/zelda.nes
 *
 * Provides both read (Recent_parse) and write (Recent_save) operations.
 * Extracted from minui.c for testability.
 */

#ifndef __RECENT_FILE_H__
#define __RECENT_FILE_H__

/**
 * Recent game entry
 */
typedef struct Recent_Entry {
	char* path; // ROM path (relative to SDCARD_PATH, starts with /)
	char* alias; // Custom display name (NULL if no alias)
} Recent_Entry;

/**
 * Parses recent.txt and returns all valid entries.
 *
 * Reads the recent.txt file, validates each ROM exists, and creates
 * entries for valid ROMs only.
 *
 * Format: path<TAB>alias (alias is optional)
 * Example:
 *   /Roms/GB/mario.gb<TAB>Super Mario Land
 *   /Roms/NES/zelda.nes
 *
 * @param recent_path Full path to recent.txt file
 * @param sdcard_path SDCARD_PATH constant (e.g., "/mnt/SDCARD")
 * @param entry_count Output: number of entries found
 * @return Array of Recent_Entry* (caller must free with Recent_freeEntries)
 *
 * @note Only includes ROMs that exist on filesystem
 * @note Skips empty lines
 * @note Paths in recent.txt are relative to sdcard_path
 */
Recent_Entry** Recent_parse(char* recent_path, const char* sdcard_path, int* entry_count);

/**
 * Saves recent entries to recent.txt file
 *
 * Writes entries in tab-delimited format: path<TAB>alias (alias optional)
 *
 * @param recent_path Full path to recent.txt file
 * @param entries Array of entry pointers to save
 * @param count Number of entries in array
 * @return 1 on success, 0 on failure (couldn't open file)
 */
int Recent_save(char* recent_path, Recent_Entry** entries, int count);

/**
 * Frees entry array returned by Recent_parse()
 *
 * @param entries Array of entry pointers
 * @param count Number of entries in array
 */
void Recent_freeEntries(Recent_Entry** entries, int count);

#endif // __RECENT_FILE_H__
