#ifndef SHELLUI_UTILS_H
#define SHELLUI_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <parson/parson.h>

#ifdef PLATFORM
#include "sdl.h"
#endif

///////////////////////////////
// String utilities
///////////////////////////////

/**
 * Duplicates a string, returning NULL if input is NULL.
 * Caller must free the returned pointer.
 */
char* safe_strdup(const char* s);

/**
 * Trims leading and trailing whitespace from a string in-place.
 */
void trimWhitespace(char* s);

/**
 * Converts escape sequences (\n) to actual characters.
 * Writes result to dst buffer of given size.
 */
void unescapeNewlines(char* dst, const char* src, size_t dst_size);

///////////////////////////////
// JSON helpers (parson wrappers)
///////////////////////////////

/**
 * Gets an integer from a JSON object with a default value.
 */
int json_get_int(JSON_Object* obj, const char* name, int def);

/**
 * Gets a boolean from a JSON object with a default value.
 */
bool json_get_bool(JSON_Object* obj, const char* name, bool def);

///////////////////////////////
// Color utilities
///////////////////////////////

#ifdef PLATFORM
/**
 * Parses a hex color string (#RRGGBB) to SDL_Color.
 * Returns black if parsing fails.
 */
SDL_Color hexToColor(const char* hex);
#endif

#endif // SHELLUI_UTILS_H
