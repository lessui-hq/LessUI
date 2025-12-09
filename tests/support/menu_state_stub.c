/**
 * menu_state_stub.c - Menu state and testable function implementations
 *
 * Provides MinArchMenuState structure and real implementations of
 * testable menu functions (initState, updateState, getAlias).
 *
 * These functions have no SDL dependencies and can be tested directly.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Include just the type definitions
#include "minarch_menu.h"

// Utils functions we depend on (linked from utils.c)
#include "utils.h"

///////////////////////////////
// Menu State (from minarch_menu.c)
///////////////////////////////

static MinArchMenuState menu = {
    .bitmap = NULL,
    .overlay = NULL,
    .items =
        {
            [MENU_ITEM_CONT] = "Continue",
            [MENU_ITEM_SAVE] = "Save",
            [MENU_ITEM_LOAD] = "Load",
            [MENU_ITEM_OPTS] = "Options",
            [MENU_ITEM_QUIT] = "Quit",
        },
    .disc_paths = {NULL},
    .minui_dir = {0},
    .slot_path = {0},
    .base_path = {0},
    .bmp_path = {0},
    .txt_path = {0},
    .disc = -1,
    .total_discs = 0,
    .slot = 0,
    .save_exists = 0,
    .preview_exists = 0,
};

MinArchMenuState* MinArchMenu_getState(void) {
	return &menu;
}

///////////////////////////////
// External dependencies from minarch.c
// Stubbed here for testing
///////////////////////////////

// State path generation stub - generates predictable paths for testing
// The actual implementation is in minarch_state.c but we stub it here
// Tests configure this behavior by setting mock_state_slot
extern struct Game mock_game;
extern int mock_state_slot;
extern char test_dir[256];

void State_getPath(char* filename) {
	sprintf(filename, "%s/%s.st%d", test_dir, mock_game.name, mock_state_slot);
}

///////////////////////////////
// Real implementations of testable menu functions
// (extracted from minarch_menu.c, no SDL dependencies)
///////////////////////////////

static void Menu_initState_ctx(MinArchContext* ctx) {
	MinArchMenuState* m = ctx->menu;
	if (exists(m->slot_path))
		m->slot = getInt(m->slot_path);
	if (m->slot == 8)
		m->slot = 0;

	m->save_exists = 0;
	m->preview_exists = 0;
}

static void Menu_updateState_ctx(MinArchContext* ctx) {
	MinArchMenuState* m = ctx->menu;
	struct Game* g = ctx->game;

	int last_slot = *ctx->state_slot;
	*ctx->state_slot = m->slot;

	char save_path[256];
	State_getPath(save_path);

	*ctx->state_slot = last_slot;

	sprintf(m->bmp_path, "%s/%s.%d.bmp", m->minui_dir, g->name, m->slot);
	sprintf(m->txt_path, "%s/%s.%d.txt", m->minui_dir, g->name, m->slot);

	m->save_exists = exists(save_path);
	m->preview_exists = m->save_exists && exists(m->bmp_path);
}

static void getAlias(char* path, char* alias) {
	char* tmp;
	char map_path[256];
	strcpy(map_path, path);
	tmp = strrchr(map_path, '/');
	if (tmp) {
		tmp += 1;
		strcpy(tmp, "map.txt");
	}
	char* file_name = strrchr(path, '/');
	if (file_name)
		file_name += 1;

	if (exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			char line[256];
			while (fgets(line, 256, file) != NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line) == 0)
					continue;

				tmp = strchr(line, '\t');
				if (!tmp)
					continue;
				tmp[0] = '\0';
				tmp += 1;

				if (exactMatch(line, file_name)) {
					strcpy(alias, tmp);
					fclose(file);
					return;
				}
			}
			fclose(file);
		}
	}
}

///////////////////////////////
// Public API implementations
///////////////////////////////

void MinArchMenu_initState(MinArchContext* ctx) {
	Menu_initState_ctx(ctx);
}

void MinArchMenu_updateState(MinArchContext* ctx) {
	Menu_updateState_ctx(ctx);
}

void MinArchMenu_getAlias(MinArchContext* ctx, char* path, char* alias) {
	(void)ctx;
	getAlias(path, alias);
}

///////////////////////////////
// Stub implementations for untested functions
// (required for linking but not tested yet)
///////////////////////////////

void MinArchMenu_init(MinArchContext* ctx) { (void)ctx; }
void MinArchMenu_quit(MinArchContext* ctx) { (void)ctx; }
void MinArchMenu_loop(MinArchContext* ctx) { (void)ctx; }
void MinArchMenu_beforeSleep(MinArchContext* ctx) { (void)ctx; }
void MinArchMenu_afterSleep(MinArchContext* ctx) { (void)ctx; }
void MinArchMenu_saveState(MinArchContext* ctx) { (void)ctx; }
void MinArchMenu_loadState(MinArchContext* ctx) { (void)ctx; }
void MinArchMenu_scale(MinArchContext* ctx, struct SDL_Surface* src, struct SDL_Surface* dst) {
	(void)ctx;
	(void)src;
	(void)dst;
}
int MinArchMenu_message(MinArchContext* ctx, char* message, char** pairs) {
	(void)ctx;
	(void)message;
	(void)pairs;
	return 0;
}
int MinArchMenu_options(MinArchContext* ctx, MenuList* list) {
	(void)ctx;
	(void)list;
	return 0;
}

///////////////////////////////
// Navigation functions (testable, pure logic)
// These are copied from minarch_menu.c for unit testing
///////////////////////////////

void MinArchMenuNav_init(MinArchMenuNavState* state, int count, int max_visible) {
	state->count = count;
	state->max_visible = max_visible;
	state->selected = 0;
	state->start = 0;
	state->end = (count < max_visible) ? count : max_visible;
	state->visible_rows = state->end;
	state->dirty = 1;
	state->await_input = 0;
	state->should_exit = 0;
}

int MinArchMenuNav_navigate(MinArchMenuNavState* state, int direction) {
	if (state->count <= 0)
		return 0;

	if (direction < 0) {
		// Up
		state->selected -= 1;
		if (state->selected < 0) {
			// Wrap to bottom
			state->selected = state->count - 1;
			state->start = (state->count > state->max_visible) ? state->count - state->max_visible : 0;
			state->end = state->count;
		} else if (state->selected < state->start) {
			// Scroll up
			state->start -= 1;
			state->end -= 1;
		}
	} else if (direction > 0) {
		// Down
		state->selected += 1;
		if (state->selected >= state->count) {
			// Wrap to top
			state->selected = 0;
			state->start = 0;
			state->end = state->visible_rows;
		} else if (state->selected >= state->end) {
			// Scroll down
			state->start += 1;
			state->end += 1;
		}
	} else {
		return 0; // No direction
	}

	return 1;
}

void MinArchMenuNav_advanceItem(MinArchMenuNavState* state) {
	state->selected += 1;
	if (state->selected >= state->count) {
		// Wrap to top
		state->selected = 0;
		state->start = 0;
		state->end = state->visible_rows;
	} else if (state->selected >= state->end) {
		// Scroll down
		state->start += 1;
		state->end += 1;
	}
}

int MinArchMenuNav_cycleValue(MenuItem* item, int direction) {
	if (!item->values)
		return 0;

	if (direction < 0) {
		// Left - decrement with wraparound
		if (item->value > 0) {
			item->value -= 1;
		} else {
			// Count values and wrap to end
			int j;
			for (j = 0; item->values[j]; j++)
				;
			item->value = j - 1;
		}
	} else if (direction > 0) {
		// Right - increment with wraparound
		if (item->values[item->value + 1]) {
			item->value += 1;
		} else {
			item->value = 0;
		}
	} else {
		return 0; // No direction
	}

	return 1;
}

MinArchMenuAction MinArchMenuNav_getAction(MenuList* list, MenuItem* item, int menu_type, int btn_a,
                                           int btn_b, int btn_x, char** button_labels) {
	if (btn_b) {
		return MENU_ACTION_EXIT;
	}

	if (btn_a) {
		if (item->on_confirm) {
			return MENU_ACTION_CONFIRM;
		}
		if (item->submenu) {
			return MENU_ACTION_SUBMENU;
		}
		if (list->on_confirm) {
			// Check if this is a button binding item
			// Must have both button_labels and matching values to be a binding
			if (button_labels && item->values == button_labels) {
				return MENU_ACTION_AWAIT_INPUT;
			}
			return MENU_ACTION_CONFIRM;
		}
	}

	if (btn_x && menu_type == MENU_INPUT) {
		return MENU_ACTION_CLEAR_INPUT;
	}

	return MENU_ACTION_NONE;
}
