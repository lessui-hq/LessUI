/**
 * collections.h - Generic data structures for MinUI
 *
 * Provides Array (dynamic array) and Hash (key-value map) data structures.
 * Extracted from minui.c for better testability and reusability.
 *
 * Array: Generic dynamic array that stores void pointers
 * Hash: Simple key-value map using parallel string arrays
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

///////////////////////////////
// Simple hash map (key-value store)
///////////////////////////////

/**
 * Simple key-value map using parallel arrays.
 *
 * Used for loading map.txt files that alias ROM display names.
 * Not a true hash - just linear search through keys array.
 */
typedef struct Hash {
	Array* keys;
	Array* values;
} Hash;

/**
 * Creates a new empty hash map.
 *
 * @return Pointer to allocated Hash
 *
 * @warning Caller must free with Hash_free()
 */
Hash* Hash_new(void);

/**
 * Frees a hash map and all its keys and values.
 *
 * @param self Hash to free
 */
void Hash_free(Hash* self);

/**
 * Stores a key-value pair in the hash map.
 *
 * Both key and value are duplicated with strdup().
 * Does not check for duplicate keys - allows multiple entries.
 *
 * @param self Hash to modify
 * @param key Key string
 * @param value Value string
 */
void Hash_set(Hash* self, char* key, char* value);

/**
 * Retrieves a value by key from the hash map.
 *
 * @param self Hash to search
 * @param key Key to look up
 * @return Pointer to value string, or NULL if key not found
 *
 * @note Returned pointer is owned by the Hash - do not free
 */
char* Hash_get(Hash* self, char* key);

#endif // __COLLECTIONS_H__
