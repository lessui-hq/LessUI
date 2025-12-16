/**
 * collections.c - Generic data structures for MinUI
 *
 * Provides Array (dynamic array) and Hash (key-value map) data structures.
 * Extracted from minui.c for better testability and reusability.
 */

#include "collections.h"
#include "log.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

///////////////////////////////
// Dynamic array implementation
///////////////////////////////

/**
 * Creates a new empty array.
 *
 * @return Pointer to allocated Array
 *
 * @warning Caller must free with Array_free() or type-specific free function
 */
Array* Array_new(void) {
	Array* self = malloc(sizeof(Array));
	if (!self)
		return NULL;

	self->count = 0;
	self->capacity = 8;
	self->items = malloc(sizeof(void*) * self->capacity);
	if (!self->items) {
		free(self);
		return NULL;
	}
	return self;
}

/**
 * Appends an item to the end of the array.
 *
 * Automatically doubles capacity when full.
 *
 * @param self Array to modify
 * @param item Pointer to add (not copied, caller retains ownership)
 */
void Array_push(Array* self, void* item) {
	if (self->count >= self->capacity) {
		self->capacity *= 2;
		void** new_items = realloc(self->items, sizeof(void*) * self->capacity);
		if (!new_items) {
			LOG_error("Failed to grow array to capacity %d\n", self->capacity);
			self->capacity /= 2; // restore original capacity
			return;
		}
		self->items = new_items;
	}
	self->items[self->count++] = item;
}

/**
 * Inserts an item at the beginning of the array.
 *
 * Shifts all existing items to the right. Used to add most
 * recent game to top of recents list.
 *
 * @param self Array to modify
 * @param item Pointer to insert
 */
void Array_unshift(Array* self, void* item) {
	if (self->count == 0)
		return Array_push(self, item);
	Array_push(self, NULL); // ensures we have enough capacity
	for (int i = self->count - 2; i >= 0; i--) {
		self->items[i + 1] = self->items[i];
	}
	self->items[0] = item;
}

/**
 * Removes and returns the last item from the array.
 *
 * @param self Array to modify
 * @return Pointer to removed item, or NULL if array is empty
 *
 * @note Caller assumes ownership of returned pointer
 */
void* Array_pop(Array* self) {
	if (self->count == 0)
		return NULL;
	return self->items[--self->count];
}

/**
 * Reverses the order of all items in the array.
 *
 * @param self Array to modify
 */
void Array_reverse(Array* self) {
	int end = self->count - 1;
	int mid = self->count / 2;
	for (int i = 0; i < mid; i++) {
		void* item = self->items[i];
		self->items[i] = self->items[end - i];
		self->items[end - i] = item;
	}
}

/**
 * Frees the array structure.
 *
 * @param self Array to free
 *
 * @warning Does NOT free the items themselves - use type-specific free functions
 */
void Array_free(Array* self) {
	free(self->items);
	free(self);
}

/**
 * Finds the index of a string in a string array.
 *
 * @param self Array of string pointers
 * @param str String to find
 * @return Index of first matching string, or -1 if not found
 */
int StringArray_indexOf(Array* self, char* str) {
	for (int i = 0; i < self->count; i++) {
		if (exactMatch(self->items[i], str))
			return i;
	}
	return -1;
}

/**
 * Frees a string array and all strings it contains.
 *
 * @param self Array to free
 */
void StringArray_free(Array* self) {
	for (int i = 0; i < self->count; i++) {
		free(self->items[i]);
	}
	Array_free(self);
}
