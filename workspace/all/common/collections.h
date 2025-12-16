/**
 * collections.h - Generic data structures for Launcher
 *
 * Provides Array (dynamic array) data structure.
 * Extracted from launcher.c for better testability and reusability.
 *
 * For hash maps, use StringMap from stringmap.h (backed by khash).
 */

#ifndef __COLLECTIONS_H__
#define __COLLECTIONS_H__

///////////////////////////////
// Dynamic array
///////////////////////////////

/**
 * Generic dynamic array with automatic growth.
 *
 * Stores pointers to any type. Initial capacity is 8,
 * doubles when full. Used for directories, entries, and recents.
 */
typedef struct Array {
	int count;
	int capacity;
	void** items;
} Array;

/**
 * Creates a new empty array.
 *
 * @return Pointer to allocated Array
 *
 * @warning Caller must free with Array_free() or type-specific free function
 */
Array* Array_new(void);

/**
 * Appends an item to the end of the array.
 *
 * Automatically doubles capacity when full.
 *
 * @param self Array to modify
 * @param item Pointer to add (not copied, caller retains ownership)
 */
void Array_push(Array* self, void* item);

/**
 * Inserts an item at the beginning of the array.
 *
 * Shifts all existing items to the right. Used to add most
 * recent game to top of recents list.
 *
 * @param self Array to modify
 * @param item Pointer to insert
 */
void Array_unshift(Array* self, void* item);

/**
 * Removes and returns the last item from the array.
 *
 * @param self Array to modify
 * @return Pointer to removed item, or NULL if array is empty
 *
 * @note Caller assumes ownership of returned pointer
 */
void* Array_pop(Array* self);

/**
 * Reverses the order of all items in the array.
 *
 * @param self Array to modify
 */
void Array_reverse(Array* self);

/**
 * Frees the array structure.
 *
 * @param self Array to free
 *
 * @warning Does NOT free the items themselves - use type-specific free functions
 */
void Array_free(Array* self);

/**
 * Finds the index of a string in a string array.
 *
 * @param self Array of string pointers
 * @param str String to find
 * @return Index of first matching string, or -1 if not found
 */
int StringArray_indexOf(Array* self, char* str);

/**
 * Frees a string array and all strings it contains.
 *
 * @param self Array to free
 */
void StringArray_free(Array* self);

#endif // __COLLECTIONS_H__
