#ifndef SHELLUI_UI_LIST_H
#define SHELLUI_UI_LIST_H

#include "common.h"
#include "sdl.h"

// Per-item feature flags
typedef struct {
	char* background_color;    // Hex color for item background
	char* background_image;    // Image path for item background
	char* confirm_text;        // Custom confirm button text for this item
	char* alignment;           // Text alignment: "left", "center", "right"
	bool can_disable;          // Item can be toggled disabled/enabled
	bool disabled;             // Item is disabled (grayed out)
	bool draw_arrows;          // Draw < > arrows around option value
	bool hide_action;          // Hide action button when selected
	bool hide_cancel;          // Hide cancel button when selected
	bool hide_confirm;         // Hide confirm button when selected
	bool is_header;            // Item is a non-selectable header
	bool unselectable;         // Item cannot be selected
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

	// Button configuration
	char* confirm_button;      // Physical button (default: "A")
	char* confirm_text;        // Confirm button label (default: "SELECT")
	char* cancel_button;       // Physical button (default: "B")
	char* cancel_text;         // Cancel button label (default: "BACK")
	char* action_button;       // Action button (optional)
	char* action_text;         // Action button label
	char* enable_button;       // Enable/disable toggle button (default: "Y")

	// Styling
	char* background_color;    // Global background color
	char* background_image;    // Global background image

	// Output
	char* write_location;      // File path or "-" for stdout
	char* write_value;         // "selected", "state", "name", "value"

	// Flags
	bool disable_auto_sleep;
	bool show_hardware_group;
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

#endif // SHELLUI_UI_LIST_H
