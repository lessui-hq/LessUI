#include "ui_list.h"
#include "shui_utils.h"
#include "api.h"
#include "defines.h"
#include <parson/parson.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Match minarch menu padding
#define OPTION_PADDING 8

// Helper to parse features object from JSON
static void parse_item_features(JSON_Object* features_obj, ListItemFeature* features) {
	if (!features_obj) return;

	const char* str;
	str = json_object_get_string(features_obj, "confirm_text");
	if (str) features->confirm_text = strdup(str);

	// Boolean fields - parson returns 0 for false, 1 for true, -1 for not present
	int val;
	val = json_object_get_boolean(features_obj, "disabled");
	if (val != -1) features->disabled = (val == 1);

	val = json_object_get_boolean(features_obj, "hide_cancel");
	if (val != -1) features->hide_cancel = (val == 1);

	val = json_object_get_boolean(features_obj, "hide_confirm");
	if (val != -1) features->hide_confirm = (val == 1);

	val = json_object_get_boolean(features_obj, "is_header");
	if (val != -1) features->is_header = (val == 1);

	val = json_object_get_boolean(features_obj, "unselectable");
	if (val != -1) features->unselectable = (val == 1);
}

ListItem* ui_list_parse_json(const char* json, const char* item_key, int* item_count) {
	*item_count = 0;
	if (!json) return NULL;

	JSON_Value* root = json_parse_string(json);
	if (!root) return NULL;

	JSON_Object* obj = json_object(root);
	JSON_Array* arr = NULL;

	// Try to get array from item_key, or treat root as array
	if (obj && item_key && strlen(item_key) > 0) {
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
		ListItem* item = &items[i];

		// Initialize defaults
		memset(&item->features, 0, sizeof(ListItemFeature));

		// Handle string items
		if (json_value_get_type(item_val) == JSONString) {
			item->name = strdup(json_value_get_string(item_val));
		}
		// Handle object items
		else if (json_value_get_type(item_val) == JSONObject) {
			JSON_Object* item_obj = json_value_get_object(item_val);

			const char* name = json_object_get_string(item_obj, "name");
			item->name = name ? strdup(name) : strdup("");

			const char* value = json_object_get_string(item_obj, "value");
			if (value) item->value = strdup(value);

			// Parse options array
			JSON_Array* options_arr = json_object_get_array(item_obj, "options");
			if (options_arr) {
				int opt_count = json_array_get_count(options_arr);
				if (opt_count > 0) {
					item->options = calloc(opt_count, sizeof(char*));
					item->option_count = opt_count;
					item->has_options = true;

					for (int j = 0; j < opt_count; j++) {
						const char* opt = json_array_get_string(options_arr, j);
						item->options[j] = opt ? strdup(opt) : strdup("");
					}

					// Parse selected index
					JSON_Value* sel_val = json_object_get_value(item_obj, "selected");
					if (sel_val && json_value_get_type(sel_val) == JSONNumber) {
						item->selected = (int)json_value_get_number(sel_val);
						if (item->selected < 0) item->selected = 0;
						if (item->selected >= opt_count) item->selected = opt_count - 1;
					}
					item->initial_selected = item->selected;
				}
			}

			// Parse features object
			JSON_Object* features_obj = json_object_get_object(item_obj, "features");
			if (features_obj) {
				item->has_features = true;
				parse_item_features(features_obj, &item->features);
			}

			// Also check top-level is_header and disabled for compatibility
			int val = json_object_get_boolean(item_obj, "is_header");
			if (val == 1) item->features.is_header = true;

			val = json_object_get_boolean(item_obj, "disabled");
			if (val == 1) item->features.disabled = true;
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
		while (*line && isspace((unsigned char)*line)) line++;
		char* end = line + strlen(line) - 1;
		while (end > line && isspace((unsigned char)*end)) *end-- = '\0';

		if (*line) {  // Skip empty lines
			items[i].name = strdup(line);
			memset(&items[i].features, 0, sizeof(ListItemFeature));
			i++;
		}
		line = strtok(NULL, "\n");
	}
	free(copy);

	*item_count = i;
	return items;
}

static void free_item_features(ListItemFeature* f) {
	free(f->confirm_text);
}

void ui_list_free_items(ListItem* items, int count) {
	if (!items) return;
	for (int i = 0; i < count; i++) {
		free(items[i].name);
		free(items[i].value);
		if (items[i].options) {
			for (int j = 0; j < items[i].option_count; j++) {
				free(items[i].options[j]);
			}
			free(items[i].options);
		}
		free_item_features(&items[i].features);
	}
	free(items);
}

// Check if item is selectable
static bool is_selectable(ListItem* item) {
	return !item->features.is_header && !item->features.disabled && !item->features.unselectable;
}

// Find next selectable item (skipping headers, disabled, unselectable)
static int find_next_selectable(const ListOptions* opts, int from, int direction) {
	int i = from;
	int attempts = 0;
	while (attempts < opts->item_count) {
		i += direction;
		if (i < 0) i = opts->item_count - 1;
		if (i >= opts->item_count) i = 0;

		if (is_selectable(&opts->items[i])) {
			return i;
		}
		attempts++;
	}
	return from;  // No selectable item found
}

// Generate state JSON for write_value="state"
static char* generate_state_json(const ListOptions* opts, int selected) {
	JSON_Value* root = json_value_init_object();
	JSON_Object* obj = json_object(root);

	json_object_set_number(obj, "selected", selected);

	// Create items array with current state
	JSON_Value* items_val = json_value_init_array();
	JSON_Array* items_arr = json_array(items_val);

	for (int i = 0; i < opts->item_count; i++) {
		ListItem* item = &opts->items[i];
		JSON_Value* item_val = json_value_init_object();
		JSON_Object* item_obj = json_object(item_val);

		json_object_set_string(item_obj, "name", item->name ? item->name : "");

		if (item->has_options) {
			json_object_set_number(item_obj, "selected", item->selected);

			// Include options array
			JSON_Value* opts_val = json_value_init_array();
			JSON_Array* opts_arr = json_array(opts_val);
			for (int j = 0; j < item->option_count; j++) {
				json_array_append_string(opts_arr, item->options[j]);
			}
			json_object_set_value(item_obj, "options", opts_val);
		}

		json_array_append_value(items_arr, item_val);
	}

	json_object_set_value(obj, opts->title ? "settings" : "items", items_val);

	char* result = json_serialize_to_string_pretty(root);
	json_value_free(root);
	return result;
}

ListResult ui_list_show(SDL_Surface* screen, const ListOptions* opts) {
	ListResult result = {EXIT_ERROR, -1, NULL, NULL};
	if (!screen || !opts || opts->item_count == 0) return result;
	if (!font.medium) return result;

	// Find first selectable item
	int selected = opts->initial_index;
	if (selected < 0 || selected >= opts->item_count) selected = 0;
	if (!is_selectable(&opts->items[selected])) {
		selected = find_next_selectable(opts, selected, 1);
	}

	// Track if any items have options (for showing left/right hints)
	bool has_options = false;
	for (int i = 0; i < opts->item_count; i++) {
		if (opts->items[i].has_options) {
			has_options = true;
			break;
		}
	}
	(void)has_options;  // TODO: use for left/right button hints

	// Calculate visible range using dynamic UI layout
	int first_visible = 0;
	int visible_count = ui.row_count;
	if (opts->title) visible_count--;  // Title takes one row

	int redraw = 1;
	int show_setting = 0;

	// Drain any stale input events from before the list appeared
	PAD_poll();
	PAD_reset();

	while (1) {
		GFX_startFrame();
		PWR_update(&redraw, &show_setting, NULL, NULL);

		// Input handling
		PAD_poll();
		ListItem* sel_item = &opts->items[selected];

		// Confirm (A button)
		if (PAD_justPressed(BTN_A)) {
			result.exit_code = EXIT_SUCCESS_CODE;
			result.selected_index = selected;

			// Return value based on item type
			if (sel_item->has_options && sel_item->selected < sel_item->option_count) {
				result.selected_value = strdup(sel_item->options[sel_item->selected]);
			} else if (sel_item->value) {
				result.selected_value = strdup(sel_item->value);
			} else {
				result.selected_value = strdup(sel_item->name);
			}

			// Generate state JSON
			result.state_json = generate_state_json(opts, selected);
			return result;
		}

		// Cancel (B button)
		if (PAD_justPressed(BTN_B)) {
			result.exit_code = EXIT_CANCEL;
			result.selected_index = -1;
			return result;
		}

		// Menu
		if (PAD_justPressed(BTN_MENU)) {
			result.exit_code = EXIT_MENU;
			return result;
		}

		// Up/Down navigation
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

		// Left/Right for option cycling
		if (sel_item->has_options && sel_item->option_count > 1) {
			if (PAD_justPressed(BTN_LEFT) || PAD_justRepeated(BTN_LEFT)) {
				sel_item->selected--;
				if (sel_item->selected < 0) sel_item->selected = sel_item->option_count - 1;
				redraw = 1;
			}
			if (PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_RIGHT)) {
				sel_item->selected++;
				if (sel_item->selected >= sel_item->option_count) sel_item->selected = 0;
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

			int y = ui.edge_padding_px;

			// Title (using font.medium like minarch)
			if (opts->title) {
				SDL_Surface* title_text = TTF_RenderUTF8_Blended(font.medium, opts->title, COLOR_GRAY);
				if (title_text) {
					int title_x = ui.edge_padding_px;
					// Handle title alignment
					if (opts->title_alignment) {
						if (strcmp(opts->title_alignment, "center") == 0) {
							title_x = (screen->w - title_text->w) / 2;
						} else if (strcmp(opts->title_alignment, "right") == 0) {
							title_x = screen->w - title_text->w - ui.edge_padding_px;
						}
					}
					SDL_Rect pos = {title_x, y + ui.option_offset_px, title_text->w, title_text->h};
					SDL_BlitSurface(title_text, NULL, screen, &pos);
					SDL_FreeSurface(title_text);
				}
				y += ui.pill_height_px;
			}

			// List items (matching minarch menu style)
			int item_height = ui.option_size_px;
			int row_width = screen->w - (ui.edge_padding_px * 2);

			for (int i = first_visible; i < opts->item_count && i < first_visible + visible_count; i++) {
				ListItem* item = &opts->items[i];
				int ox = ui.edge_padding_px;

				// Determine text colors
				SDL_Color label_color = COLOR_WHITE;
				SDL_Color value_color = COLOR_WHITE;

				if (item->features.disabled || item->features.unselectable) {
					label_color = COLOR_GRAY;
					value_color = COLOR_GRAY;
				} else if (item->features.is_header) {
					label_color = COLOR_GRAY;
				}

				// Calculate label width for pill sizing
				int label_w = row_width;
				if (item->has_options && item->option_count > 0) {
					int lw = 0;
					TTF_SizeUTF8(font.medium, item->name, &lw, NULL);
					label_w = lw + DP(OPTION_PADDING * 2);
				}

				// Draw pills for selected item
				if (i == selected && is_selectable(item)) {
					if (item->has_options && item->option_count > 0) {
						// Gray pill for full row (shows value on gray background)
						GFX_blitPill(ASSET_OPTION, screen,
						             &(SDL_Rect){ox, y, row_width, item_height});
					}
					// White pill for label
					GFX_blitPill(ASSET_OPTION_WHITE, screen,
					             &(SDL_Rect){ox, y, label_w, item_height});
					label_color = COLOR_BLACK;
				}

				// Render item name using font.medium (like minarch)
				if (item->name) {
					SDL_Surface* text = TTF_RenderUTF8_Blended(font.medium, item->name, label_color);
					if (text) {
						int text_x = ox + DP(OPTION_PADDING);
						int text_y = y + ui.option_offset_px;
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){text_x, text_y, text->w, text->h});
						SDL_FreeSurface(text);
					}
				}

				// Render option value on the right side using font.small (like minarch)
				if (item->has_options && item->option_count > 0) {
					const char* opt_str = item->options[item->selected];
					if (opt_str) {
						SDL_Surface* opt_text = TTF_RenderUTF8_Blended(font.small, opt_str, value_color);
						if (opt_text) {
							int opt_x = ox + row_width - opt_text->w - DP(OPTION_PADDING);
							int opt_y = y + ui.option_value_offset_px;
							SDL_BlitSurface(opt_text, NULL, screen, &(SDL_Rect){opt_x, opt_y, opt_text->w, opt_text->h});
							SDL_FreeSurface(opt_text);
						}
					}
				}

				y += item_height;
			}

			// Scroll indicators
			if (first_visible > 0) {
				int scroll_y = DP(opts->title ? ui.edge_padding + ui.pill_height : ui.edge_padding);
				GFX_blitText(font.small, "...", 0, COLOR_GRAY, screen,
					&(SDL_Rect){screen->w - DP(ui.edge_padding * 2), scroll_y, 0, 0});
			}
			if (first_visible + visible_count < opts->item_count) {
				GFX_blitText(font.small, "...", 0, COLOR_GRAY, screen,
					&(SDL_Rect){screen->w - DP(ui.edge_padding * 2), screen->h - DP(ui.pill_height + ui.edge_padding), 0, 0});
			}

			// Button hints (uppercase for UI consistency)
			char confirm_upper[64] = "SELECT";
			char cancel_upper[64] = "BACK";
			if (opts->confirm_text) {
				strncpy(confirm_upper, opts->confirm_text, sizeof(confirm_upper) - 1);
				toUppercase(confirm_upper);
			}
			if (opts->cancel_text) {
				strncpy(cancel_upper, opts->cancel_text, sizeof(cancel_upper) - 1);
				toUppercase(cancel_upper);
			}

			// Determine which hints to show
			if (!sel_item->features.hide_confirm && !sel_item->features.hide_cancel) {
				char* hints[] = {"B", cancel_upper, "A", confirm_upper, NULL};
				GFX_blitButtonGroup(hints, 1, screen, 1);
			} else if (!sel_item->features.hide_confirm) {
				char* hints[] = {"A", confirm_upper, NULL};
				GFX_blitButtonGroup(hints, 0, screen, 1);
			} else if (!sel_item->features.hide_cancel) {
				char* hints[] = {"B", cancel_upper, NULL};
				GFX_blitButtonGroup(hints, 0, screen, 1);
			}

			GFX_flip(screen);
			redraw = 0;
		} else {
			GFX_sync();
		}
	}
}
