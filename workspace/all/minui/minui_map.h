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

#include "collections.h"

/**
 * Loads a map.txt file into a Hash table.
 *
 * Parses the tab-delimited file and returns a Hash mapping
 * filenames to their display aliases.
 *
 * @param map_path Full path to map.txt file
 * @return Hash table (caller must free with Hash_free), or NULL on error
 *
 * @note Tab-delimited format: filename<TAB>display name
 * @note Empty lines are skipped
 */
Hash* Map_load(const char* map_path);

/**
 * Looks up the display alias for a ROM file from map.txt.
 *
 * Searches for map.txt in the same directory as the ROM file.
 * If found, looks up the ROM's filename in the map and returns the alias.
 *
 * @param path Full path to ROM file
 * @param alias Output buffer for alias (min 256 bytes)
 * @return Pointer to alias buffer (even if no alias found, returns same buffer)
 *
 * @note If no map.txt exists or ROM not found in map, alias is unchanged
 */
char* Map_getAlias(char* path, char* alias);

#endif // MINUI_MAP_H
