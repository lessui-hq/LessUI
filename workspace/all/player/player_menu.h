/**
 * player_menu.h - In-game menu system for Player
 *
 * The menu system handles:
 * - In-game pause menu (Continue, Save, Load, Options, Quit)
 * - Save state management with slot selection and previews
 * - Options submenus (Frontend, Emulator, Controls, Shortcuts)
 * - Multi-disc selection for games with multiple discs
 * - Power management (sleep/wake, auto-save before sleep)
 *
 * The menu is displayed over a screenshot of the game and provides
 * access to all runtime configuration options.
 */

#ifndef PLAYER_MENU_H
#define PLAYER_MENU_H

#include "player_context.h"
#include "player_menu_types.h"

// Forward declaration for SDL_Surface (avoid SDL header conflicts)
// The actual SDL headers are included by player.c via sdl.h
struct SDL_Surface;

///////////////////////////////
// Menu constants
///////////////////////////////

#define MENU_ITEM_COUNT 5
#define MENU_SLOT_COUNT 8
#define MENU_MAX_DISCS 9

// Menu item indices
enum {
	MENU_ITEM_CONT,
	MENU_ITEM_SAVE,
	MENU_ITEM_LOAD,
	MENU_ITEM_OPTS,
	MENU_ITEM_QUIT,
};

///////////////////////////////
// Menu state structure
///////////////////////////////

/**
 * Menu runtime state - tracks in-game menu data.
 *
 * This struct is initialized by Menu_init() and holds all state
 * needed for the in-game pause menu, save state previews, and
 * multi-disc management.
 */
typedef struct PlayerMenuState {
	struct SDL_Surface* bitmap; // Game screenshot for menu background
	struct SDL_Surface* overlay; // Semi-transparent overlay

	char* items[MENU_ITEM_COUNT]; // Menu item labels
	char* disc_paths[MENU_MAX_DISCS]; // Multi-disc paths (up to 9)

	char launcher_dir[256]; // Launcher data directory for this game
	char slot_path[256]; // Path to slot tracking file
	char base_path[256]; // Base ROM directory path
	char bmp_path[256]; // Current preview image path
	char txt_path[256]; // Current save metadata path

	int disc; // Current disc index (-1 if single disc)
	int total_discs; // Total disc count (0 if single disc)
	int slot; // Current save slot (0-8)
	int save_exists; // Current slot has save data
	int preview_exists; // Current slot has preview image
} PlayerMenuState;

/**
 * Get the global menu state instance.
 * Used for context initialization in player.c.
 */
PlayerMenuState* PlayerMenu_getState(void);

///////////////////////////////
// Menu lifecycle
///////////////////////////////

/**
 * Initialize the menu system.
 *
 * Sets up the menu overlay surface, parses multi-disc info from m3u,
 * and prepares menu state paths.
 *
 * @param ctx Player context
 */
void PlayerMenu_init(PlayerContext* ctx);

/**
 * Cleanup menu resources.
 *
 * Frees the overlay surface and any allocated disc paths.
 *
 * @param ctx Player context
 */
void PlayerMenu_quit(PlayerContext* ctx);

///////////////////////////////
// Menu display
///////////////////////////////

/**
 * Main menu loop - displays in-game menu and handles input.
 *
 * This function blocks until the user exits the menu. It:
 * 1. Captures game screenshot as background
 * 2. Saves SRAM/RTC (in case of crash)
 * 3. Reduces CPU speed and enables sleep
 * 4. Displays menu over screenshot
 * 5. Handles navigation and option changes
 * 6. Restores game state and resumes
 *
 * @param ctx Player context
 */
void PlayerMenu_loop(PlayerContext* ctx);

///////////////////////////////
// Sleep/wake handlers
///////////////////////////////

/**
 * Called before device enters sleep mode.
 *
 * Saves SRAM, RTC, and creates auto-save state.
 * Records current game for auto-resume on next boot.
 *
 * @param ctx Player context
 */
void PlayerMenu_beforeSleep(PlayerContext* ctx);

/**
 * Called after device wakes from sleep.
 *
 * Clears auto-resume flag and restores CPU speed.
 *
 * @param ctx Player context
 */
void PlayerMenu_afterSleep(PlayerContext* ctx);

///////////////////////////////
// State management
///////////////////////////////

/**
 * Initialize state slot tracking for the current game.
 *
 * Determines which save slots have data and previews.
 *
 * @param ctx Player context
 */
void PlayerMenu_initState(PlayerContext* ctx);

/**
 * Update state slot info after slot change.
 *
 * Checks if current slot has save data and preview image.
 *
 * @param ctx Player context
 */
void PlayerMenu_updateState(PlayerContext* ctx);

/**
 * Save current state to selected slot.
 *
 * Creates state file and screenshot preview.
 *
 * @param ctx Player context
 */
void PlayerMenu_saveState(PlayerContext* ctx);

/**
 * Load state from selected slot.
 *
 * Restores game state from save file.
 *
 * @param ctx Player context
 */
void PlayerMenu_loadState(PlayerContext* ctx);

///////////////////////////////
// Dialog helpers
///////////////////////////////

/**
 * Display a message dialog with button options.
 *
 * Shows message text and waits for user to select an option.
 *
 * @param ctx Player context
 * @param message Message text to display
 * @param pairs NULL-terminated array of button label/value pairs
 * @return Selected option index, or -1 if cancelled
 */
int PlayerMenu_message(PlayerContext* ctx, char* message, char** pairs);

/**
 * Display an options menu.
 *
 * Shows a list of configurable options with current values.
 * Handles navigation and value changes.
 *
 * @param ctx Player context
 * @param list Menu list to display
 * @return Status code (0 = back, other = specific action)
 */
int PlayerMenu_options(PlayerContext* ctx, MenuList* list);

///////////////////////////////
// Internal helpers (for testing)
///////////////////////////////

/**
 * Scale a surface to fit the menu preview area.
 *
 * Used for scaling game screenshots and save state previews.
 *
 * @param ctx Player context
 * @param src Source surface
 * @param dst Destination surface (must be allocated)
 */
void PlayerMenu_scale(PlayerContext* ctx, struct SDL_Surface* src, struct SDL_Surface* dst);

/**
 * Get alias name for a ROM from its path.
 *
 * Checks map.txt files for display name overrides.
 *
 * @param ctx Player context
 * @param path ROM path
 * @param alias Output buffer for alias (or original name if not found)
 */
void PlayerMenu_getAlias(PlayerContext* ctx, char* path, char* alias);

///////////////////////////////
// Menu Navigation (testable)
///////////////////////////////

/**
 * Initialize navigation state for an options menu.
 *
 * Sets up selection, pagination, and visibility based on item count
 * and screen constraints.
 *
 * @param state Navigation state to initialize
 * @param count Total number of menu items
 * @param max_visible Maximum visible items (from screen height calculation)
 */
void PlayerMenuNav_init(PlayerMenuNavState* state, int count, int max_visible);

/**
 * Handle up/down navigation input.
 *
 * Updates selected item and pagination with wraparound.
 * Pure function - no side effects.
 *
 * @param state Navigation state to update
 * @param direction -1 for up, +1 for down
 * @return 1 if state changed (dirty), 0 if no change
 */
int PlayerMenuNav_navigate(PlayerMenuNavState* state, int direction);

/**
 * Advance to next item (after binding or callback).
 *
 * Moves selection down with pagination and wraparound.
 * Used after MENU_CALLBACK_NEXT_ITEM or button binding completion.
 *
 * @param state Navigation state to update
 */
void PlayerMenuNav_advanceItem(PlayerMenuNavState* state);

/**
 * Cycle a menu item's value left or right.
 *
 * Updates item->value with wraparound within the values array.
 * Pure function - does not call callbacks.
 *
 * @param item Menu item to update
 * @param direction -1 for left (decrement), +1 for right (increment)
 * @return 1 if value changed, 0 if no change (no values array)
 */
int PlayerMenuNav_cycleValue(MenuItem* item, int direction);

/**
 * Determine action from button press.
 *
 * Examines the current item and menu type to determine what action
 * should be taken for A, B, or X button presses.
 * Pure function - returns action code for caller to execute.
 *
 * @param list Menu list being displayed
 * @param item Currently selected item
 * @param menu_type Menu type (MENU_LIST, MENU_VAR, etc.)
 * @param btn_a 1 if A pressed
 * @param btn_b 1 if B pressed
 * @param btn_x 1 if X pressed
 * @param btn_labels Pointer to btn_labels array (for input detection)
 * @return Action to perform (MENU_ACTION_*)
 */
PlayerMenuAction PlayerMenuNav_getAction(MenuList* list, MenuItem* item, int menu_type, int btn_a,
                                         int btn_b, int btn_x, char** btn_labels);

#endif /* PLAYER_MENU_H */
