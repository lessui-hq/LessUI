#include "ui_message.h"
#include "fonts.h"
#include "shui_utils.h"
#include "api.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// For SDL1, we need a manual surface scaling function
#ifndef USE_SDL2
static SDL_Surface* scale_surface(SDL_Surface* surface, int width, int height) {
	SDL_Surface* scaled = SDL_CreateRGBSurface(
		surface->flags, width, height,
		surface->format->BitsPerPixel,
		surface->format->Rmask, surface->format->Gmask,
		surface->format->Bmask, surface->format->Amask
	);
	if (!scaled) return NULL;

	int bpp = surface->format->BytesPerPixel;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			int xo1 = x * surface->w / width;
			int xo2 = (x + 1) * surface->w / width;
			if (xo2 <= xo1) xo2 = xo1 + 1;
			int yo1 = y * surface->h / height;
			int yo2 = (y + 1) * surface->h / height;
			if (yo2 <= yo1) yo2 = yo1 + 1;
			int n = (xo2 - xo1) * (yo2 - yo1);

			int v[4] = {0, 0, 0, 0};
			for (int xo = xo1; xo < xo2; xo++) {
				for (int yo = yo1; yo < yo2; yo++) {
					Uint8* ps = (Uint8*)surface->pixels + yo * surface->pitch + xo * bpp;
					for (int i = 0; i < bpp; i++) {
						v[i] += ps[i];
					}
				}
			}

			Uint8* pd = (Uint8*)scaled->pixels + y * scaled->pitch + x * bpp;
			for (int i = 0; i < bpp; i++) {
				pd[i] = v[i] / n;
			}
		}
	}
	return scaled;
}
#endif

// Maximum lines to display
#define MAX_LINES 8
#define MAX_SUBTEXT_LINES 4
#define SUBTEXT_GAP 12  // Gap between main text and subtext in dp

// Word structure for text wrapping
typedef struct {
	char text[256];
	int width;
	bool is_newline;  // True if this is a forced newline marker
} Word;

// Line structure for rendered text
typedef struct {
	char text[512];
	int width;
} Line;

ExitCode ui_message_show(SDL_Surface* screen, const MessageOptions* opts) {
	if (!screen || !opts || !g_font_large) return EXIT_ERROR;

	struct timeval start_time;
	gettimeofday(&start_time, NULL);

	int show_setting = 0;
	int redraw = 1;

	if (opts->timeout <= 0) {
		PWR_disableAutosleep();
	}

	// Process message text
	char processed_text[1024] = "";
	if (opts->text) {
		unescapeNewlines(processed_text, opts->text, sizeof(processed_text));
	}

	// Parse into words (handling newlines)
	Word words[256];
	int word_count = 0;
	int word_height = 0;

	char temp[1024];
	strncpy(temp, processed_text, sizeof(temp) - 1);
	temp[sizeof(temp) - 1] = '\0';

	char* token = strtok(temp, " ");
	while (token && word_count < 256) {
		// Check for embedded newlines
		char* nl = strchr(token, '\n');
		while (nl) {
			// Part before newline
			*nl = '\0';
			if (strlen(token) > 0) {
				strncpy(words[word_count].text, token, sizeof(words[word_count].text) - 1);
				TTF_SizeUTF8(g_font_large, token, &words[word_count].width, &word_height);
				words[word_count].is_newline = false;
				word_count++;
			}
			// Newline marker
			words[word_count].text[0] = '\0';
			words[word_count].width = 0;
			words[word_count].is_newline = true;
			word_count++;
			// Continue with remainder
			token = nl + 1;
			nl = strchr(token, '\n');
		}
		// Remaining part
		if (strlen(token) > 0) {
			strncpy(words[word_count].text, token, sizeof(words[word_count].text) - 1);
			TTF_SizeUTF8(g_font_large, token, &words[word_count].width, &word_height);
			words[word_count].is_newline = false;
			word_count++;
		}
		token = strtok(NULL, " ");
	}

	// Calculate space width
	int space_width = 0;
	TTF_SizeUTF8(g_font_large, " ", &space_width, NULL);

	// Build lines with word wrap
	Line lines[MAX_LINES];
	int line_count = 0;
	int max_width = screen->w - DP(32);  // Padding on each side

	for (int i = 0; i < MAX_LINES; i++) {
		lines[i].text[0] = '\0';
		lines[i].width = 0;
	}

	for (int i = 0; i < word_count && line_count < MAX_LINES; i++) {
		if (words[i].is_newline) {
			line_count++;
			continue;
		}

		int new_width = lines[line_count].width + words[i].width;
		if (lines[line_count].width > 0) {
			new_width += space_width;
		}

		if (lines[line_count].width == 0) {
			// First word on line
			strncpy(lines[line_count].text, words[i].text, sizeof(lines[line_count].text) - 1);
			lines[line_count].width = words[i].width;
		} else if (new_width <= max_width) {
			// Fits on current line
			strncat(lines[line_count].text, " ", sizeof(lines[line_count].text) - strlen(lines[line_count].text) - 1);
			strncat(lines[line_count].text, words[i].text, sizeof(lines[line_count].text) - strlen(lines[line_count].text) - 1);
			lines[line_count].width = new_width;
		} else {
			// Start new line
			line_count++;
			if (line_count < MAX_LINES) {
				strncpy(lines[line_count].text, words[i].text, sizeof(lines[line_count].text) - 1);
				lines[line_count].width = words[i].width;
			}
		}
	}
	if (lines[line_count].width > 0) {
		line_count++;
	}

	// Process subtext (similar word-wrapping but with smaller font)
	Line subtext_lines[MAX_SUBTEXT_LINES];
	int subtext_line_count = 0;
	int subtext_line_height = 0;

	for (int i = 0; i < MAX_SUBTEXT_LINES; i++) {
		subtext_lines[i].text[0] = '\0';
		subtext_lines[i].width = 0;
	}

	if (opts->subtext && g_font_small) {
		char processed_subtext[1024] = "";
		unescapeNewlines(processed_subtext, opts->subtext, sizeof(processed_subtext));

		// Parse subtext into words
		Word subtext_words[256];
		int subtext_word_count = 0;

		char subtext_temp[1024];
		strncpy(subtext_temp, processed_subtext, sizeof(subtext_temp) - 1);
		subtext_temp[sizeof(subtext_temp) - 1] = '\0';

		char* st_token = strtok(subtext_temp, " ");
		while (st_token && subtext_word_count < 256) {
			char* nl = strchr(st_token, '\n');
			while (nl) {
				*nl = '\0';
				if (strlen(st_token) > 0) {
					strncpy(subtext_words[subtext_word_count].text, st_token,
					        sizeof(subtext_words[subtext_word_count].text) - 1);
					TTF_SizeUTF8(g_font_small, st_token,
					             &subtext_words[subtext_word_count].width, &subtext_line_height);
					subtext_words[subtext_word_count].is_newline = false;
					subtext_word_count++;
				}
				subtext_words[subtext_word_count].text[0] = '\0';
				subtext_words[subtext_word_count].width = 0;
				subtext_words[subtext_word_count].is_newline = true;
				subtext_word_count++;
				st_token = nl + 1;
				nl = strchr(st_token, '\n');
			}
			if (strlen(st_token) > 0) {
				strncpy(subtext_words[subtext_word_count].text, st_token,
				        sizeof(subtext_words[subtext_word_count].text) - 1);
				TTF_SizeUTF8(g_font_small, st_token,
				             &subtext_words[subtext_word_count].width, &subtext_line_height);
				subtext_words[subtext_word_count].is_newline = false;
				subtext_word_count++;
			}
			st_token = strtok(NULL, " ");
		}

		// Calculate space width for small font
		int subtext_space_width = 0;
		TTF_SizeUTF8(g_font_small, " ", &subtext_space_width, NULL);

		// Build subtext lines with word wrap
		for (int i = 0; i < subtext_word_count && subtext_line_count < MAX_SUBTEXT_LINES; i++) {
			if (subtext_words[i].is_newline) {
				subtext_line_count++;
				continue;
			}

			int new_width = subtext_lines[subtext_line_count].width + subtext_words[i].width;
			if (subtext_lines[subtext_line_count].width > 0) {
				new_width += subtext_space_width;
			}

			if (subtext_lines[subtext_line_count].width == 0) {
				strncpy(subtext_lines[subtext_line_count].text, subtext_words[i].text,
				        sizeof(subtext_lines[subtext_line_count].text) - 1);
				subtext_lines[subtext_line_count].width = subtext_words[i].width;
			} else if (new_width <= max_width) {
				strncat(subtext_lines[subtext_line_count].text, " ",
				        sizeof(subtext_lines[subtext_line_count].text) -
				            strlen(subtext_lines[subtext_line_count].text) - 1);
				strncat(subtext_lines[subtext_line_count].text, subtext_words[i].text,
				        sizeof(subtext_lines[subtext_line_count].text) -
				            strlen(subtext_lines[subtext_line_count].text) - 1);
				subtext_lines[subtext_line_count].width = new_width;
			} else {
				subtext_line_count++;
				if (subtext_line_count < MAX_SUBTEXT_LINES) {
					strncpy(subtext_lines[subtext_line_count].text, subtext_words[i].text,
					        sizeof(subtext_lines[subtext_line_count].text) - 1);
					subtext_lines[subtext_line_count].width = subtext_words[i].width;
				}
			}
		}
		if (subtext_lines[subtext_line_count].width > 0) {
			subtext_line_count++;
		}
	}

	// Drain any stale input events from before the dialog appeared
	PAD_poll();
	PAD_reset();

	// Main loop
	while (1) {
		GFX_startFrame();
		PWR_update(&redraw, &show_setting, NULL, NULL);

		// Input handling
		PAD_poll();
		if (PAD_justPressed(BTN_A)) {
			return EXIT_SUCCESS_CODE;
		}
		if (PAD_justPressed(BTN_B)) {
			return EXIT_CANCEL;
		}
		if (PAD_justPressed(BTN_MENU)) {
			return EXIT_MENU;
		}

		if (redraw) {
			// Background color
			SDL_Color bg = {0, 0, 0, 255};
			if (opts->background_color) {
				bg = hexToColor(opts->background_color);
			}
			uint32_t bg_color = SDL_MapRGB(screen->format, bg.r, bg.g, bg.b);
			SDL_FillRect(screen, NULL, bg_color);

			// Background image
			if (opts->background_image) {
				SDL_Surface* img = IMG_Load(opts->background_image);
				if (img) {
					// Scale to fit screen while maintaining aspect ratio
					float scale_x = (float)screen->w / img->w;
					float scale_y = (float)screen->h / img->h;
					float scale = (scale_x < scale_y) ? scale_x : scale_y;

					int dst_w = img->w * scale;
					int dst_h = img->h * scale;
					int dst_x = (screen->w - dst_w) / 2;
					int dst_y = (screen->h - dst_h) / 2;

					SDL_Rect dst_rect = {dst_x, dst_y, dst_w, dst_h};
#ifdef USE_SDL2
					SDL_BlitScaled(img, NULL, screen, &dst_rect);
#else
					if (img->w == screen->w && img->h == screen->h) {
						SDL_BlitSurface(img, NULL, screen, &dst_rect);
					} else {
						SDL_Surface* scaled = scale_surface(img, dst_w, dst_h);
						if (scaled) {
							SDL_BlitSurface(scaled, NULL, screen, &dst_rect);
							SDL_FreeSurface(scaled);
						}
					}
#endif
					SDL_FreeSurface(img);
				}
			}

			// Time left display
			int time_offset = 0;
			if (opts->show_time_left && opts->timeout > 0 && g_font_small) {
				struct timeval now;
				gettimeofday(&now, NULL);
				int elapsed = now.tv_sec - start_time.tv_sec;
				int remaining = opts->timeout - elapsed;
				if (remaining < 0) remaining = 0;

				char time_str[64];
				if (remaining == 1) {
					snprintf(time_str, sizeof(time_str), "Time left: 1 second");
				} else {
					snprintf(time_str, sizeof(time_str), "Time left: %d seconds", remaining);
				}

				SDL_Surface* time_text = TTF_RenderUTF8_Blended(g_font_small, time_str, COLOR_WHITE);
				if (time_text) {
					SDL_Rect time_pos = {DP(8), DP(8), time_text->w, time_text->h};
					SDL_BlitSurface(time_text, NULL, screen, &time_pos);
					time_offset = time_text->h + DP(8);
					SDL_FreeSurface(time_text);
				}
			}

			// Calculate total message height (including subtext if present)
			int main_text_height = line_count * word_height + (line_count - 1) * DP(4);
			int subtext_height = 0;
			if (subtext_line_count > 0) {
				subtext_height = DP(SUBTEXT_GAP) + subtext_line_count * subtext_line_height +
				                 (subtext_line_count - 1) * DP(4);
			}
			int total_height = main_text_height + subtext_height;
			int start_y = (screen->h - total_height) / 2 + time_offset / 2;

			// Render message lines
			for (int i = 0; i < line_count; i++) {
				if (strlen(lines[i].text) == 0) continue;

				SDL_Surface* text = TTF_RenderUTF8_Blended(g_font_large, lines[i].text, COLOR_WHITE);
				if (!text) continue;

				int x = (screen->w - text->w) / 2;
				int y = start_y + i * (word_height + DP(4));

				// Optional pill background
				if (opts->show_pill) {
					int pill_x = x - DP(16);
					int pill_y = y - DP(4);
					int pill_w = text->w + DP(32);
					int pill_h = DP(28);
					SDL_Rect pill_rect = {pill_x, pill_y, pill_w, pill_h};
					GFX_blitPill(ASSET_BLACK_PILL, screen, &pill_rect);
				}

				SDL_Rect pos = {x, y, text->w, text->h};
				SDL_BlitSurface(text, NULL, screen, &pos);
				SDL_FreeSurface(text);
			}

			// Render subtext lines (smaller, gray)
			if (subtext_line_count > 0 && g_font_small) {
				int subtext_start_y = start_y + main_text_height + DP(SUBTEXT_GAP);
				for (int i = 0; i < subtext_line_count; i++) {
					if (strlen(subtext_lines[i].text) == 0) continue;

					SDL_Surface* text = TTF_RenderUTF8_Blended(g_font_small, subtext_lines[i].text,
					                                           COLOR_GRAY);
					if (!text) continue;

					int x = (screen->w - text->w) / 2;
					int y = subtext_start_y + i * (subtext_line_height + DP(4));

					SDL_Rect pos = {x, y, text->w, text->h};
					SDL_BlitSurface(text, NULL, screen, &pos);
					SDL_FreeSurface(text);
				}
			}

			// Button hints (uppercase for UI consistency)
			if (opts->confirm_text || opts->cancel_text) {
				char* hints[5] = {NULL};
				int idx = 0;
				char cancel_upper[64] = "";
				char confirm_upper[64] = "";
				if (opts->cancel_text) {
					strncpy(cancel_upper, opts->cancel_text, sizeof(cancel_upper) - 1);
					toUppercase(cancel_upper);
					hints[idx++] = "B";
					hints[idx++] = cancel_upper;
				}
				if (opts->confirm_text) {
					strncpy(confirm_upper, opts->confirm_text, sizeof(confirm_upper) - 1);
					toUppercase(confirm_upper);
					hints[idx++] = "A";
					hints[idx++] = confirm_upper;
				}
				GFX_blitButtonGroup(hints, (opts->confirm_text ? 1 : 0), screen, 1);
			}

			GFX_flip(screen);
			redraw = 0;
		} else {
			GFX_sync();
		}

		// Check timeout
		if (opts->timeout > 0) {
			struct timeval now;
			gettimeofday(&now, NULL);
			if (now.tv_sec - start_time.tv_sec >= opts->timeout) {
				return EXIT_TIMEOUT;
			}
			// Redraw for time left update
			if (opts->show_time_left) {
				redraw = 1;
			}
		}
	}

	return EXIT_SUCCESS_CODE;
}
