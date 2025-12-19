/**
 * player_menu_types.h - Menu system type definitions for Player
 *
 * Defines the core data structures used by the in-game menu system:
 * - MenuItem: Individual menu entries (options, buttons, submenus)
 * - MenuList: Container for groups of menu items
 * - Callback types and result codes
 *
 * Extracted from player.c for maintainability.
 */

#ifndef PLAYER_MENU_TYPES_H
#define PLAYER_MENU_TYPES_H

///////////////////////////////////////
// Forward Declarations
///////////////////////////////////////

typedef struct MenuList MenuList;
typedef struct MenuItem MenuItem;

///////////////////////////////////////
// Callback Types
///////////////////////////////////////

/**
 * Menu callback result codes.
 *
 * Returned by menu callbacks to control menu behavior after an action.
 */
typedef enum {
	MENU_CALLBACK_NOP, // No operation - stay on current item
	MENU_CALLBACK_EXIT, // Exit the current menu
	MENU_CALLBACK_NEXT_ITEM, // Move to next menu item
} MenuCallbackResult;

/**
 * Menu callback function pointer type.
 *
 * Called when a menu item is confirmed (selected) or changed.
 *
 * @param list The menu list containing the item
 * @param i Index of the affected item
 * @return MenuCallbackResult controlling subsequent behavior
 */
typedef int (*MenuList_callback_t)(MenuList* list, int i);

///////////////////////////////////////
// Menu Item Structure
///////////////////////////////////////

/**
 * Individual menu entry.
 *
 * Can represent various UI elements:
 * - Action buttons (on_confirm callback, no values)
 * - Option selectors (values array for cycling through choices)
 * - Submenus (submenu pointer to nested MenuList)
 * - Input bindings (id field for button mapping)
 */
typedef struct MenuItem {
	char* name; // Display name
	char* desc; // Optional description text
	char** values; // NULL-terminated array of option labels (for selectors)
	char* key; // Optional key identifier (used by core options)
	int id; // Optional numeric ID (used by button bindings)
	int value; // Current value index (into values array)
	MenuList* submenu; // Nested menu (if this item opens a submenu)
	MenuList_callback_t on_confirm; // Called when item is selected/confirmed
	MenuList_callback_t on_change; // Called when item value changes
} MenuItem;

///////////////////////////////////////
// Menu List Types
///////////////////////////////////////

/**
 * Menu rendering/behavior modes.
 *
 * Controls how items are displayed and how input is handled.
 */
typedef enum {
	MENU_LIST, // Standard list (save slots, main menu items)
	MENU_VAR, // Variable option list (frontend options - left/right changes value)
	MENU_FIXED, // Fixed option list (emulator options - shows current value, no cycling)
	MENU_INPUT, // Input binding mode (renders like MENU_VAR, special input handling)
} MenuListType;

///////////////////////////////////////
// Menu List Structure
///////////////////////////////////////

/**
 * Container for a group of menu items.
 *
 * Represents a complete menu screen with navigation, callbacks,
 * and optional header description.
 */
typedef struct MenuList {
	int type; // MenuListType for rendering/behavior
	int max_width; // Cached max item width (computed on first draw)
	char* desc; // Optional header description
	MenuItem* items; // NULL-terminated array of menu items
	MenuList_callback_t on_confirm; // Default confirm handler for all items
	MenuList_callback_t on_change; // Default change handler for all items
	int dirty; // Items array was rebuilt, menu must reload count/selection
} MenuList;

///////////////////////////////////////
// Menu Navigation State
///////////////////////////////////////

/**
 * Navigation state for options menu.
 *
 * Tracks selection, pagination, and menu control state.
 * Used by PlayerMenuNav_* functions for testable navigation logic.
 */
typedef struct PlayerMenuNavState {
	int selected; // Currently selected item index
	int start; // First visible item index
	int end; // One past the last visible item index
	int count; // Total item count
	int visible_rows; // Number of visible rows (computed from screen height)
	int max_visible; // Maximum visible items (based on layout)
	int dirty; // Screen needs redraw
	int await_input; // Waiting for button binding input
	int should_exit; // Menu should close
} PlayerMenuNavState;

/**
 * Action requested by input handling.
 *
 * Returned by PlayerMenuNav_handleAction() to indicate what
 * the caller should do after processing input.
 */
typedef enum {
	MENU_ACTION_NONE, // No action needed
	MENU_ACTION_EXIT, // Close the menu
	MENU_ACTION_CONFIRM, // Item confirmed (call on_confirm)
	MENU_ACTION_SUBMENU, // Open submenu (call Menu_options recursively)
	MENU_ACTION_AWAIT_INPUT, // Start button binding mode
	MENU_ACTION_CLEAR_INPUT, // Clear button binding (X pressed)
	MENU_ACTION_VALUE_LEFT, // Value changed left (call on_change)
	MENU_ACTION_VALUE_RIGHT, // Value changed right (call on_change)
} PlayerMenuAction;

#endif /* PLAYER_MENU_TYPES_H */
