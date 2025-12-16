/**
 * launcher_map.h - ROM display name aliasing via map.txt files
 *
 * Launcher uses map.txt files to provide custom display names for ROMs.
 * Format: Tab-delimited key-value pairs
 *   mario.gb<TAB>Super Mario Land
 *   zelda.gb<TAB>Link's Awakening
 *
 * If the alias starts with '.', the ROM is hidden from display.
 *
 * Maps are stb_ds hash maps. Callers use stb_ds macros directly:
 *   MapEntry* map = Map_load(path);
 *   char* val = shget(map, "filename.gb");
 *   Map_free(map);  // Frees values and map
 */

#ifndef LAUNCHER_MAP_H
#define LAUNCHER_MAP_H

#include "stb_ds.h"

/**
 * Map entry type for stb_ds string hash maps.
 * Both key and value are owned strings (must be freed).
 */
typedef struct {
	char* key;
	char* value;
} MapEntry;

/**
 * Loads a map.txt file into a stb_ds hash map.
 *
 * @param map_path Full path to map.txt file
 * @return MapEntry* hash map, or NULL on error. Caller must free with Map_free().
 */
MapEntry* Map_load(const char* map_path);

/**
 * Frees a MapEntry hash map (values and map itself).
 *
 * @param map Hash map to free (safe to call with NULL)
 */
void Map_free(MapEntry* map);

/**
 * Loads merged maps for a ROM directory (pak-bundled + user overrides).
 *
 * @param dir_path Full path to ROM directory
 * @return Merged MapEntry* hash map, or NULL if no maps exist. Caller must free with Map_free().
 */
MapEntry* Map_loadForDirectory(const char* dir_path);

/**
 * Looks up the display alias for a ROM file from map.txt.
 *
 * @param path Full path to ROM file
 * @param alias Output buffer for alias (min 256 bytes)
 * @return Pointer to alias buffer (unchanged if no alias found)
 */
char* Map_getAlias(char* path, char* alias);

#endif // LAUNCHER_MAP_H
