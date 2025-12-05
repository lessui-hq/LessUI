#include "ui_keyboard.h"
#include "fonts.h"
#include "api.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>

// Keyboard layouts (5 rows, up to 14 columns)
static const char* layout_lower[5][14] = {
	{"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", NULL},
	{"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\", NULL},
	{"a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", NULL},
	{"z", "x", "c", "v", "b", "n", "m", ",", ".", "/", NULL},
	{"ABC", " ", "OK", NULL}
};

static const char* layout_upper[5][14] = {
	{"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", NULL},
	{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "|", NULL},
	{"A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", NULL},
	{"Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", NULL},
	{"abc", " ", "OK", NULL}
};

// Current layout pointer
static const char* (*current_layout)[14] = layout_lower;

// Count valid keys in row
static int row_length(int row) {
	int len = 0;
	for (int i = 0; i < 14 && current_layout[row][i]; i++) {
		len++;
	}
	return len;
}

KeyboardResult ui_keyboard_show(SDL_Surface* screen, const KeyboardOptions* opts) {
	KeyboardResult result = {EXIT_ERROR, NULL};
	if (!screen || !g_font_large) return result;

	// Initialize text buffer
	char text[1024] = "";
	if (opts && opts->initial_value) {
		strncpy(text, opts->initial_value, sizeof(text) - 1);
	}

	// Cursor position
	int cursor_row = 1;
	int cursor_col = 0;
	current_layout = layout_lower;

	int redraw = 1;
	int show_setting = 0;

	PWR_disableAutosleep();

	// Key dimensions
	int key_w = DP(24);
	int key_h = DP(28);
	int key_spacing = DP(4);
	int kb_start_y = screen->h - (5 * key_h + 4 * key_spacing) - DP(32);

	while (1) {
		GFX_startFrame();
		PWR_update(&redraw, &show_setting, NULL, NULL);

		// Input handling
		PAD_poll();

		// Confirm (A button)
		if (PAD_justPressed(BTN_A)) {
			const char* key = current_layout[cursor_row][cursor_col];
			if (key) {
				// Special keys
				if (strcmp(key, "OK") == 0) {
					result.exit_code = EXIT_SUCCESS_CODE;
					result.text = strdup(text);
					return result;
				} else if (strcmp(key, "ABC") == 0 || strcmp(key, "abc") == 0) {
					// Toggle case
					current_layout = (current_layout == layout_lower) ? layout_upper : layout_lower;
					redraw = 1;
				} else if (strcmp(key, " ") == 0) {
					// Space
					if (strlen(text) < sizeof(text) - 2) {
						strcat(text, " ");
						redraw = 1;
					}
				} else {
					// Regular key
					if (strlen(text) < sizeof(text) - 2) {
						strcat(text, key);
						redraw = 1;
					}
				}
			}
		}

		// Backspace (B button)
		if (PAD_justPressed(BTN_B)) {
			int len = strlen(text);
			if (len > 0) {
				text[len - 1] = '\0';
				redraw = 1;
			}
		}

		// Cancel (Y button)
		if (PAD_justPressed(BTN_Y)) {
			result.exit_code = EXIT_CANCEL;
			result.text = opts && opts->initial_value ? strdup(opts->initial_value) : strdup("");
			return result;
		}

		// Menu
		if (PAD_justPressed(BTN_MENU)) {
			result.exit_code = EXIT_MENU;
			return result;
		}

		// Navigation
		if (PAD_justPressed(BTN_UP) || PAD_justRepeated(BTN_UP)) {
			if (cursor_row > 0) {
				cursor_row--;
				// Adjust column for new row length
				int new_len = row_length(cursor_row);
				if (cursor_col >= new_len) cursor_col = new_len - 1;
				redraw = 1;
			}
		}
		if (PAD_justPressed(BTN_DOWN) || PAD_justRepeated(BTN_DOWN)) {
			if (cursor_row < 4) {
				cursor_row++;
				int new_len = row_length(cursor_row);
				if (cursor_col >= new_len) cursor_col = new_len - 1;
				redraw = 1;
			}
		}
		if (PAD_justPressed(BTN_LEFT) || PAD_justRepeated(BTN_LEFT)) {
			if (cursor_col > 0) {
				cursor_col--;
				redraw = 1;
			}
		}
		if (PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_RIGHT)) {
			if (cursor_col < row_length(cursor_row) - 1) {
				cursor_col++;
				redraw = 1;
			}
		}

		// Toggle layout with Select
		if (PAD_justPressed(BTN_SELECT)) {
			current_layout = (current_layout == layout_lower) ? layout_upper : layout_lower;
			redraw = 1;
		}

		if (redraw) {
			GFX_clear(screen);

			// Title
			if (opts && opts->title && g_font_small) {
				SDL_Surface* title_text = TTF_RenderUTF8_Blended(g_font_small, opts->title, COLOR_WHITE);
				if (title_text) {
					SDL_Rect pos = {DP(16), DP(8), title_text->w, title_text->h};
					SDL_BlitSurface(title_text, NULL, screen, &pos);
					SDL_FreeSurface(title_text);
				}
			}

			// Text input area (pill background)
			int input_y = DP(40);
			int input_h = DP(32);
			SDL_Rect input_bg = {DP(16), input_y, screen->w - DP(32), input_h};
			GFX_blitPill(ASSET_BLACK_PILL, screen, &input_bg);

			// Current text
			if (g_font_large) {
				char display_text[1024];
				snprintf(display_text, sizeof(display_text), "%s_", text);  // Add cursor
				SDL_Surface* text_surf = TTF_RenderUTF8_Blended(g_font_large, display_text, COLOR_WHITE);
				if (text_surf) {
					int text_x = DP(24);
					int text_y = input_y + (input_h - text_surf->h) / 2;
					SDL_Rect pos = {text_x, text_y, text_surf->w, text_surf->h};
					SDL_BlitSurface(text_surf, NULL, screen, &pos);
					SDL_FreeSurface(text_surf);
				}
			}

			// Keyboard
			for (int row = 0; row < 5; row++) {
				int row_len = row_length(row);
				int total_width = row_len * key_w + (row_len - 1) * key_spacing;
				int start_x = (screen->w - total_width) / 2;
				int y = kb_start_y + row * (key_h + key_spacing);

				for (int col = 0; col < row_len; col++) {
					const char* key = current_layout[row][col];
					if (!key) break;

					int x = start_x + col * (key_w + key_spacing);

					// Key width (special keys are wider)
					int kw = key_w;
					if (strcmp(key, " ") == 0) kw = key_w * 4;
					else if (strcmp(key, "ABC") == 0 || strcmp(key, "abc") == 0 || strcmp(key, "OK") == 0) {
						kw = key_w * 2;
					}

					// Background
					SDL_Rect key_rect = {x, y, kw, key_h};
					if (row == cursor_row && col == cursor_col) {
						GFX_blitPill(ASSET_WHITE_PILL, screen, &key_rect);
					} else {
						GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &key_rect);
					}

					// Key label
					SDL_Color color = (row == cursor_row && col == cursor_col) ? COLOR_BLACK : COLOR_WHITE;
					const char* label = (strcmp(key, " ") == 0) ? "SPACE" : key;

					SDL_Surface* key_text = TTF_RenderUTF8_Blended(g_font_large, label, color);
					if (key_text) {
						int tx = x + (kw - key_text->w) / 2;
						int ty = y + (key_h - key_text->h) / 2;
						SDL_Rect tpos = {tx, ty, key_text->w, key_text->h};
						SDL_BlitSurface(key_text, NULL, screen, &tpos);
						SDL_FreeSurface(key_text);
					}
				}
			}

			// Button hints
			char* hints[] = {"Y", "CANCEL", "B", "DELETE", "A", "SELECT", NULL};
			GFX_blitButtonGroup(hints, 2, screen, 1);

			GFX_flip(screen);
			redraw = 0;
		} else {
			GFX_sync();
		}
	}
}
