#include "ui_progress.h"
#include "fonts.h"
#include "api.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// Progress bar dimensions
#define BAR_WIDTH_PERCENT 60   // Bar is 60% of screen width
#define BAR_HEIGHT 16          // Bar height in dp

// Indeterminate animation
#define BOUNCE_WIDTH_PERCENT 20  // Bouncing segment is 20% of bar width
#define BOUNCE_PERIOD_MS 1500    // Full bounce cycle duration

// Helper to compare strings (NULL-safe)
static bool str_matches(const char* a, const char* b) {
	if (a == NULL && b == NULL) return true;
	if (a == NULL || b == NULL) return false;
	return strcmp(a, b) == 0;
}

// Check if context (title + message) matches
static bool context_matches(ProgressState* state, const ProgressOptions* opts) {
	return str_matches(state->context_title, opts->title) &&
	       str_matches(state->context_message, opts->message);
}

void ui_progress_update(ProgressState* state, const ProgressOptions* opts) {
	if (!state || !opts) return;

	bool same_context = context_matches(state, opts);

	if (!same_context || !state->active) {
		// New context: reset and set value immediately
		free(state->context_title);
		free(state->context_message);
		state->context_title = opts->title ? strdup(opts->title) : NULL;
		state->context_message = opts->message ? strdup(opts->message) : NULL;
		animated_value_set_immediate(&state->value, (float)opts->value);
	} else {
		// Same context: animate to new value
		animated_value_set(&state->value, (float)opts->value, PROGRESS_ANIMATION_MS);
	}

	state->indeterminate = opts->indeterminate;
	state->active = true;
}

bool ui_progress_needs_animation(ProgressState* state) {
	if (!state || !state->active) return false;
	return state->indeterminate || animated_value_is_animating(&state->value);
}

void ui_progress_reset(ProgressState* state) {
	if (!state) return;
	free(state->context_title);
	free(state->context_message);
	state->context_title = NULL;
	state->context_message = NULL;
	animated_value_reset(&state->value);
	state->indeterminate = false;
	state->active = false;
}

void ui_progress_render(SDL_Surface* screen, ProgressState* state, const ProgressOptions* opts) {
	if (!screen || !state || !opts || !g_font_large) return;

	GFX_clear(screen);

	int screen_cx = screen->w / 2;
	int screen_cy = screen->h / 2;

	// Calculate layout
	int bar_w = screen->w * BAR_WIDTH_PERCENT / 100;
	int bar_h = DP(BAR_HEIGHT);

	// Title (optional, above message)
	int title_h = 0;
	if (opts->title && g_font_small) {
		SDL_Surface* title_text = TTF_RenderUTF8_Blended(g_font_small, opts->title, COLOR_GRAY);
		if (title_text) {
			title_h = title_text->h + DP(8);
			SDL_FreeSurface(title_text);
		}
	}

	// Message height
	int msg_h = 0;
	if (opts->message) {
		int tw, th;
		TTF_SizeUTF8(g_font_large, opts->message, &tw, &th);
		msg_h = th;
	}

	// Subtext height (optional, below message)
	int subtext_h = 0;
	if (opts->subtext && g_font_small) {
		int tw, th;
		TTF_SizeUTF8(g_font_small, opts->subtext, &tw, &th);
		subtext_h = th + DP(4);  // Small gap between message and subtext
	}

	// Total content height: title + message + subtext + gap + bar
	int gap = DP(16);
	int total_h = title_h + msg_h + subtext_h + gap + bar_h;
	int start_y = screen_cy - total_h / 2;
	int bar_x = (screen->w - bar_w) / 2;

	// Draw title
	int y = start_y;
	if (opts->title && g_font_small) {
		SDL_Surface* title_text = TTF_RenderUTF8_Blended(g_font_small, opts->title, COLOR_GRAY);
		if (title_text) {
			int title_x = screen_cx - title_text->w / 2;
			SDL_Rect pos = {title_x, y, title_text->w, title_text->h};
			SDL_BlitSurface(title_text, NULL, screen, &pos);
			y += title_text->h + DP(8);
			SDL_FreeSurface(title_text);
		}
	}

	// Draw message
	if (opts->message) {
		SDL_Surface* msg_text = TTF_RenderUTF8_Blended(g_font_large, opts->message, COLOR_WHITE);
		if (msg_text) {
			int msg_x = screen_cx - msg_text->w / 2;
			SDL_Rect pos = {msg_x, y, msg_text->w, msg_text->h};
			SDL_BlitSurface(msg_text, NULL, screen, &pos);
			y += msg_text->h;
			SDL_FreeSurface(msg_text);
		}
	}

	// Draw subtext (smaller, gray, below message)
	if (opts->subtext && g_font_small) {
		y += DP(4);  // Small gap between message and subtext
		SDL_Surface* subtext = TTF_RenderUTF8_Blended(g_font_small, opts->subtext, COLOR_GRAY);
		if (subtext) {
			int subtext_x = screen_cx - subtext->w / 2;
			SDL_Rect pos = {subtext_x, y, subtext->w, subtext->h};
			SDL_BlitSurface(subtext, NULL, screen, &pos);
			y += subtext->h;
			SDL_FreeSurface(subtext);
		}
	}

	y += gap;
	int bar_y = y;

	// Draw progress bar background (dark gray)
	Uint32 bg_color = SDL_MapRGB(screen->format, 0x33, 0x33, 0x33);
	SDL_Rect bar_bg = {bar_x, bar_y, bar_w, bar_h};
	SDL_FillRect(screen, &bar_bg, bg_color);

	// Draw progress fill (white)
	Uint32 fill_color = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);

	if (state->indeterminate) {
		// Bouncing bar animation
		struct timeval now;
		gettimeofday(&now, NULL);
		long ms = (now.tv_sec * 1000 + now.tv_usec / 1000) % BOUNCE_PERIOD_MS;

		// Calculate position: ping-pong from 0 to 1 and back
		float progress = (float)ms / BOUNCE_PERIOD_MS;
		float pos;
		if (progress < 0.5f) {
			pos = progress * 2.0f;  // 0 to 1
		} else {
			pos = 2.0f - progress * 2.0f;  // 1 to 0
		}

		int bounce_w = bar_w * BOUNCE_WIDTH_PERCENT / 100;
		int max_x = bar_w - bounce_w;
		int fill_x = bar_x + (int)(pos * max_x);

		SDL_Rect fill_rect = {fill_x, bar_y, bounce_w, bar_h};
		SDL_FillRect(screen, &fill_rect, fill_color);
	} else {
		// Determinate progress bar - use animated value
		float current_value = animated_value_get(&state->value);
		if (current_value < 0) current_value = 0;
		if (current_value > 100) current_value = 100;

		int fill_w = (int)(bar_w * current_value / 100.0f);
		if (fill_w > 0) {
			SDL_Rect fill_rect = {bar_x, bar_y, fill_w, bar_h};
			SDL_FillRect(screen, &fill_rect, fill_color);
		}
	}

	GFX_present(NULL);
}
