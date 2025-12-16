/**
 * stringmap.c - Fast string-to-string hash map implementation
 *
 * Uses khash for O(1) lookups. All keys and values are copied on insert.
 */

#include "stringmap.h"
#include "../vendor/khash/khash.h"
#include <stdlib.h>
#include <string.h>

// Instantiate khash for string keys -> string values
KHASH_MAP_INIT_STR(strmap, char*)

struct StringMap {
	khash_t(strmap) * h;
};

StringMap* StringMap_new(void) {
	StringMap* map = malloc(sizeof(StringMap));
	if (!map)
		return NULL;
	map->h = kh_init(strmap);
	if (!map->h) {
		free(map);
		return NULL;
	}
	return map;
}

void StringMap_free(StringMap* map) {
	if (!map)
		return;

	// Free all copied keys and values
	khint_t k;
	for (k = kh_begin(map->h); k != kh_end(map->h); ++k) {
		if (kh_exist(map->h, k)) {
			free((char*)kh_key(map->h, k));
			free(kh_value(map->h, k));
		}
	}

	kh_destroy(strmap, map->h);
	free(map);
}

void StringMap_set(StringMap* map, const char* key, const char* value) {
	if (!map || !key || !value)
		return;

	int ret;
	khint_t k = kh_put(strmap, map->h, key, &ret);

	if (ret < 0) {
		// Allocation failed
		return;
	}

	if (ret == 0) {
		// Key already exists - update value
		// Copy new value first, only free old if copy succeeds
		char* new_value = strdup(value);
		if (!new_value)
			return; // Keep old value on OOM
		free(kh_value(map->h, k));
		kh_value(map->h, k) = new_value;
	} else {
		// New key - copy both key and value
		char* key_copy = strdup(key);
		char* value_copy = strdup(value);
		if (!key_copy || !value_copy) {
			// Rollback: remove the slot khash allocated
			free(key_copy);
			free(value_copy);
			kh_del(strmap, map->h, k);
			return;
		}
		kh_key(map->h, k) = key_copy;
		kh_value(map->h, k) = value_copy;
	}
}

char* StringMap_get(StringMap* map, const char* key) {
	if (!map || !key)
		return NULL;

	khint_t k = kh_get(strmap, map->h, key);
	if (k == kh_end(map->h))
		return NULL;

	return kh_value(map->h, k);
}

int StringMap_count(StringMap* map) {
	if (!map)
		return 0;
	return (int)kh_size(map->h);
}
