#include "ui_list.h"
#include "fonts.h"
#include "api.h"
#include "defines.h"
#include <parson/parson.h>
#include <stdlib.h>
#include <string.h>

// UI constants
#define VISIBLE_ITEMS 8
#define ITEM_HEIGHT_DP 28
#define TITLE_HEIGHT_DP 32

ListItem* ui_list_parse_json(const char* json, const char* item_key, int* item_count) {
	*item_count = 0;
	if (!json) return NULL;

	JSON_Value* root = json_parse_string(json);
	if (!root) return NULL;

	JSON_Object* obj = json_object(root);
	JSON_Array* arr = NULL;

	// Try to get array from item_key, or treat root as array
	if (obj && item_key) {
		arr = json_object_get_array(obj, item_key);
	}
	if (!arr) {
		arr = json_value_get_array(root);
	}
	if (!arr) {
		json_value_free(root);
		return NULL;
	}

	int count = json_array_get_count(arr);
	if (count == 0) {
		json_value_free(root);
		return NULL;
	}

	ListItem* items = calloc(count, sizeof(ListItem));
	if (!items) {
		json_value_free(root);
		return NULL;
	}

	for (int i = 0; i < count; i++) {
		JSON_Value* item_val = json_array_get_value(arr, i);

		// Handle string items
		if (json_value_get_type(item_val) == JSONString) {
			items[i].name = strdup(json_value_get_string(item_val));
			items[i].value = NULL;
			items[i].is_header = false;
			items[i].disabled = false;
		}
		// Handle object items
		else if (json_value_get_type(item_val) == JSONObject) {
			JSON_Object* item_obj = json_value_get_object(item_val);
			const char* name = json_object_get_string(item_obj, "name");
			if (name) {
				items[i].name = strdup(name);
			} else {
				items[i].name = strdup("");
			}

			const char* value = json_object_get_string(item_obj, "value");
			if (value) {
				items[i].value = strdup(value);
			}

			items[i].is_header = json_object_get_boolean(item_obj, "is_header") != 0;
			items[i].disabled = json_object_get_boolean(item_obj, "disabled") != 0;
		}
	}

	json_value_free(root);
	*item_count = count;
	return items;
}

ListItem* ui_list_parse_text(const char* text, int* item_count) {
	*item_count = 0;
	if (!text || !*text) return NULL;

	// Count lines
	int count = 1;
	for (const char* p = text; *p; p++) {
		if (*p == '\n') count++;
	}

	ListItem* items = calloc(count, sizeof(ListItem));
	if (!items) return NULL;

	// Parse lines
	char* copy = strdup(text);
	char* line = strtok(copy, "\n");
	int i = 0;
	while (line && i < count) {
		// Trim leading/trailing whitespace
		while (*line && isspace(*line)) line++;
		char* end = line + strlen(line) - 1;
		while (end > line && isspace(*end)) *end-- = '\0';

		if (*line) {  // Skip empty lines
			items[i].name = strdup(line);
			items[i].value = NULL;
			items[i].is_header = false;
			items[i].disabled = false;
			i++;
		}
		line = strtok(NULL, "\n");
	}
	free(copy);

	*item_count = i;
	return items;
}

void ui_list_free_items(ListItem* items, int count) {
	if (!items) return;
	for (int i = 0; i < count; i++) {
		free(items[i].name);
		free(items[i].value);
	}
	free(items);
}

// Find next selectable item (skipping headers and disabled)
static int find_next_selectable(const ListOptions* opts, int from, int direction) {
	int i = from;
	int attempts = 0;
	while (attempts < opts->item_count) {
		i += direction;
		if (i < 0) i = opts->item_count - 1;
		if (i >= opts->item_count) i = 0;

		if (!opts->items[i].is_header && !opts->items[i].disabled) {
			return i;
		}
		attempts++;
	}
	return from;  // No selectable item found
}

ListResult ui_list_show(SDL_Surface* screen, const ListOptions* opts) {
	ListResult result = {EXIT_ERROR, -1, NULL};
	if (!screen || !opts || opts->item_count == 0) return result;

	if (!g_font_large) return result;

	// Find first selectable item
	int selected = opts->initial_index;
	if (selected < 0 || selected >= opts->item_count) selected = 0;
	if (opts->items[selected].is_header || opts->items[selected].disabled) {
		selected = find_next_selectable(opts, selected, 1);
	}

	// Calculate visible range
	int first_visible = 0;
	int visible_count = VISIBLE_ITEMS;
	if (opts->title) visible_count--;  // Title takes one row

	int redraw = 1;
	int show_setting = 0;

	PWR_disableAutosleep();

	while (1) {
		GFX_startFrame();
		PWR_update(&redraw, &show_setting, NULL, NULL);

		// Input handling
		PAD_poll();

		if (PAD_justPressed(BTN_A)) {
			result.exit_code = EXIT_SUCCESS_CODE;
			result.selected_index = selected;
			if (opts->items[selected].value) {
				result.selected_value = strdup(opts->items[selected].value);
			} else {
				result.selected_value = strdup(opts->items[selected].name);
			}
			return result;
		}
		if (PAD_justPressed(BTN_B)) {
			result.exit_code = EXIT_CANCEL;
			return result;
		}
		if (PAD_justPressed(BTN_MENU)) {
			result.exit_code = EXIT_MENU;
			return result;
		}

		if (PAD_justPressed(BTN_UP) || PAD_justRepeated(BTN_UP)) {
			int new_sel = find_next_selectable(opts, selected, -1);
			if (new_sel != selected) {
				selected = new_sel;
				redraw = 1;
			}
		}
		if (PAD_justPressed(BTN_DOWN) || PAD_justRepeated(BTN_DOWN)) {
			int new_sel = find_next_selectable(opts, selected, 1);
			if (new_sel != selected) {
				selected = new_sel;
				redraw = 1;
			}
		}

		// Adjust scroll to keep selected visible
		if (selected < first_visible) {
			first_visible = selected;
			redraw = 1;
		}
		if (selected >= first_visible + visible_count) {
			first_visible = selected - visible_count + 1;
			redraw = 1;
		}

		if (redraw) {
			GFX_clear(screen);

			int y = DP(8);

			// Title
			if (opts->title && g_font_small) {
				SDL_Surface* title_text = TTF_RenderUTF8_Blended(g_font_small, opts->title, COLOR_WHITE);
				if (title_text) {
					SDL_Rect pos = {DP(16), y, title_text->w, title_text->h};
					SDL_BlitSurface(title_text, NULL, screen, &pos);
					SDL_FreeSurface(title_text);
				}
				y += DP(TITLE_HEIGHT_DP);
			}

			// List items
			int item_height = DP(ITEM_HEIGHT_DP);
			for (int i = first_visible; i < opts->item_count && i < first_visible + visible_count; i++) {
				ListItem* item = &opts->items[i];

				// Highlight selected
				if (i == selected) {
					SDL_Rect pill = {DP(8), y, screen->w - DP(16), item_height};
					GFX_blitPill(ASSET_WHITE_PILL, screen, &pill);
				}

				// Determine text color
				SDL_Color color = COLOR_WHITE;
				if (i == selected) {
					color = COLOR_BLACK;
				} else if (item->disabled) {
					color = COLOR_GRAY;
				} else if (item->is_header) {
					color = COLOR_GRAY;
				}

				// Render item name
				if (item->name && g_font_large) {
					SDL_Surface* text = TTF_RenderUTF8_Blended(g_font_large, item->name, color);
					if (text) {
						int text_x = DP(16);
						int text_y = y + (item_height - text->h) / 2;
						SDL_Rect pos = {text_x, text_y, text->w, text->h};
						SDL_BlitSurface(text, NULL, screen, &pos);
						SDL_FreeSurface(text);
					}
				}

				y += item_height;
			}

			// Scroll indicators
			if (first_visible > 0) {
				// Show "more above" indicator
				GFX_blitText(g_font_small, "...", 0, COLOR_GRAY, screen,
					&(SDL_Rect){screen->w - DP(32), DP(opts->title ? TITLE_HEIGHT_DP + 8 : 8), 0, 0});
			}
			if (first_visible + visible_count < opts->item_count) {
				// Show "more below" indicator
				GFX_blitText(g_font_small, "...", 0, COLOR_GRAY, screen,
					&(SDL_Rect){screen->w - DP(32), screen->h - DP(48), 0, 0});
			}

			// Button hints
			const char* confirm = opts->confirm_text ? opts->confirm_text : "SELECT";
			const char* cancel = opts->cancel_text ? opts->cancel_text : "BACK";
			char* hints[] = {"B", (char*)cancel, "A", (char*)confirm, NULL};
			GFX_blitButtonGroup(hints, 1, screen, 1);

			GFX_flip(screen);
			redraw = 0;
		} else {
			GFX_sync();
		}
	}
}
