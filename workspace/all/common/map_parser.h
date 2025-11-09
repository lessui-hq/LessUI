/**
 * map_parser.h - ROM display name aliasing via map.txt files
 *
 * MinUI uses map.txt files to provide custom display names for ROMs.
 * Format: Tab-delimited key-value pairs
 *   mario.gb<TAB>Super Mario Land
 *   zelda.gb<TAB>Link's Awakening
 *
 * If the alias starts with '.', the ROM is hidden from display.
 *
 * Extracted from minarch.c and minui.c for testability.
 */

#ifndef __MAP_PARSER_H__
#define __MAP_PARSER_H__

/**
 * Looks up the display alias for a ROM file from map.txt.
 *
 * Searches for map.txt in the same directory as the ROM file.
 * If found, looks up the ROM's filename in the map and returns the alias.
 *
 * Example:
 *   ROM: /Roms/GB/mario.gb
 *   map.txt: "mario.gb\tSuper Mario Land"
 *   Returns: "Super Mario Land" (written to alias buffer)
 *
 * @param path Full path to ROM file
 * @param alias Output buffer for alias (min 256 bytes)
 * @return Pointer to alias buffer (even if no alias found, returns same buffer)
 *
 * @note If no map.txt exists or ROM not found in map, alias is unchanged
 * @note Tab-delimited format: filename<TAB>display name
 */
char* Map_getAlias(char* path, char* alias);

#endif // __MAP_PARSER_H__
