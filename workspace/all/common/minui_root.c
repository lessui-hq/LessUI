/**
 * minui_root.c - Root directory generation utilities
 *
 * Implements root directory building, deduplication, and alias application.
 * Extracted from minui.c for testability.
 */

#include "minui_root.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////
// Alias List Implementation
///////////////////////////////

MinUIAliasList* MinUIAliasList_new(int capacity) {
	MinUIAliasList* list = malloc(sizeof(MinUIAliasList));
	if (!list)
		return NULL;

	list->items = malloc(sizeof(MinUIAlias) * capacity);
	if (!list->items) {
		free(list);
		return NULL;
	}

	list->count = 0;
	list->capacity = capacity;
	return list;
}

void MinUIAliasList_free(MinUIAliasList* list) {
	if (!list)
		return;
	free(list->items);
	free(list);
}

bool MinUIAliasList_add(MinUIAliasList* list, const char* key, const char* value) {
	if (!list || !key || !value)
		return false;

	if (list->count >= list->capacity) {
		int new_capacity = list->capacity * 2;
		MinUIAlias* new_items = realloc(list->items, sizeof(MinUIAlias) * new_capacity);
		if (!new_items)
			return false;
		list->items = new_items;
		list->capacity = new_capacity;
	}

	strncpy(list->items[list->count].key, key, MINUI_ROOT_MAX_NAME - 1);
	list->items[list->count].key[MINUI_ROOT_MAX_NAME - 1] = '\0';
	strncpy(list->items[list->count].value, value, MINUI_ROOT_MAX_NAME - 1);
	list->items[list->count].value[MINUI_ROOT_MAX_NAME - 1] = '\0';
	list->count++;
	return true;
}

const char* MinUIAliasList_get(const MinUIAliasList* list, const char* key) {
	if (!list || !key)
		return NULL;

	for (int i = 0; i < list->count; i++) {
		if (strcmp(list->items[i].key, key) == 0) {
			return list->items[i].value;
		}
	}
	return NULL;
}

///////////////////////////////
// Map Line Parsing
///////////////////////////////

bool MinUIRoot_parseMapLine(const char* line, char* out_key, char* out_value) {
	if (!line || !out_key || !out_value)
		return false;

	out_key[0] = '\0';
	out_value[0] = '\0';

	// Skip empty lines
	if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r')
		return false;

	// Find tab separator
	const char* tab = strchr(line, '\t');
	if (!tab)
		return false;

	// Extract key (before tab)
	size_t key_len = tab - line;
	if (key_len == 0 || key_len >= MINUI_ROOT_MAX_NAME)
		return false;

	strncpy(out_key, line, key_len);
	out_key[key_len] = '\0';

	// Extract value (after tab)
	const char* value_start = tab + 1;
	size_t value_len = strlen(value_start);

	// Trim trailing newlines
	while (value_len > 0 &&
	       (value_start[value_len - 1] == '\n' || value_start[value_len - 1] == '\r')) {
		value_len--;
	}

	if (value_len == 0 || value_len >= MINUI_ROOT_MAX_NAME)
		return false;

	strncpy(out_value, value_start, value_len);
	out_value[value_len] = '\0';

	return true;
}

///////////////////////////////
// Hidden File Detection
///////////////////////////////

bool MinUIRoot_isHidden(const char* filename) {
	if (!filename)
		return true;

	// Dot files are hidden
	if (filename[0] == '.')
		return true;

	// . and .. are always hidden
	if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
		return true;

	return false;
}

///////////////////////////////
// Name Matching
///////////////////////////////

bool MinUIRoot_namesMatch(const char* name1, const char* name2) {
	if (!name1 || !name2)
		return false;

	return strcmp(name1, name2) == 0;
}

void MinUIRoot_getDisplayName(const char* folder_name, char* out_name) {
	if (!out_name)
		return;
	out_name[0] = '\0';

	if (!folder_name)
		return;

	// Check for tag prefix pattern "NNN) "
	const char* name_start = folder_name;
	const char* paren = strchr(folder_name, ')');
	if (paren && paren > folder_name) {
		// Check if prefix is all digits
		bool all_digits = true;
		for (const char* p = folder_name; p < paren; p++) {
			if (!isdigit((unsigned char)*p)) {
				all_digits = false;
				break;
			}
		}
		if (all_digits && paren[1] == ' ') {
			name_start = paren + 2;
		}
	}

	strncpy(out_name, name_start, MINUI_ROOT_MAX_NAME - 1);
	out_name[MINUI_ROOT_MAX_NAME - 1] = '\0';
}

void MinUIRoot_extractFilename(const char* path, char* out_filename) {
	if (!out_filename)
		return;
	out_filename[0] = '\0';

	if (!path)
		return;

	const char* slash = strrchr(path, '/');
	if (slash) {
		strncpy(out_filename, slash + 1, MINUI_ROOT_MAX_NAME - 1);
	} else {
		strncpy(out_filename, path, MINUI_ROOT_MAX_NAME - 1);
	}
	out_filename[MINUI_ROOT_MAX_NAME - 1] = '\0';
}

///////////////////////////////
// Entry Deduplication
///////////////////////////////

int MinUIRoot_deduplicateEntries(MinUIRootEntry* entries, int count) {
	if (!entries || count <= 0)
		return 0;

	int visible_count = 0;
	const char* prev_name = NULL;

	for (int i = 0; i < count; i++) {
		if (prev_name && MinUIRoot_namesMatch(prev_name, entries[i].name)) {
			entries[i].visible = 0;
		} else {
			entries[i].visible = 1;
			visible_count++;
			prev_name = entries[i].name;
		}
	}

	return visible_count;
}

///////////////////////////////
// Alias Application
///////////////////////////////

int MinUIRoot_applyAliases(MinUIRootEntry* entries, int count, const MinUIAliasList* aliases) {
	if (!entries || count <= 0 || !aliases)
		return 0;

	int renamed = 0;

	for (int i = 0; i < count; i++) {
		char filename[MINUI_ROOT_MAX_NAME];
		MinUIRoot_extractFilename(entries[i].path, filename);

		const char* alias = MinUIAliasList_get(aliases, filename);
		if (alias) {
			strncpy(entries[i].name, alias, MINUI_ROOT_MAX_NAME - 1);
			entries[i].name[MINUI_ROOT_MAX_NAME - 1] = '\0';
			renamed++;
		}
	}

	return renamed;
}

///////////////////////////////
// System Directory Validation
///////////////////////////////

bool MinUIRoot_isValidSystemDir(const char* dir_name) {
	if (!dir_name)
		return false;

	// Can't be empty or hidden
	if (dir_name[0] == '\0' || dir_name[0] == '.')
		return false;

	// Must have at least 2 characters
	if (strlen(dir_name) < 2)
		return false;

	return true;
}

///////////////////////////////
// Entry Sorting
///////////////////////////////

static int compare_entries_qsort(const void* a, const void* b) {
	const MinUIRootEntry* ea = (const MinUIRootEntry*)a;
	const MinUIRootEntry* eb = (const MinUIRootEntry*)b;
	return MinUIRoot_compareEntries(ea, eb);
}

int MinUIRoot_compareEntries(const MinUIRootEntry* a, const MinUIRootEntry* b) {
	if (!a || !b)
		return 0;

	// Case-insensitive comparison
	return strcasecmp(a->name, b->name);
}

void MinUIRoot_sortEntries(MinUIRootEntry* entries, int count) {
	if (!entries || count <= 1)
		return;

	qsort(entries, count, sizeof(MinUIRootEntry), compare_entries_qsort);
}

///////////////////////////////
// Entry Counting
///////////////////////////////

int MinUIRoot_countVisible(const MinUIRootEntry* entries, int count) {
	if (!entries)
		return 0;

	int visible = 0;
	for (int i = 0; i < count; i++) {
		if (entries[i].visible) {
			visible++;
		}
	}
	return visible;
}
