#include "ui_keyboard.h"
#include "fonts.h"
#include "api.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>

// Keyboard layouts (5 rows, 13 columns max)
// Using \0 for empty cells to mark row end
#define LAYOUT_ROWS 5
#define LAYOUT_COLS 13

static const char* layout_lower[LAYOUT_ROWS][LAYOUT_COLS] = {
	{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", NULL},
	{"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", NULL},
	{"a", "s", "d", "f", "g", "h", "j", "k", "l", NULL},
	{"z", "x", "c", "v", "b", "n", "m", NULL},
	{"ABC", "SPACE", "OK", NULL}
};

static const char* layout_upper[LAYOUT_ROWS][LAYOUT_COLS] = {
	{"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", NULL},
	{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", NULL},
	{"A", "S", "D", "F", "G", "H", "J", "K", "L", NULL},
	{"Z", "X", "C", "V", "B", "N", "M", NULL},
	{"#+=", "SPACE", "OK", NULL}
};

static const char* layout_symbol[LAYOUT_ROWS][LAYOUT_COLS] = {
	{"~", "`", "-", "_", "=", "+", "[", "]", "{", "}", NULL},
	{"\\", "|", ";", ":", "'", "\"", ",", ".", "<", ">", NULL},
	{"/", "?", "!", "@", "#", "$", "%", "^", "&", NULL},
	{"*", "(", ")", "-", "_", "=", "+", NULL},
	{"abc", "SPACE", "OK", NULL}
};

// Current layout (0=lower, 1=upper, 2=symbol)
static int current_layout_idx = 0;

static const char* (*get_layout(void))[LAYOUT_COLS] {
	switch (current_layout_idx) {
		case 1: return layout_upper;
		case 2: return layout_symbol;
		default: return layout_lower;
	}
}

// Count valid keys in row
static int row_length(int row) {
	const char* (*layout)[LAYOUT_COLS] = get_layout();
	int len = 0;
	for (int i = 0; i < LAYOUT_COLS && layout[row][i]; i++) {
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
	current_layout_idx = 0;

	int redraw = 1;
	int show_setting = 0;

	PWR_disableAutosleep();

	// Layout constants
	int margin = DP(8);
	int spacing = DP(3);
	int key_size = (screen->w - (2 * margin) - (9 * spacing)) / 10;  // 10 keys in widest row
	int kb_height = (5 * key_size) + (4 * spacing);
	int kb_start_y = screen->h - kb_height - DP(40);  // Room for button hints

	// Colors
	Uint32 color_key_bg = SDL_MapRGB(screen->format, TRIAD_DARK_GRAY);
	Uint32 color_key_selected = SDL_MapRGB(screen->format, TRIAD_WHITE);
	Uint32 color_input_bg = SDL_MapRGB(screen->format, 0x1a, 0x1a, 0x1a);

	while (1) {
		GFX_startFrame();
		PWR_update(&redraw, &show_setting, NULL, NULL);

		// Input handling
		PAD_poll();

		const char* (*layout)[LAYOUT_COLS] = get_layout();

		// Confirm (A button)
		if (PAD_justPressed(BTN_A)) {
			const char* key = layout[cursor_row][cursor_col];
			if (key) {
				if (strcmp(key, "OK") == 0) {
					result.exit_code = EXIT_SUCCESS_CODE;
					result.text = strdup(text);
					return result;
				} else if (strcmp(key, "ABC") == 0) {
					current_layout_idx = 1;  // uppercase
					redraw = 1;
				} else if (strcmp(key, "abc") == 0) {
					current_layout_idx = 0;  // lowercase
					redraw = 1;
				} else if (strcmp(key, "#+=") == 0) {
					current_layout_idx = 2;  // symbols
					redraw = 1;
				} else if (strcmp(key, "SPACE") == 0) {
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

		// Navigation with wrapping
		if (PAD_justPressed(BTN_UP) || PAD_justRepeated(BTN_UP)) {
			cursor_row--;
			if (cursor_row < 0) cursor_row = LAYOUT_ROWS - 1;
			int new_len = row_length(cursor_row);
			if (cursor_col >= new_len) cursor_col = new_len - 1;
			redraw = 1;
		}
		if (PAD_justPressed(BTN_DOWN) || PAD_justRepeated(BTN_DOWN)) {
			cursor_row++;
			if (cursor_row >= LAYOUT_ROWS) cursor_row = 0;
			int new_len = row_length(cursor_row);
			if (cursor_col >= new_len) cursor_col = new_len - 1;
			redraw = 1;
		}
		if (PAD_justPressed(BTN_LEFT) || PAD_justRepeated(BTN_LEFT)) {
			cursor_col--;
			if (cursor_col < 0) cursor_col = row_length(cursor_row) - 1;
			redraw = 1;
		}
		if (PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_RIGHT)) {
			cursor_col++;
			if (cursor_col >= row_length(cursor_row)) cursor_col = 0;
			redraw = 1;
		}

		// Cycle layouts with Select
		if (PAD_justPressed(BTN_SELECT)) {
			current_layout_idx = (current_layout_idx + 1) % 3;
			int new_len = row_length(cursor_row);
			if (cursor_col >= new_len) cursor_col = new_len - 1;
			redraw = 1;
		}

		if (redraw) {
			GFX_clear(screen);

			// Title
			int title_h = 0;
			if (opts && opts->title && g_font_small) {
				SDL_Surface* title_text = TTF_RenderUTF8_Blended(g_font_small, opts->title, COLOR_WHITE);
				if (title_text) {
					SDL_Rect pos = {(screen->w - title_text->w) / 2, DP(8), title_text->w, title_text->h};
					SDL_BlitSurface(title_text, NULL, screen, &pos);
					title_h = title_text->h + DP(8);
					SDL_FreeSurface(title_text);
				}
			}

			// Text input area
			int input_y = DP(8) + title_h;
			int input_h = DP(32);
			SDL_Rect input_bg = {margin, input_y, screen->w - (2 * margin), input_h};
			SDL_FillRect(screen, &input_bg, color_input_bg);

			// Current text with cursor
			if (g_font_large) {
				char display_text[1024];
				snprintf(display_text, sizeof(display_text), "%s_", text);
				SDL_Surface* text_surf = TTF_RenderUTF8_Blended(g_font_large, display_text, COLOR_WHITE);
				if (text_surf) {
					int text_x = margin + DP(8);
					int text_y = input_y + (input_h - text_surf->h) / 2;
					SDL_Rect pos = {text_x, text_y, text_surf->w, text_surf->h};
					SDL_BlitSurface(text_surf, NULL, screen, &pos);
					SDL_FreeSurface(text_surf);
				}
			}

			// Draw keyboard
			for (int row = 0; row < LAYOUT_ROWS; row++) {
				int row_len = row_length(row);
				if (row_len == 0) continue;

				// Calculate row width and starting position
				int row_width;
				int start_x;

				if (row == LAYOUT_ROWS - 1) {
					// Bottom row: special keys span full width
					row_width = screen->w - (2 * margin);
					start_x = margin;
				} else {
					// Regular rows: uniform key sizes, centered
					row_width = (row_len * key_size) + ((row_len - 1) * spacing);
					start_x = (screen->w - row_width) / 2;
				}

				int y = kb_start_y + row * (key_size + spacing);

				for (int col = 0; col < row_len; col++) {
					const char* key = layout[row][col];
					if (!key) break;

					int x, kw;

					if (row == LAYOUT_ROWS - 1) {
						// Bottom row: divide evenly
						kw = (row_width - ((row_len - 1) * spacing)) / row_len;
						x = start_x + col * (kw + spacing);
					} else {
						// Regular rows
						kw = key_size;
						x = start_x + col * (key_size + spacing);
					}

					// Draw key background
					SDL_Rect key_rect = {x, y, kw, key_size};
					bool selected = (row == cursor_row && col == cursor_col);
					SDL_FillRect(screen, &key_rect, selected ? color_key_selected : color_key_bg);

					// Draw key label
					SDL_Color text_color = selected ? COLOR_BLACK : COLOR_WHITE;
					SDL_Surface* key_text = TTF_RenderUTF8_Blended(g_font_large, key, text_color);
					if (key_text) {
						int tx = x + (kw - key_text->w) / 2;
						int ty = y + (key_size - key_text->h) / 2;
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
