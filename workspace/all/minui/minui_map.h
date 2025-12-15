/**
 * minui_map.h - ROM display name aliasing via map.txt files
 *
 * MinUI uses map.txt files to provide custom display names for ROMs.
 * Format: Tab-delimited key-value pairs
 *   mario.gb<TAB>Super Mario Land
 *   zelda.gb<TAB>Link's Awakening
 *
 * If the alias starts with '.', the ROM is hidden from display.
 */

#ifndef MINUI_MAP_H
#define MINUI_MAP_H

#include "stringmap.h"

/**
 * Loads a map.txt file into a StringMap.
 *
 * Parses the tab-delimited file and returns a StringMap mapping
 * filenames to their display aliases.
 *
 * @param map_path Full path to map.txt file
 * @return StringMap (caller must free with StringMap_free), or NULL on error
 *
 * @note Tab-delimited format: filename<TAB>display name
 * @note Empty lines are skipped
 */
StringMap* Map_load(const char* map_path);

/**
 * Loads merged maps for a ROM directory (pak-bundled + user overrides).
 *
 * Efficiently loads both pak-bundled and user maps for batch aliasing.
 * User entries override pak entries when both exist.
 *
 * @param dir_path Full path to ROM directory (e.g., "/mnt/SDCARD/Roms/MAME")
 * @return Merged StringMap (caller must free), or NULL if no maps exist
 *
 * @note For arcade directories with 50k+ entries, this is much more efficient
 *       than calling Map_getAlias per ROM (loads maps once, not per-ROM)
 */
StringMap* Map_loadForDirectory(const char* dir_path);

/**
 * Looks up the display alias for a ROM file from map.txt.
 *
 * Searches for map.txt in two locations with precedence:
 * 1. ROM directory (user's custom map) - highest priority
 * 2. Pak directory (pak-bundled map) - fallback
 *
 * Maps are merged with user overrides taking precedence. This allows paks
 * to bundle default name mappings (e.g., arcade game names) while users
 * can override any entry with their own map.txt.
 *
 * @param path Full path to ROM file
 * @param alias Output buffer for alias (min 256 bytes)
 * @return Pointer to alias buffer (even if no alias found, returns same buffer)
 *
 * @note If no map.txt exists or ROM not found in map, alias is unchanged
 */
char* Map_getAlias(char* path, char* alias);

#endif // MINUI_MAP_H
