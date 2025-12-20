#ifdef PLATFORM

#include "fonts.h"
#include "defines.h"

TTF_Font* g_font_large = NULL;
TTF_Font* g_font_small = NULL;

void fonts_init(void) {
	if (g_font_large) return;

	g_font_large = TTF_OpenFont(FONT_PATH, DP(FONT_LARGE));
	if (g_font_large) {
		TTF_SetFontStyle(g_font_large, TTF_STYLE_BOLD);
	}

	g_font_small = TTF_OpenFont(FONT_PATH, DP(FONT_SMALL));
}

void fonts_cleanup(void) {
	if (g_font_large) {
		TTF_CloseFont(g_font_large);
		g_font_large = NULL;
	}
	if (g_font_small) {
		TTF_CloseFont(g_font_small);
		g_font_small = NULL;
	}
}

#endif // PLATFORM
