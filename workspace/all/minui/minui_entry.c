/**
 * minui_entry.c - Entry type for MinUI file browser
 *
 * Implements Entry operations for the file browser.
 */

#include "minui_entry.h"
#include "minui_str_compare.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

///////////////////////////////
// Entry functions
///////////////////////////////

/**
 * Sets an entry's display name and computes its sort key.
 *
 * The sort key is the name with any leading article ("The ", "A ", "An ")
 * stripped, ensuring sorting and alphabetical indexing are consistent.
 *
 * @param self Entry to update
 * @param name New display name (will be copied)
 * @return 1 on success, 0 on allocation failure
 */
int Entry_setName(Entry* self, const char* name) {
	char* new_name = strdup(name);
	if (!new_name)
		return 0;

	// Compute sort key by skipping leading article
	const char* key_start = skip_article(name);
	char* new_sort_key = strdup(key_start);
	if (!new_sort_key) {
		free(new_name);
		return 0;
	}

	// Free old values and assign new ones
	if (self->name)
		free(self->name);
	if (self->sort_key)
		free(self->sort_key);

	self->name = new_name;
	self->sort_key = new_sort_key;
	return 1;
}

/**
 * Creates a new entry from a path.
 *
 * Automatically processes the display name to remove extensions,
 * region codes, and other metadata.
 *
 * @param path Full path to the file/folder
 * @param type ENTRY_DIR, ENTRY_PAK, or ENTRY_ROM
 * @return Pointer to allocated Entry, or NULL on failure
 *
 * @warning Caller must free with Entry_free()
 */
Entry* Entry_new(const char* path, int type) {
	char display_name[256];
	getDisplayName(path, display_name);

	Entry* self = malloc(sizeof(Entry));
	if (!self)
		return NULL;

	self->path = strdup(path);
	if (!self->path) {
		free(self);
		return NULL;
	}

	// Initialize to NULL before Entry_setName
	self->name = NULL;
	self->sort_key = NULL;
	if (!Entry_setName(self, display_name)) {
		free(self->path);
		free(self);
		return NULL;
	}

	self->unique = NULL;
	self->type = type;
	self->alpha = 0;
	return self;
}

/**
 * Frees an entry and all its strings.
 *
 * @param self Entry to free
 */
void Entry_free(Entry* self) {
	if (!self)
		return;
	free(self->path);
	free(self->name);
	free(self->sort_key);
	if (self->unique)
		free(self->unique);
	free(self);
}

///////////////////////////////
// EntryArray functions
///////////////////////////////

/**
 * Finds an entry by path in an entry array.
 *
 * @param self Array of Entry pointers
 * @param path Path to search for
 * @return Index of matching entry, or -1 if not found
 */
int EntryArray_indexOf(Array* self, const char* path) {
	for (int i = 0; i < self->count; i++) {
		Entry* entry = self->items[i];
		if (exactMatch(entry->path, path))
			return i;
	}
	return -1;
}

/**
 * Comparison function for qsort - sorts entries using natural sort.
 *
 * Uses sort_key for comparison, which has leading articles stripped.
 * Natural sort orders numeric sequences by value, not lexicographically.
 * Example: "Game 2" < "Game 10" (unlike strcmp where "Game 10" < "Game 2")
 */
static int EntryArray_sortEntry(const void* a, const void* b) {
	Entry* item1 = *(Entry**)a;
	Entry* item2 = *(Entry**)b;
	return strnatcasecmp(item1->sort_key, item2->sort_key);
}

/**
 * Sorts an entry array alphabetically by display name.
 *
 * @param self Array to sort (modified in place)
 */
void EntryArray_sort(Array* self) {
	qsort(self->items, self->count, sizeof(void*), EntryArray_sortEntry);
}

/**
 * Frees an entry array and all entries it contains.
 *
 * @param self Array to free
 */
void EntryArray_free(Array* self) {
	if (!self)
		return;
	for (int i = 0; i < self->count; i++) {
		Entry_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////
// IntArray functions
///////////////////////////////

/**
 * Creates a new empty integer array.
 *
 * @return Pointer to allocated IntArray, or NULL on failure
 *
 * @warning Caller must free with IntArray_free()
 */
IntArray* IntArray_new(void) {
	IntArray* self = malloc(sizeof(IntArray));
	if (!self)
		return NULL;
	self->count = 0;
	memset(self->items, 0, sizeof(int) * INT_ARRAY_MAX);
	return self;
}

/**
 * Appends an integer to the array.
 * Silently drops if array is full.
 *
 * @param self Array to modify
 * @param i Value to append
 */
void IntArray_push(IntArray* self, int i) {
	if (self->count >= INT_ARRAY_MAX)
		return;
	self->items[self->count++] = i;
}

/**
 * Frees an integer array.
 *
 * @param self Array to free
 */
void IntArray_free(IntArray* self) {
	free(self);
}
