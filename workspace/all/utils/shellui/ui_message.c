#include "ui_message.h"
#include "api.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#ifdef USE_SDL2
#include <SDL2/SDL_image.h>
#else
#include <SDL/SDL_image.h>
#endif

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

// Fonts for message rendering
static TTF_Font* font_large = NULL;
static TTF_Font* font_small = NULL;

// Maximum lines to display
#define MAX_LINES 8

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

// Convert hex color string to SDL_Color
static SDL_Color hex_to_color(const char* hex) {
	SDL_Color color = {0, 0, 0, 255};
	if (!hex || hex[0] != '#' || strlen(hex) < 7) {
		return color;
	}

	unsigned int r, g, b;
	if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
		color.r = r;
		color.g = g;
		color.b = b;
	}
	return color;
}

// Replace \n escape sequences with actual newlines
static void process_escapes(const char* src, char* dst, size_t dst_size) {
	size_t si = 0, di = 0;
	while (src[si] && di < dst_size - 1) {
		if (src[si] == '\\' && src[si + 1] == 'n') {
			dst[di++] = '\n';
			si += 2;
		} else {
			dst[di++] = src[si++];
		}
	}
	dst[di] = '\0';
}

// Trim whitespace from string
static void trim(char* s) {
	if (!s || !*s) return;

	// Trim leading
	char* start = s;
	while (*start && isspace(*start)) start++;

	// Trim trailing
	char* end = start + strlen(start) - 1;
	while (end > start && isspace(*end)) *end-- = '\0';

	// Shift if needed
	if (start != s) {
		memmove(s, start, strlen(start) + 1);
	}
}

void ui_message_init(void) {
	if (font_large) return;

	font_large = TTF_OpenFont(FONT_PATH, DP(FONT_LARGE));
	if (font_large) {
		TTF_SetFontStyle(font_large, TTF_STYLE_BOLD);
	}

	font_small = TTF_OpenFont(FONT_PATH, DP(FONT_SMALL));
}

void ui_message_cleanup(void) {
	if (font_large) {
		TTF_CloseFont(font_large);
		font_large = NULL;
	}
	if (font_small) {
		TTF_CloseFont(font_small);
		font_small = NULL;
	}
}

ExitCode ui_message_show(SDL_Surface* screen, const MessageOptions* opts) {
	if (!screen || !opts) return EXIT_ERROR;

	ui_message_init();
	if (!font_large) return EXIT_ERROR;

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
		process_escapes(opts->text, processed_text, sizeof(processed_text));
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
				TTF_SizeUTF8(font_large, token, &words[word_count].width, &word_height);
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
			TTF_SizeUTF8(font_large, token, &words[word_count].width, &word_height);
			words[word_count].is_newline = false;
			word_count++;
		}
		token = strtok(NULL, " ");
	}

	// Calculate space width
	int space_width = 0;
	TTF_SizeUTF8(font_large, " ", &space_width, NULL);

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
				bg = hex_to_color(opts->background_color);
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
			if (opts->show_time_left && opts->timeout > 0 && font_small) {
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

				SDL_Surface* time_text = TTF_RenderUTF8_Blended(font_small, time_str, COLOR_WHITE);
				if (time_text) {
					SDL_Rect time_pos = {DP(8), DP(8), time_text->w, time_text->h};
					SDL_BlitSurface(time_text, NULL, screen, &time_pos);
					time_offset = time_text->h + DP(8);
					SDL_FreeSurface(time_text);
				}
			}

			// Calculate total message height
			int total_height = line_count * word_height + (line_count - 1) * DP(4);
			int start_y = (screen->h - total_height) / 2 + time_offset / 2;

			// Render message lines
			for (int i = 0; i < line_count; i++) {
				if (strlen(lines[i].text) == 0) continue;

				SDL_Surface* text = TTF_RenderUTF8_Blended(font_large, lines[i].text, COLOR_WHITE);
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

			// Button hints
			if (opts->confirm_text || opts->cancel_text) {
				char* hints[5] = {NULL};
				int idx = 0;
				if (opts->cancel_text) {
					hints[idx++] = "B";
					hints[idx++] = (char*)opts->cancel_text;
				}
				if (opts->confirm_text) {
					hints[idx++] = "A";
					hints[idx++] = (char*)opts->confirm_text;
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
