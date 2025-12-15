/**
 * stringmap.h - Fast string-to-string hash map
 *
 * A simple wrapper around khash providing O(1) string key lookups.
 * Used for ROM alias maps, config lookups, etc.
 *
 * Example:
 *   StringMap* map = StringMap_new();
 *   StringMap_set(map, "mslug.zip", "Metal Slug");
 *   char* name = StringMap_get(map, "mslug.zip");  // "Metal Slug"
 *   StringMap_free(map);
 */

#ifndef STRINGMAP_H
#define STRINGMAP_H

/**
 * Opaque string map handle.
 */
typedef struct StringMap StringMap;

/**
 * Creates a new empty string map.
 *
 * @return Pointer to allocated StringMap, or NULL on failure
 * @warning Caller must free with StringMap_free()
 */
StringMap* StringMap_new(void);

/**
 * Frees a string map and all its contents.
 *
 * @param map Map to free (safe to pass NULL)
 */
void StringMap_free(StringMap* map);

/**
 * Stores a key-value pair in the map.
 *
 * Both key and value are copied. If key already exists, value is updated.
 *
 * @param map Map to modify
 * @param key Key string (must not be NULL)
 * @param value Value string (must not be NULL)
 */
void StringMap_set(StringMap* map, const char* key, const char* value);

/**
 * Retrieves a value by key.
 *
 * @param map Map to search
 * @param key Key to look up
 * @return Pointer to value string, or NULL if not found
 *
 * @note Returned pointer is owned by the map - do not free
 */
char* StringMap_get(StringMap* map, const char* key);

/**
 * Returns the number of entries in the map.
 *
 * @param map Map to query
 * @return Number of key-value pairs
 */
int StringMap_count(StringMap* map);

#endif /* STRINGMAP_H */
