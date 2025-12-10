/**
 * api_fakes.c - Platform API function fake implementations
 *
 * Defines the actual fake function instances using fff macros.
 * This file provides mockable versions of GFX_*, PAD_*, PWR_*, VIB_*
 * functions for unit testing.
 */

#include "api_fakes.h"

#include <string.h>

///////////////////////////////
// Global UI State
///////////////////////////////

// Font set - NULL pointers are fine for testing
FontSet font = {
    .large = NULL,
    .small = NULL,
    .tiny = NULL,
};

// UI layout - reasonable defaults for 640x480
UILayout ui = {
    .screen_width = 640,
    .screen_height = 480,
    .edge_padding = 8,
    .button_padding = 8,
    .padding = 4,
    .pill_height = 32,
    .text_baseline = 8, // DEPRECATED
    .button_size = 20,
    .option_size = 24,
    .option_baseline = 2, // DEPRECATED
    .option_value_baseline = 4, // DEPRECATED
    .text_offset_px = 12, // Typical value for 32dp pill with font.large
    .option_offset_px = 8, // Typical value for 24dp option with font.medium
    .option_value_offset_px = 6, // Typical value for 24dp option with font.small
    .button_text_offset_px = 6, // Typical value for button hints
    .button_label_offset_px = 5, // Typical value for button labels
};

///////////////////////////////
// GFX Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(SDL_Surface*, GFX_resize, int, int, int);
DEFINE_FAKE_VOID_FUNC(GFX_setEffect, int);
DEFINE_FAKE_VOID_FUNC(GFX_startFrame);
DEFINE_FAKE_VOID_FUNC(GFX_clear, SDL_Surface*);
DEFINE_FAKE_VOID_FUNC(GFX_clearAll);
DEFINE_FAKE_VOID_FUNC(GFX_flip, SDL_Surface*);
DEFINE_FAKE_VOID_FUNC(GFX_sync);

DEFINE_FAKE_VALUE_FUNC(int, GFX_blitHardwareGroup, SDL_Surface*, int);
DEFINE_FAKE_VALUE_FUNC(int, GFX_truncateText, TTF_Font*, const char*, char*, int, int);
DEFINE_FAKE_VOID_FUNC(GFX_blitPill, int, SDL_Surface*, SDL_Rect*);
DEFINE_FAKE_VOID_FUNC(GFX_blitHardwareHints, SDL_Surface*, int);
DEFINE_FAKE_VOID_FUNC(GFX_blitButtonGroup, char**, int, SDL_Surface*, int);
DEFINE_FAKE_VOID_FUNC(GFX_blitRect, int, SDL_Surface*, SDL_Rect*);
DEFINE_FAKE_VOID_FUNC(GFX_blitMessage, TTF_Font*, const char*, SDL_Surface*, SDL_Rect*);
DEFINE_FAKE_VOID_FUNC(GFX_blitAsset, int, SDL_Rect*, SDL_Surface*, SDL_Rect*);

///////////////////////////////
// PAD Fake Definitions
///////////////////////////////

DEFINE_FAKE_VOID_FUNC(PAD_reset);
DEFINE_FAKE_VOID_FUNC(PAD_poll);
DEFINE_FAKE_VALUE_FUNC(int, PAD_justPressed, int);
DEFINE_FAKE_VALUE_FUNC(int, PAD_tappedMenu, Uint32);

///////////////////////////////
// PWR Fake Definitions
///////////////////////////////

DEFINE_FAKE_VOID_FUNC(PWR_setCPUSpeed, int);
DEFINE_FAKE_VOID_FUNC(PWR_warn, int);
DEFINE_FAKE_VOID_FUNC(PWR_enableSleep);
DEFINE_FAKE_VOID_FUNC(PWR_disableSleep);
DEFINE_FAKE_VOID_FUNC(PWR_enableAutosleep);
DEFINE_FAKE_VOID_FUNC(PWR_disableAutosleep);
DEFINE_FAKE_VOID_FUNC(PWR_powerOff);
DEFINE_FAKE_VOID_FUNC(PWR_update, int*, int*, void (*)(void), void (*)(void));

///////////////////////////////
// VIB Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(int, VIB_getStrength);
DEFINE_FAKE_VOID_FUNC(VIB_setStrength, int);

///////////////////////////////
// Reset All Fakes
///////////////////////////////

void reset_all_api_fakes(void) {
	// GFX
	RESET_FAKE(GFX_resize);
	RESET_FAKE(GFX_setEffect);
	RESET_FAKE(GFX_startFrame);
	RESET_FAKE(GFX_clear);
	RESET_FAKE(GFX_clearAll);
	RESET_FAKE(GFX_flip);
	RESET_FAKE(GFX_sync);
	RESET_FAKE(GFX_blitHardwareGroup);
	RESET_FAKE(GFX_truncateText);
	RESET_FAKE(GFX_blitPill);
	RESET_FAKE(GFX_blitHardwareHints);
	RESET_FAKE(GFX_blitButtonGroup);
	RESET_FAKE(GFX_blitRect);
	RESET_FAKE(GFX_blitMessage);
	RESET_FAKE(GFX_blitAsset);

	// PAD
	RESET_FAKE(PAD_reset);
	RESET_FAKE(PAD_poll);
	RESET_FAKE(PAD_justPressed);
	RESET_FAKE(PAD_tappedMenu);

	// PWR
	RESET_FAKE(PWR_setCPUSpeed);
	RESET_FAKE(PWR_warn);
	RESET_FAKE(PWR_enableSleep);
	RESET_FAKE(PWR_disableSleep);
	RESET_FAKE(PWR_enableAutosleep);
	RESET_FAKE(PWR_disableAutosleep);
	RESET_FAKE(PWR_powerOff);
	RESET_FAKE(PWR_update);

	// VIB
	RESET_FAKE(VIB_getStrength);
	RESET_FAKE(VIB_setStrength);
}
