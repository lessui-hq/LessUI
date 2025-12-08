/**
 * minui_entry.h - Entry type for MinUI file browser
 *
 * Defines the Entry struct used to represent files and folders in the browser,
 * along with IntArray for alphabetical indexing.
 */

#ifndef MINUI_ENTRY_H
#define MINUI_ENTRY_H

#include "collections.h"

/**
 * Type of entry in the file browser.
 */
typedef enum EntryType {
	ENTRY_DIR, // Directory (open to browse contents)
	ENTRY_PAK, // .pak folder (executable tool/app)
	ENTRY_ROM, // ROM file (launch with emulator)
} EntryType;

/**
 * Represents a file or folder in the browser.
 *
 * Entries can be ROMs, directories, or .pak applications.
 * Display names are processed to remove region codes and extensions.
 */
typedef struct Entry {
	char* path; // Full path to file/folder
	char* name; // Cleaned display name (may be aliased via map.txt)
	char* sort_key; // Sorting key (name with leading article skipped)
	char* unique; // Disambiguating text when multiple entries have same name
	int type; // ENTRY_DIR, ENTRY_PAK, or ENTRY_ROM
	int alpha; // Index into parent Directory's alphas array for L1/R1 navigation
} Entry;

/**
 * Fixed-size array of integers for alphabetical indexing.
 *
 * Stores up to 27 indices (one for # and one for each letter A-Z).
 * Each value is the index of the first entry starting with that letter.
 */
#define INT_ARRAY_MAX 27
typedef struct IntArray {
	int count;
	int items[INT_ARRAY_MAX];
} IntArray;

// Entry functions
Entry* Entry_new(const char* path, int type);
void Entry_free(Entry* self);
int Entry_setName(Entry* self, const char* name);

// EntryArray functions (operate on Array of Entry*)
int EntryArray_indexOf(Array* self, const char* path);
void EntryArray_sort(Array* self);
void EntryArray_free(Array* self);

// IntArray functions
IntArray* IntArray_new(void);
void IntArray_push(IntArray* self, int i);
void IntArray_free(IntArray* self);

#endif // MINUI_ENTRY_H
