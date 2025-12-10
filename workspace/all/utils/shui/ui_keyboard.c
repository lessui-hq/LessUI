#include "ui_keyboard.h"
#include "api.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>

// Match minarch menu padding
#define OPTION_PADDING 8

// Keyboard layouts (4 rows, 10 columns max - iPhone style)
#define LAYOUT_ROWS 4
#define LAYOUT_COLS 10

// Layout indices
#define LAYOUT_LOWER 0
#define LAYOUT_UPPER 1
#define LAYOUT_NUM 2
#define LAYOUT_SYM 3
#define LAYOUT_COUNT 4

// Lowercase letters
static const char* layout_lower[LAYOUT_ROWS][LAYOUT_COLS] = {
	{"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
	{"a", "s", "d", "f", "g", "h", "j", "k", "l", NULL},
	{"aA", "z", "x", "c", "v", "b", "n", "m", "DEL", NULL},
	{"123", "SPACE", "OK", NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

// Uppercase letters
static const char* layout_upper[LAYOUT_ROWS][LAYOUT_COLS] = {
	{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
	{"A", "S", "D", "F", "G", "H", "J", "K", "L", NULL},
	{"Aa", "Z", "X", "C", "V", "B", "N", "M", "DEL", NULL},
	{"123", "SPACE", "OK", NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

// Numbers and primary symbols
static const char* layout_num[LAYOUT_ROWS][LAYOUT_COLS] = {
	{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
	{"-", "/", ":", ";", "(", ")", "$", "&", "@", "\""},
	{"#+=", ".", ",", "?", "!", "'", "DEL", NULL, NULL, NULL},
	{"ABC", "SPACE", "OK", NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

// Additional symbols
static const char* layout_sym[LAYOUT_ROWS][LAYOUT_COLS] = {
	{"[", "]", "{", "}", "#", "%", "^", "*", "+", "="},
	{"_", "\\", "|", "~", "<", ">", NULL, NULL, NULL, NULL},
	{"123", ".", ",", "?", "!", "'", "DEL", NULL, NULL, NULL},
	{"ABC", "SPACE", "OK", NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

// Get layout array by index
static const char* (*get_layout(int layout_idx))[LAYOUT_COLS] {
	switch (layout_idx) {
		case LAYOUT_UPPER: return layout_upper;
		case LAYOUT_NUM: return layout_num;
		case LAYOUT_SYM: return layout_sym;
		default: return layout_lower;
	}
}

// Count valid keys in row for given layout
static int row_length(int layout_idx, int row) {
	const char* (*layout)[LAYOUT_COLS] = get_layout(layout_idx);
	int len = 0;
	for (int i = 0; i < LAYOUT_COLS && layout[row][i]; i++) {
		len++;
	}
	return len;
}

// Delete last character from text buffer, returns 1 if changed
static int do_backspace(char* text) {
	int len = strlen(text);
	if (len > 0) {
		text[len - 1] = '\0';
		return 1;
	}
	return 0;
}

// Change layout and adjust cursor if needed
static void set_layout(int* layout_idx, int new_layout, int cursor_row, int* cursor_col) {
	*layout_idx = new_layout;
	int new_len = row_length(new_layout, cursor_row);
	if (*cursor_col >= new_len) {
		*cursor_col = new_len - 1;
	}
}

KeyboardResult ui_keyboard_show(SDL_Surface* screen, const KeyboardOptions* opts) {
	KeyboardResult result = {EXIT_ERROR, NULL};
	if (!screen || !font.medium) return result;

	// Initialize text buffer
	char text[1024] = "";
	if (opts && opts->initial_value) {
		strncpy(text, opts->initial_value, sizeof(text) - 1);
	}

	// Cursor and layout state (all local, no globals)
	int cursor_row = 0;
	int cursor_col = 0;
	int layout_idx = LAYOUT_LOWER;

	int redraw = 1;
	int show_setting = 0;

	PWR_disableAutosleep();

	// Dynamic layout calculation
	// Available vertical space: between input area and button hints
	int title_area = DP(ui.pill_height);  // Title row
	int input_area = DP(ui.pill_height);  // Input field
	int button_area = DP(ui.pill_height + ui.edge_padding);  // Button hints at bottom
	int spacing = DP(2);
	int edge_padding = DP(ui.edge_padding);

	// Calculate key size from available height (square keys)
	int available_height = screen->h - title_area - input_area - button_area - edge_padding;
	int key_size_from_height = (available_height - (LAYOUT_ROWS - 1) * spacing) / LAYOUT_ROWS;

	// Also constrain by available width (so keys don't get cut off)
	int available_width = screen->w - (2 * edge_padding);
	int key_size_from_width = (available_width - (LAYOUT_COLS - 1) * spacing) / LAYOUT_COLS;

	// Use smaller of the two to ensure keyboard fits both dimensions
	int key_size = (key_size_from_height < key_size_from_width) ? key_size_from_height : key_size_from_width;

	// Calculate keyboard width and centering
	int kb_width = (LAYOUT_COLS * key_size) + ((LAYOUT_COLS - 1) * spacing);
	int kb_start_x = (screen->w - kb_width) / 2;
	int kb_start_y = title_area + input_area + DP(ui.edge_padding / 2);

	// Colors
	Uint32 color_key_bg = SDL_MapRGB(screen->format, TRIAD_DARK_GRAY);
	Uint32 color_key_selected = SDL_MapRGB(screen->format, TRIAD_WHITE);
	Uint32 color_input_bg = SDL_MapRGB(screen->format, 0x1a, 0x1a, 0x1a);

	// Drain any stale input events from before the keyboard appeared
	PAD_poll();
	PAD_reset();

	while (1) {
		GFX_startFrame();
		PWR_update(&redraw, &show_setting, NULL, NULL);

		// Input handling
		PAD_poll();

		const char* (*layout)[LAYOUT_COLS] = get_layout(layout_idx);

		// Confirm (A button)
		if (PAD_justPressed(BTN_A)) {
			const char* key = layout[cursor_row][cursor_col];
			if (key) {
				if (strcmp(key, "OK") == 0) {
					result.exit_code = EXIT_SUCCESS_CODE;
					result.text = strdup(text);
					return result;
				} else if (strcmp(key, "aA") == 0 || strcmp(key, "Aa") == 0) {
					// Toggle between lowercase and uppercase
					int new_layout = (layout_idx == LAYOUT_LOWER) ? LAYOUT_UPPER : LAYOUT_LOWER;
					set_layout(&layout_idx, new_layout, cursor_row, &cursor_col);
					redraw = 1;
				} else if (strcmp(key, "DEL") == 0) {
					redraw = do_backspace(text);
				} else if (strcmp(key, "123") == 0) {
					set_layout(&layout_idx, LAYOUT_NUM, cursor_row, &cursor_col);
					redraw = 1;
				} else if (strcmp(key, "ABC") == 0) {
					set_layout(&layout_idx, LAYOUT_LOWER, cursor_row, &cursor_col);
					redraw = 1;
				} else if (strcmp(key, "#+=") == 0) {
					set_layout(&layout_idx, LAYOUT_SYM, cursor_row, &cursor_col);
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
			redraw = do_backspace(text);
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
			int row_len = row_length(layout_idx, cursor_row);
			if (cursor_col >= row_len) cursor_col = row_len - 1;
			redraw = 1;
		}
		if (PAD_justPressed(BTN_DOWN) || PAD_justRepeated(BTN_DOWN)) {
			cursor_row++;
			if (cursor_row >= LAYOUT_ROWS) cursor_row = 0;
			int row_len = row_length(layout_idx, cursor_row);
			if (cursor_col >= row_len) cursor_col = row_len - 1;
			redraw = 1;
		}
		if (PAD_justPressed(BTN_LEFT) || PAD_justRepeated(BTN_LEFT)) {
			cursor_col--;
			if (cursor_col < 0) cursor_col = row_length(layout_idx, cursor_row) - 1;
			redraw = 1;
		}
		if (PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_RIGHT)) {
			cursor_col++;
			if (cursor_col >= row_length(layout_idx, cursor_row)) cursor_col = 0;
			redraw = 1;
		}

		// Cycle layouts with Select
		if (PAD_justPressed(BTN_SELECT)) {
			set_layout(&layout_idx, (layout_idx + 1) % LAYOUT_COUNT, cursor_row, &cursor_col);
			redraw = 1;
		}

		if (redraw) {
			// Refresh layout pointer to reflect any layout changes from input handling
			layout = get_layout(layout_idx);

			GFX_clear(screen);

			// Title (centered horizontally, vertically centered in title_area)
			if (opts && opts->title) {
				SDL_Surface* title_text = TTF_RenderUTF8_Blended(font.medium, opts->title, COLOR_GRAY);
				if (title_text) {
					int title_y = GFX_centerTextY(font.medium, DP(ui.pill_height));
					SDL_Rect pos = {(screen->w - title_text->w) / 2, title_y, title_text->w, title_text->h};
					SDL_BlitSurface(title_text, NULL, screen, &pos);
					SDL_FreeSurface(title_text);
				}
			}

			// Text input area (pill-style like list items)
			int input_y = title_area;
			int input_h = DP(ui.pill_height);
			int input_x = kb_start_x;
			int input_w = kb_width;
			SDL_FillRect(screen, &(SDL_Rect){input_x, input_y, input_w, input_h}, color_input_bg);

			// Current text with cursor
			char display_text[1024];
			snprintf(display_text, sizeof(display_text), "%s_", text);
			SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.medium, display_text, COLOR_WHITE);
			if (text_surf) {
				int text_x = input_x + DP(OPTION_PADDING);
				int text_y = input_y + GFX_centerTextY(font.medium, input_h);
				// Clip text if too wide
				int max_text_w = input_w - DP(OPTION_PADDING * 2);
				SDL_Rect src = {0, 0, text_surf->w > max_text_w ? max_text_w : text_surf->w, text_surf->h};
				if (text_surf->w > max_text_w) {
					// Show end of text when too long
					src.x = text_surf->w - max_text_w;
				}
				SDL_BlitSurface(text_surf, &src, screen, &(SDL_Rect){text_x, text_y, src.w, text_surf->h});
				SDL_FreeSurface(text_surf);
			}

			// Draw keyboard (square keys, centered)
			for (int row = 0; row < LAYOUT_ROWS; row++) {
				int row_len = row_length(layout_idx, row);
				if (row_len == 0) continue;

				// Calculate row starting position (centered within keyboard width)
				int row_width;
				int start_x;

				if (row == LAYOUT_ROWS - 1) {
					// Bottom row: special keys span keyboard width
					row_width = kb_width;
					start_x = kb_start_x;
				} else {
					// Regular rows: square keys, centered
					row_width = (row_len * key_size) + ((row_len - 1) * spacing);
					start_x = kb_start_x + (kb_width - row_width) / 2;
				}

				int y = kb_start_y + row * (key_size + spacing);

				for (int col = 0; col < row_len; col++) {
					const char* key = layout[row][col];
					if (!key) break;

					int x, kw;

					if (row == LAYOUT_ROWS - 1) {
						// Bottom row: side keys are 2x key_size, SPACE takes remaining width
						// Layout is: [mode] [SPACE] [OK] where mode is 123/ABC
						int side_key_w = key_size * 2;
						int space_w = kb_width - (2 * side_key_w) - (2 * spacing);

						if (col == 0) {
							// Left key (123/ABC)
							kw = side_key_w;
							x = start_x;
						} else if (col == 1) {
							// SPACE (wide)
							kw = space_w;
							x = start_x + side_key_w + spacing;
						} else {
							// Right key (OK)
							kw = side_key_w;
							x = start_x + side_key_w + spacing + space_w + spacing;
						}
					} else {
						// Regular rows: square keys
						kw = key_size;
						x = start_x + col * (key_size + spacing);
					}

					// Draw key background
					SDL_Rect key_rect = {x, y, kw, key_size};
					bool selected = (row == cursor_row && col == cursor_col);
					SDL_FillRect(screen, &key_rect, selected ? color_key_selected : color_key_bg);

					// Draw key label (use small font for multi-char keys)
					SDL_Color text_color = selected ? COLOR_BLACK : COLOR_WHITE;
					TTF_Font* key_font = strlen(key) > 1 ? font.small : font.medium;
					SDL_Surface* key_text = TTF_RenderUTF8_Blended(key_font, key, text_color);
					if (key_text) {
						int tx = x + (kw - key_text->w) / 2;
						int ty = y + GFX_centerTextY(key_font, key_size);
						SDL_BlitSurface(key_text, NULL, screen, &(SDL_Rect){tx, ty, key_text->w, key_text->h});
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
