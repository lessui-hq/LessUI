#ifndef SHELLUI_FONTS_H
#define SHELLUI_FONTS_H

#ifdef PLATFORM
#include "api.h"

/**
 * Shared fonts for shellui UI modules.
 *
 * These are initialized once by the daemon and shared across
 * ui_message, ui_list, and ui_keyboard modules.
 */
extern TTF_Font* g_font_large;
extern TTF_Font* g_font_small;

/**
 * Initialize shared fonts. Call once at daemon startup.
 * Safe to call multiple times (no-op if already initialized).
 */
void fonts_init(void);

/**
 * Cleanup shared fonts. Call once at daemon shutdown.
 */
void fonts_cleanup(void);

#endif // PLATFORM

#endif // SHELLUI_FONTS_H
