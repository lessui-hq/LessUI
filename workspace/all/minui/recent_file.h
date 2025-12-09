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
 * Recent game entry (for file I/O)
 */
typedef struct Recent_Entry {
	char* path; // ROM path (relative to SDCARD_PATH, starts with /)
	char* alias; // Custom display name (NULL if no alias)
} Recent_Entry;

/**
 * Recent game (runtime representation)
 *
 * Paths are stored relative to SDCARD_PATH for platform portability.
 * This allows the same SD card to work across different devices.
 */
typedef struct Recent {
	char* path; // Path relative to SDCARD_PATH (without prefix)
	char* alias; // Optional custom display name
	int available; // 1 if emulator exists, 0 if not
} Recent;

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

///////////////////////////////
// Recent runtime operations
///////////////////////////////

// Forward declaration - hasEmu implementation stays in minui.c (depends on globals)
// Callers provide this function pointer when creating Recent instances
typedef int (*Recent_HasEmuFunc)(char* emu_name);

/**
 * Creates a new recent entry.
 *
 * @param path ROM path relative to SDCARD_PATH (without prefix)
 * @param alias Optional custom display name, or NULL
 * @param sdcard_path SDCARD_PATH constant (e.g., "/mnt/SDCARD")
 * @param has_emu Function to check if emulator exists
 * @return Pointer to allocated Recent
 *
 * @warning Caller must free with Recent_free()
 */
Recent* Recent_new(char* path, char* alias, const char* sdcard_path, Recent_HasEmuFunc has_emu);

/**
 * Frees a recent entry.
 *
 * @param self Recent to free
 */
void Recent_free(Recent* self);

/**
 * Finds a recent by path in a recent array.
 *
 * @param self Array of Recent pointers (as void**)
 * @param count Array size
 * @param str Path to search for (relative to SDCARD_PATH)
 * @return Index of matching recent, or -1 if not found
 */
int RecentArray_indexOf(void** self, int count, char* str);

/**
 * Frees a recent array and all recents it contains.
 *
 * @param self Array of Recent pointers (as void**)
 * @param count Array size
 */
void RecentArray_free(void** self, int count);

#endif // __RECENT_FILE_H__
