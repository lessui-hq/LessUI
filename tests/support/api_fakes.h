/**
 * api_fakes.h - Platform API function fakes using fff framework
 *
 * Declares fake platform API functions for unit testing. Uses the Fake Function
 * Framework (fff) to create mockable versions of GFX_*, PAD_*, PWR_*, VIB_*
 * functions from api.c.
 *
 * Usage:
 *   #include "api_fakes.h"
 *   RESET_ALL_API_FAKES();  // In setUp()
 *   PAD_justPressed_fake.return_val = 1;  // Configure mock behavior
 */

#ifndef API_FAKES_H
#define API_FAKES_H

#include "fff/fff.h"
#include "sdl_stubs.h"

///////////////////////////////
// Font & UI Types
///////////////////////////////

/**
 * Font set structure - matches api.h
 */
typedef struct {
	TTF_Font* large;
	TTF_Font* small;
	TTF_Font* tiny;
} FontSet;

/**
 * UI layout structure - matches api.h
 */
typedef struct {
	int screen_width;
	int screen_height;
	int edge_padding;
	int button_padding;
	int padding;
	int pill_height;
	int text_baseline; // DEPRECATED: use text_offset_px
	int button_size;
	int option_size;
	int option_baseline; // DEPRECATED: use option_offset_px
	int option_value_baseline; // DEPRECATED: use option_value_offset_px
	int text_offset_px; // Y offset in pixels to center font.large in pill_height
	int option_offset_px; // Y offset in pixels to center font.medium in option_size
	int option_value_offset_px; // Y offset in pixels to center font.small in option_size
	int button_text_offset_px; // Y offset in pixels to center font.small in button_size
	int button_label_offset_px; // Y offset in pixels to center font.tiny in button_size
} UILayout;

/**
 * GFX_Renderer structure - matches api.h (simplified for testing)
 */
typedef struct {
	void* src;       // Source pixel buffer
	int true_w;      // True source width
	int true_h;      // True source height
	int src_w;       // Cropped source width
	int src_h;       // Cropped source height
	int src_x;       // Crop offset X
	int src_y;       // Crop offset Y
	int src_p;       // Source pitch
	int dst_x;       // Destination offset X
	int dst_y;       // Destination offset Y
	int scale;       // Scale factor
} GFX_Renderer;

// Global font and UI layout (defined in api_fakes.c)
extern FontSet font;
extern UILayout ui;

// DP() macro - density-independent pixels (1:1 for testing)
#define DP(x) (x)

///////////////////////////////
// Asset IDs (from api.h)
///////////////////////////////

enum {
	ASSET_BLACK_PILL,
	ASSET_WHITE_PILL,
	ASSET_DARK_GRAY_PILL,
	ASSET_STATE_BG,
	ASSET_PAGE,
	ASSET_DOT,
	// Add more as needed
};

///////////////////////////////
// Effect Types (from api.h)
///////////////////////////////

enum {
	EFFECT_NONE = 0,
	EFFECT_SCANLINE,
	EFFECT_GRID,
	// Add more as needed
};

///////////////////////////////
// CPU Speed Constants
///////////////////////////////

enum {
	CPU_SPEED_IDLE = 0,
	CPU_SPEED_NORMAL,
	CPU_SPEED_PERFORMANCE,
};

///////////////////////////////
// Button Constants
///////////////////////////////

enum {
	BTN_UP = 0,
	BTN_DOWN,
	BTN_LEFT,
	BTN_RIGHT,
	BTN_A,
	BTN_B,
	BTN_X,
	BTN_Y,
	BTN_L1,
	BTN_R1,
	BTN_L2,
	BTN_R2,
	BTN_SELECT,
	BTN_START,
	BTN_MENU,
	BTN_POWER,
	BTN_NONE = -1,
};

// Button wake/sleep/menu aliases
#define BTN_WAKE BTN_MENU
#define BTN_SLEEP BTN_POWER

///////////////////////////////
// GFX Function Fakes
///////////////////////////////

DECLARE_FAKE_VALUE_FUNC(SDL_Surface*, GFX_resize, int, int, int);
DECLARE_FAKE_VOID_FUNC(GFX_setEffect, int);
DECLARE_FAKE_VOID_FUNC(GFX_startFrame);
DECLARE_FAKE_VOID_FUNC(GFX_clear, SDL_Surface*);
DECLARE_FAKE_VOID_FUNC(GFX_clearAll);
DECLARE_FAKE_VOID_FUNC(GFX_present, GFX_Renderer*);
DECLARE_FAKE_VOID_FUNC(GFX_sync);

DECLARE_FAKE_VALUE_FUNC(int, GFX_blitHardwareGroup, SDL_Surface*, int);
DECLARE_FAKE_VALUE_FUNC(int, GFX_truncateText, TTF_Font*, const char*, char*, int, int);
DECLARE_FAKE_VOID_FUNC(GFX_blitPill, int, SDL_Surface*, SDL_Rect*);
DECLARE_FAKE_VOID_FUNC(GFX_blitHardwareHints, SDL_Surface*, int);
DECLARE_FAKE_VOID_FUNC(GFX_blitButtonGroup, char**, int, SDL_Surface*, int);
DECLARE_FAKE_VOID_FUNC(GFX_blitRect, int, SDL_Surface*, SDL_Rect*);
DECLARE_FAKE_VOID_FUNC(GFX_blitMessage, TTF_Font*, const char*, SDL_Surface*, SDL_Rect*);
DECLARE_FAKE_VOID_FUNC(GFX_blitAsset, int, SDL_Rect*, SDL_Surface*, SDL_Rect*);

///////////////////////////////
// PAD Function Fakes
///////////////////////////////

DECLARE_FAKE_VOID_FUNC(PAD_reset);
DECLARE_FAKE_VOID_FUNC(PAD_poll);
DECLARE_FAKE_VALUE_FUNC(int, PAD_justPressed, int);
DECLARE_FAKE_VALUE_FUNC(int, PAD_tappedMenu, Uint32);

///////////////////////////////
// PWR Function Fakes
///////////////////////////////

DECLARE_FAKE_VOID_FUNC(PWR_setCPUSpeed, int);
DECLARE_FAKE_VOID_FUNC(PWR_warn, int);
DECLARE_FAKE_VOID_FUNC(PWR_enableSleep);
DECLARE_FAKE_VOID_FUNC(PWR_disableSleep);
DECLARE_FAKE_VOID_FUNC(PWR_enableAutosleep);
DECLARE_FAKE_VOID_FUNC(PWR_disableAutosleep);
DECLARE_FAKE_VOID_FUNC(PWR_powerOff);
DECLARE_FAKE_VOID_FUNC(PWR_update, int*, int*, void (*)(void), void (*)(void));

///////////////////////////////
// VIB Function Fakes
///////////////////////////////

DECLARE_FAKE_VALUE_FUNC(int, VIB_getStrength);
DECLARE_FAKE_VOID_FUNC(VIB_setStrength, int);

///////////////////////////////
// Convenience Reset
///////////////////////////////

/**
 * Reset all API fakes - call in setUp()
 */
void reset_all_api_fakes(void);

#define RESET_ALL_API_FAKES() reset_all_api_fakes()

#endif // API_FAKES_H
