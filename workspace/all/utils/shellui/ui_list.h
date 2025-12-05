#ifndef SHELLUI_UI_LIST_H
#define SHELLUI_UI_LIST_H

#include "common.h"
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

// List item
typedef struct {
	char* name;           // Display name
	char* value;          // Value to return (defaults to name if NULL)
	bool is_header;       // Header item (not selectable)
	bool disabled;        // Disabled item (not selectable)
} ListItem;

// List display options
typedef struct {
	char* title;          // Dialog title (NULL = no title)
	char* confirm_text;   // Confirm button label (NULL = "SELECT")
	char* cancel_text;    // Cancel button label (NULL = "BACK")
	ListItem* items;      // Array of items
	int item_count;       // Number of items
	int initial_index;    // Initially selected index
} ListOptions;

// Result from list selection
typedef struct {
	ExitCode exit_code;
	int selected_index;   // Index of selected item (-1 if cancelled)
	char* selected_value; // Value of selected item (caller must free)
} ListResult;

// Initialize list UI resources
void ui_list_init(void);

// Clean up list UI resources
void ui_list_cleanup(void);

// Parse list items from JSON string
// Returns allocated array and sets item_count. Caller must free.
ListItem* ui_list_parse_json(const char* json, const char* item_key, int* item_count);

// Parse list items from plain text (one item per line)
ListItem* ui_list_parse_text(const char* text, int* item_count);

// Free list items
void ui_list_free_items(ListItem* items, int count);

// Show a list dialog
ListResult ui_list_show(SDL_Surface* screen, const ListOptions* opts);

#endif // SHELLUI_UI_LIST_H
