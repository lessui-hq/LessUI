#ifndef SHUI_UI_LIST_H
#define SHUI_UI_LIST_H

#include "common.h"
#include "sdl.h"

// Per-item feature flags
typedef struct {
	char* confirm_text;        // Custom confirm button text for this item
	bool disabled;             // Item is disabled (grayed out)
	bool is_header;            // Item is a non-selectable header
	bool unselectable;         // Item cannot be selected
	bool hide_confirm;         // Hide confirm button when selected
	bool hide_cancel;          // Hide cancel button when selected
} ListItemFeature;

// List item with optional toggle/options support
typedef struct {
	char* name;                // Display name
	char* value;               // Value to return (defaults to name if NULL)

	// Options/toggle support
	char** options;            // Array of option strings (for toggle items)
	int option_count;          // Number of options
	int selected;              // Currently selected option index
	int initial_selected;      // Initial option index (for reset)

	// Features
	ListItemFeature features;
	bool has_features;
	bool has_options;
} ListItem;

// List display options
typedef struct {
	// Content
	char* title;               // Dialog title (NULL = no title)
	char* title_alignment;     // Title alignment: "left", "center", "right"
	ListItem* items;           // Array of items
	int item_count;            // Number of items
	int initial_index;         // Initially selected index

	// Button labels
	char* confirm_text;        // Confirm button label (default: "SELECT")
	char* cancel_text;         // Cancel button label (default: "BACK")

	// Output
	char* write_location;      // File path or "-" for stdout
	char* write_value;         // "selected", "state", "name", "value"
} ListOptions;

// Result from list selection
typedef struct {
	ExitCode exit_code;
	int selected_index;        // Index of selected item (-1 if cancelled)
	char* selected_value;      // Value of selected item (caller must free)
	char* state_json;          // Full state JSON if write_value="state" (caller must free)
} ListResult;

// Parse list items from JSON string
// Returns allocated array and sets item_count. Caller must free.
ListItem* ui_list_parse_json(const char* json, const char* item_key, int* item_count);

// Parse list items from plain text (one item per line)
ListItem* ui_list_parse_text(const char* text, int* item_count);

// Free list items
void ui_list_free_items(ListItem* items, int count);

// Show a list dialog
ListResult ui_list_show(SDL_Surface* screen, const ListOptions* opts);

#endif // SHUI_UI_LIST_H
