/**
 * api.c - Platform abstraction layer implementation for MinUI
 *
 * Provides cross-platform API functions for graphics (GFX_*), sound (SND_*),
 * input (PAD_*), power management (PWR_*), and vibration (VIB_*). This file
 * implements the common layer that works on all devices, while platform-specific
 * implementations are provided through PLAT_* functions defined in each platform's
 * directory.
 *
 * Key components:
 * - Graphics: SDL-based rendering, asset management, text rendering, UI helpers
 * - Sound: Audio mixing, resampling, ring buffer management
 * - Input: Button state tracking, repeat handling, analog stick support
 * - Power: Battery monitoring, sleep/wake, brightness/volume control
 * - Vibration: Rumble motor control with deferred state changes
 *
 * Memory Management:
 * - SDL surfaces are reference counted (use SDL_FreeSurface when done)
 * - Audio buffer is dynamically allocated and resized as needed
 * - Asset textures loaded once at init, freed at quit
 * - Font resources managed through TTF_CloseFont
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <msettings.h>

#include "api.h"
// NOLINTNEXTLINE(bugprone-suspicious-include) - Intentionally bundled to avoid makefile changes
#include "audio_resampler.c"
#include "defines.h"
#include "gfx_text.h"
#include "pad.h"
#include "utils.h"

///////////////////////////////
// Graphics - Core initialization and state
///////////////////////////////

// Pre-mapped RGB color values for the current display format
// These are initialized in GFX_init() based on the screen's pixel format
uint32_t RGB_WHITE;
uint32_t RGB_BLACK;
uint32_t RGB_LIGHT_GRAY;
uint32_t RGB_GRAY;
uint32_t RGB_DARK_GRAY;

///////////////////////////////
// Display Points (DP) scaling system
///////////////////////////////

// Global display scale factor (PPI / 144)
float gfx_dp_scale = 2.0f; // Default to 2x until UI_initLayout is called

// Runtime-calculated UI layout parameters
UI_Layout ui = {
    .pill_height = 30,
    .row_count = 6,
    .padding = 10,
    .edge_padding = 10,
    .text_baseline = 6, // (30 * 2) / 10 = 6 for 30dp pill
    .button_size = 20, // (30 * 2) / 3 = 20 for 30dp pill (button icons)
    .button_margin = 5,
    .option_size = 22, // (30 * 3) / 4 = 22 for 30dp pill (submenu rows)
    .option_baseline = 2, // (22 * 1) / 10 = 2 for 22dp option (font.medium)
    .option_value_baseline = 4, // ~18% - slightly lower for smaller font.small
    .button_padding = 12,
};

/**
 * Initializes the resolution-independent UI scaling system.
 *
 * Calculates dp_scale from screen PPI, snaps to favorable ratios for clean
 * asset scaling, then determines optimal pill height to fill the screen.
 *
 * DP Scale Calculation:
 *   1. Calculate PPI: sqrt(width² + height²) / diagonal_inches
 *   2. Calculate raw dp_scale: ppi / 144.0 (handheld gaming device baseline)
 *   3. Apply optional SCALE_MODIFIER if defined in platform.h
 *
 * Row Fitting Algorithm:
 *   - Try 8→6 rows (prefer more content)
 *   - For each row count, calculate pill height to fill available space
 *   - Select first configuration where pill fits 28-32dp range
 *   - Adjust pill_height to produce even physical pixels (for centering)
 *
 * @param screen_width Screen width in physical pixels
 * @param screen_height Screen height in physical pixels
 * @param diagonal_inches Physical screen diagonal in inches (from platform.h)
 *
 * @note Sets global gfx_dp_scale and ui struct values
 * @note Must be called before any DP() macro usage
 */
void UI_initLayout(int screen_width, int screen_height, float diagonal_inches) {
	// Calculate PPI and dp_scale
	float diagonal_px = sqrtf((float)(screen_width * screen_width + screen_height * screen_height));
	float ppi = diagonal_px / diagonal_inches;
	float raw_dp_scale = ppi / 120.0f;

	// Apply platform scale modifier if defined
#ifdef SCALE_MODIFIER
	raw_dp_scale *= SCALE_MODIFIER;
#endif

	// Use the calculated dp_scale directly (no snapping to preserve PPI accuracy)
	// Asset-level even-pixel adjustments handle rounding where needed
	gfx_dp_scale = raw_dp_scale;

	// Layout calculation: treat everything as uniform rows
	// Screen layout: top_padding + content_rows + footer_row + bottom_padding
	// All rows (content + footer) use the same pill_height for visual consistency
	const int MIN_PILL = 28;
	const int MAX_PILL = 32;

	// Internal padding between UI elements (always 10dp)
	const int internal_padding = 10;

	// Edge padding: distance from screen edges
	// EDGE_PADDING allows smaller values on devices where bezel provides visual margin
#ifdef EDGE_PADDING
	const int edge_padding = EDGE_PADDING;
#else
	const int edge_padding = internal_padding;
#endif

	int screen_height_dp = (int)(screen_height / gfx_dp_scale + 0.5f);
	int available_dp = screen_height_dp - (edge_padding * 2);

	// Calculate maximum possible rows (no arbitrary limit)
	int max_possible_rows = (available_dp / MIN_PILL) - 1; // -1 for footer
	if (max_possible_rows < 1)
		max_possible_rows = 1;

	// Try different row counts, starting from maximum (prefer more content)
	// Prefer even pixels but accept odd if needed
	int best_pill = 0;
	int best_rows = 0;
	int best_is_even = 0;

	for (int content_rows = max_possible_rows; content_rows >= 1; content_rows--) {
		int total_rows = content_rows + 1; // +1 for footer
		int pill = available_dp / total_rows;

		// Early exit: pills only get larger as rows decrease
		if (pill > MAX_PILL)
			break;

		int pill_px = DP(pill);
		int is_even = (pill_px % 2 == 0);

		if (pill >= MIN_PILL) {
			if (is_even) {
				// Perfect: in range AND even pixels
				best_pill = pill;
				best_rows = content_rows;
				best_is_even = 1;
				LOG_info("Row calc: %d rows → pill=%ddp (%dpx even) ✓\n", content_rows, pill,
				         pill_px);
				break;
			} else if (best_rows == 0) {
				// Acceptable but odd pixels - keep as backup
				best_pill = pill;
				best_rows = content_rows;
				LOG_info("Row calc: %d rows → pill=%ddp (%dpx odd) - backup\n", content_rows, pill,
				         pill_px);
			}
		}
	}

	if (best_is_even) {
		LOG_info("Row calc: Using even-pixel configuration\n");
	} else if (best_rows > 0) {
		LOG_info("Row calc: Using odd-pixel fallback (no even option in range)\n");
	} else {
		// Emergency fallback (should never happen with reasonable MIN_PILL)
		best_pill = MIN_PILL;
		best_rows = 1;
		LOG_info("Row calc: EMERGENCY FALLBACK to %ddp, 1 row\n", MIN_PILL);
	}

	ui.screen_width = (int)(screen_width / gfx_dp_scale + 0.5f);
	ui.screen_height = screen_height_dp;
	ui.screen_width_px = screen_width;
	ui.screen_height_px = screen_height;
	ui.pill_height = best_pill;
	ui.row_count = best_rows;
	ui.padding = internal_padding;
	ui.edge_padding = edge_padding;

	int used_dp = (ui.row_count + 1) * ui.pill_height;
	(void)used_dp; // Used in LOG_info below
	LOG_info("Row calc: FINAL rows=%d, pill=%ddp (%dpx), using %d/%d dp (%.1f%%)\n", ui.row_count,
	         ui.pill_height, DP(ui.pill_height), used_dp, available_dp,
	         (used_dp * 100.0f) / available_dp);

	// Derived proportional sizes (also ensure even pixels where needed)
	ui.button_size = (ui.pill_height * 2) / 3; // ~20 for 30dp pill (button icons)
	int button_px = DP(ui.button_size);
	if (button_px % 2 != 0)
		ui.button_size++; // Buttons look better even

	ui.button_margin = (ui.pill_height - ui.button_size) / 2; // Center button in pill
	ui.button_padding = (ui.pill_height * 2) / 5; // ~12 for 30dp pill

	// Submenu option rows - larger than button icons
	ui.option_size = (ui.pill_height * 3) / 4; // ~22 for 30dp pill
	int option_px = DP(ui.option_size);
	if (option_px % 2 != 0)
		ui.option_size++; // Options look better even

	// Text baseline offset - positions text slightly above center to account for
	// visual weight of font glyphs (most text sits above baseline, descenders are rare)
	// Gives ~6dp for 30dp pill, scales proportionally with pill height
	ui.text_baseline = (ui.pill_height * 2) / 10;
	// Option baselines for submenu rows (smaller fonts need different positioning)
	ui.option_baseline = (ui.option_size * 10 + 5) / 100; // ~10% with rounding (2dp for 22dp)
	ui.option_value_baseline = (ui.option_size * 18 + 5) / 100; // ~18% with rounding (4dp for 22dp)

	// Settings indicators
	ui.settings_size = ui.pill_height / 8; // ~4dp for 30dp pill
	ui.settings_width = 80; // Fixed width in dp

	LOG_info("UI_initLayout: %dx%d @ %.2f\" → PPI=%.0f, dp_scale=%.2f\n", screen_width,
	         screen_height, diagonal_inches, ppi, gfx_dp_scale);
	LOG_info("UI_initLayout: pill=%ddp, rows=%d, padding=%ddp, edge_padding=%ddp\n", ui.pill_height,
	         ui.row_count, ui.padding, ui.edge_padding);
}

static struct GFX_Context {
	SDL_Surface* screen;
	SDL_Surface* assets;

	int mode;
	int vsync;
} gfx;

static SDL_Rect asset_rects[ASSET_COUNT];
static uint32_t asset_rgbs[ASSET_COLORS];
GFX_Fonts font;

///////////////////////////////

static struct PWR_Context {
	int initialized;

	int can_sleep;
	int can_poweroff;
	int can_autosleep;
	int requested_sleep;
	int requested_wake;

	pthread_t battery_pt;
	int is_charging;
	int charge;
	int should_warn;

	SDL_Surface* overlay;
} pwr = {0};

///////////////////////////////

// Unused variable for discarding return values
static int _;

/**
 * Scales a surface using bilinear interpolation.
 *
 * Performs smooth downscaling/upscaling of RGBA8888 or RGB888 surfaces
 * using bilinear filtering. Each output pixel samples the 4 nearest source
 * pixels and interpolates linearly in both X and Y directions.
 *
 * Used for scaling UI assets (icons, pills, buttons) from @1x/@2x/@3x/@4x
 * tiers to the exact dp_scale required by the current screen. Provides
 * smooth edges compared to nearest-neighbor scaling.
 *
 * @param src Source surface to scale
 * @param dst Destination surface (must be pre-allocated at target size)
 *
 * @note Both surfaces must be locked if SDL_MUSTLOCK is true
 * @note Works with 3-byte (RGB888) and 4-byte (RGBA8888) pixel formats
 * @note dst dimensions determine the output scale
 */
static void GFX_scaleBilinear(SDL_Surface* src, SDL_Surface* dst) {
	if (!src || !dst)
		return;

	int src_w = src->w;
	int src_h = src->h;
	int dst_w = dst->w;
	int dst_h = dst->h;

	// Calculate scale ratios (how much to step through source for each dest pixel)
	float x_ratio = ((float)src_w) / dst_w;
	float y_ratio = ((float)src_h) / dst_h;

	// Determine bytes per pixel (3 for RGB, 4 for RGBA)
	int bpp = src->format->BytesPerPixel;

	// Lock surfaces if needed
	if (SDL_MUSTLOCK(src))
		SDL_LockSurface(src);
	if (SDL_MUSTLOCK(dst))
		SDL_LockSurface(dst);

	uint8_t* src_pixels = (uint8_t*)src->pixels;
	uint8_t* dst_pixels = (uint8_t*)dst->pixels;

	for (int y = 0; y < dst_h; y++) {
		for (int x = 0; x < dst_w; x++) {
			// Source coordinates (floating point)
			float src_x = x * x_ratio;
			float src_y = y * y_ratio;

			// Integer coordinates of surrounding pixels
			int x1 = (int)src_x;
			int y1 = (int)src_y;
			int x2 = (x1 + 1 < src_w) ? x1 + 1 : x1;
			int y2 = (y1 + 1 < src_h) ? y1 + 1 : y1;

			// Fractional parts for interpolation
			float x_frac = src_x - x1;
			float y_frac = src_y - y1;

			// Get pointers to the 4 surrounding pixels
			uint8_t* p11 = src_pixels + (y1 * src->pitch) + (x1 * bpp);
			uint8_t* p12 = src_pixels + (y1 * src->pitch) + (x2 * bpp);
			uint8_t* p21 = src_pixels + (y2 * src->pitch) + (x1 * bpp);
			uint8_t* p22 = src_pixels + (y2 * src->pitch) + (x2 * bpp);

			// Bilinear interpolation for each channel
			uint8_t* dst_pixel = dst_pixels + (y * dst->pitch) + (x * bpp);

			for (int c = 0; c < bpp; c++) {
				// Interpolate horizontally on top row
				float top = p11[c] * (1.0f - x_frac) + p12[c] * x_frac;
				// Interpolate horizontally on bottom row
				float bottom = p21[c] * (1.0f - x_frac) + p22[c] * x_frac;
				// Interpolate vertically
				float result = top * (1.0f - y_frac) + bottom * y_frac;
				dst_pixel[c] = (uint8_t)(result + 0.5f);
			}
		}
	}

	// Unlock surfaces
	if (SDL_MUSTLOCK(dst))
		SDL_UnlockSurface(dst);
	if (SDL_MUSTLOCK(src))
		SDL_UnlockSurface(src);
}

/**
 * Scales an image to fit within maximum dimensions while preserving aspect ratio.
 *
 * If the source image is larger than max_w or max_h, scales it down proportionally
 * based on the longer side. If the image already fits, returns a reference to the
 * original surface (caller must not free).
 *
 * @param src Source surface to scale
 * @param max_w Maximum width in pixels
 * @param max_h Maximum height in pixels
 * @return New scaled surface (caller must SDL_FreeSurface), or src if no scaling needed
 */
SDL_Surface* GFX_scaleToFit(SDL_Surface* src, int max_w, int max_h) {
	if (!src)
		return NULL;

	// Check if scaling is needed
	if (src->w <= max_w && src->h <= max_h)
		return src; // No scaling needed, return original

	// Calculate scale factor based on longest side
	float scale_w = (float)max_w / src->w;
	float scale_h = (float)max_h / src->h;
	float scale =
	    (scale_w < scale_h) ? scale_w : scale_h; // Use smaller scale (fits both dimensions)

	// Calculate new dimensions
	int new_w = (int)(src->w * scale + 0.5f);
	int new_h = (int)(src->h * scale + 0.5f);

	// Create destination surface with same format as source
	SDL_Surface* dst =
	    SDL_CreateRGBSurface(0, new_w, new_h, src->format->BitsPerPixel, src->format->Rmask,
	                         src->format->Gmask, src->format->Bmask, src->format->Amask);
	if (!dst)
		return src; // Allocation failed, return original

	// Perform bilinear scaling
	GFX_scaleBilinear(src, dst);

	return dst;
}

/**
 * Initializes the graphics subsystem.
 *
 * Sets up SDL video, initializes the DP scaling system, loads and scales
 * UI assets, initializes fonts, and prepares the color palette.
 *
 * Asset Loading and Scaling:
 * - Selects asset tier (@1x, @2x, @3x, @4x) based on ceil(dp_scale)
 * - If exact match: uses assets as-is (e.g., dp_scale=2.0 with @2x)
 * - If fractional: extracts and individually scales each asset using bilinear filtering
 *   - Pill caps → exactly pill_px (for pixel-perfect GFX_blitPill rendering)
 *   - Buttons/holes → exactly button_px
 *   - Icons (brightness, volume, wifi) → proportional with even-pixel centering
 *   - Battery assets → proportional scaling (internal offsets handled in GFX_blitBattery)
 * - Defines rectangles for each asset sprite in the texture atlas
 * - Maps asset IDs to RGB color values for fills
 *
 * Font Initialization:
 * - Opens 4 font sizes (large, medium, small, tiny)
 * - Applies bold style to all fonts
 * - Font sizes scaled based on DP() macro (resolution-independent)
 *
 * @param mode Display mode (MODE_MAIN for launcher, MODE_MENU for in-game)
 * @return Pointer to main SDL screen surface
 *
 * @note Also calls PLAT_initLid() to set up lid detection hardware
 */
SDL_Surface* GFX_init(int mode) {
	// TODO: this doesn't really belong here...
	// tried adding to PWR_init() but that was no good (not sure why)
	PLAT_initLid();

	gfx.screen = PLAT_initVideo();
	gfx.vsync = VSYNC_STRICT;
	gfx.mode = mode;

	// Initialize DP scaling system
#ifdef SCREEN_DIAGONAL
	UI_initLayout(gfx.screen->w, gfx.screen->h, SCREEN_DIAGONAL);
#endif

	RGB_WHITE = SDL_MapRGB(gfx.screen->format, TRIAD_WHITE);
	RGB_BLACK = SDL_MapRGB(gfx.screen->format, TRIAD_BLACK);
	RGB_LIGHT_GRAY = SDL_MapRGB(gfx.screen->format, TRIAD_LIGHT_GRAY);
	RGB_GRAY = SDL_MapRGB(gfx.screen->format, TRIAD_GRAY);
	RGB_DARK_GRAY = SDL_MapRGB(gfx.screen->format, TRIAD_DARK_GRAY);

	asset_rgbs[ASSET_WHITE_PILL] = RGB_WHITE;
	asset_rgbs[ASSET_BLACK_PILL] = RGB_BLACK;
	asset_rgbs[ASSET_DARK_GRAY_PILL] = RGB_DARK_GRAY;
	asset_rgbs[ASSET_OPTION] = RGB_DARK_GRAY;
	asset_rgbs[ASSET_OPTION_WHITE] = RGB_WHITE;
	asset_rgbs[ASSET_BUTTON] = RGB_WHITE;
	asset_rgbs[ASSET_PAGE_BG] = RGB_WHITE;
	asset_rgbs[ASSET_STATE_BG] = RGB_WHITE;
	asset_rgbs[ASSET_PAGE] = RGB_BLACK;
	asset_rgbs[ASSET_BAR] = RGB_WHITE;
	asset_rgbs[ASSET_BAR_BG] = RGB_BLACK;
	asset_rgbs[ASSET_BAR_BG_MENU] = RGB_DARK_GRAY;
	asset_rgbs[ASSET_UNDERLINE] = RGB_GRAY;
	asset_rgbs[ASSET_DOT] = RGB_LIGHT_GRAY;
	asset_rgbs[ASSET_HOLE] = RGB_BLACK;

	// Select asset tier based on dp_scale (use one tier higher for better quality)
	// Available tiers: @1x, @2x, @3x, @4x
	// Always downscale from a higher resolution for smoother antialiasing
	int asset_scale = (int)ceilf(gfx_dp_scale) + 1;
	if (asset_scale < 1)
		asset_scale = 1;
	if (asset_scale > 4)
		asset_scale = 4;

	// Load asset sprite sheet at selected tier
	char asset_path[MAX_PATH];
	sprintf(asset_path, RES_PATH "/assets@%ix.png", asset_scale);
	if (!exists(asset_path))
		LOG_info("missing assets, you're about to segfault dummy!\n");
	SDL_Surface* loaded_assets = IMG_Load(asset_path);

	// Define asset rectangles in the loaded sprite sheet (at asset_scale)
	// Base coordinates are @1x, multiply by asset_scale for actual position
#define ASSET_SCALE4(x, y, w, h)                                                                   \
	((x) * asset_scale), ((y) * asset_scale), ((w) * asset_scale), ((h) * asset_scale)

	asset_rects[ASSET_WHITE_PILL] = (SDL_Rect){ASSET_SCALE4(1, 1, 30, 30)};
	asset_rects[ASSET_BLACK_PILL] = (SDL_Rect){ASSET_SCALE4(33, 1, 30, 30)};
	asset_rects[ASSET_DARK_GRAY_PILL] = (SDL_Rect){ASSET_SCALE4(65, 1, 30, 30)};
	asset_rects[ASSET_OPTION] = (SDL_Rect){ASSET_SCALE4(97, 1, 20, 20)};
	asset_rects[ASSET_OPTION_WHITE] = (SDL_Rect){ASSET_SCALE4(1, 33, 20, 20)}; // Same as BUTTON src
	asset_rects[ASSET_BUTTON] = (SDL_Rect){ASSET_SCALE4(1, 33, 20, 20)};
	asset_rects[ASSET_PAGE_BG] = (SDL_Rect){ASSET_SCALE4(64, 33, 15, 15)};
	asset_rects[ASSET_STATE_BG] = (SDL_Rect){ASSET_SCALE4(23, 54, 8, 8)};
	asset_rects[ASSET_PAGE] = (SDL_Rect){ASSET_SCALE4(39, 54, 6, 6)};
	asset_rects[ASSET_BAR] = (SDL_Rect){ASSET_SCALE4(33, 58, 4, 4)};
	asset_rects[ASSET_BAR_BG] = (SDL_Rect){ASSET_SCALE4(15, 55, 4, 4)};
	asset_rects[ASSET_BAR_BG_MENU] = (SDL_Rect){ASSET_SCALE4(85, 56, 4, 4)};
	asset_rects[ASSET_UNDERLINE] = (SDL_Rect){ASSET_SCALE4(85, 51, 3, 3)};
	asset_rects[ASSET_DOT] = (SDL_Rect){ASSET_SCALE4(33, 54, 2, 2)};
	asset_rects[ASSET_BRIGHTNESS] = (SDL_Rect){ASSET_SCALE4(23, 33, 19, 19)};
	asset_rects[ASSET_VOLUME_MUTE] = (SDL_Rect){ASSET_SCALE4(44, 33, 10, 16)};
	asset_rects[ASSET_VOLUME] = (SDL_Rect){ASSET_SCALE4(44, 33, 18, 16)};
	asset_rects[ASSET_BATTERY] = (SDL_Rect){ASSET_SCALE4(47, 51, 17, 10)};
	asset_rects[ASSET_BATTERY_LOW] = (SDL_Rect){ASSET_SCALE4(66, 51, 17, 10)};
	asset_rects[ASSET_BATTERY_FILL] = (SDL_Rect){ASSET_SCALE4(81, 33, 12, 6)};
	asset_rects[ASSET_BATTERY_FILL_LOW] = (SDL_Rect){ASSET_SCALE4(1, 55, 12, 6)};
	asset_rects[ASSET_BATTERY_BOLT] = (SDL_Rect){ASSET_SCALE4(81, 41, 12, 6)};
	asset_rects[ASSET_SCROLL_UP] = (SDL_Rect){ASSET_SCALE4(97, 23, 24, 6)};
	asset_rects[ASSET_SCROLL_DOWN] = (SDL_Rect){ASSET_SCALE4(97, 31, 24, 6)};
	asset_rects[ASSET_WIFI] = (SDL_Rect){ASSET_SCALE4(95, 39, 14, 10)};
	asset_rects[ASSET_HOLE] = (SDL_Rect){ASSET_SCALE4(1, 63, 20, 20)};

	// Calculate target pixel sizes for UI elements
	float scale_ratio = gfx_dp_scale / (float)asset_scale;
	int pill_px = DP(ui.pill_height);
	int button_px = DP(ui.button_size);
	int option_px = DP(ui.option_size);

	// Asset scaling categories:
	// - SCALE_PILL: Scale to pill_px (main menu pills)
	// - SCALE_BUTTON: Scale to button_px (button icons)
	// - SCALE_OPTION: Scale to option_px, placed in virtual area (submenu option rows)
	// - SCALE_PROPORTIONAL: Scale proportionally with dp_scale
	// - SCALE_CENTERED: Scale proportionally, ensure even offset from pill_px for centering
	enum { SCALE_PROPORTIONAL, SCALE_PILL, SCALE_BUTTON, SCALE_OPTION, SCALE_CENTERED };

	// Define scaling behavior for each asset
	// Assets with SCALE_OPTION need virtual area (they're scaled differently than source)
	int asset_scale_type[ASSET_COUNT];
	for (int i = 0; i < ASSET_COUNT; i++)
		asset_scale_type[i] = SCALE_PROPORTIONAL; // Default

	// Pills scale to pill_px
	asset_scale_type[ASSET_WHITE_PILL] = SCALE_PILL;
	asset_scale_type[ASSET_BLACK_PILL] = SCALE_PILL;
	asset_scale_type[ASSET_DARK_GRAY_PILL] = SCALE_PILL;

	// Button icons scale to button_px
	asset_scale_type[ASSET_BUTTON] = SCALE_BUTTON;
	asset_scale_type[ASSET_HOLE] = SCALE_BUTTON;

	// Option pills scale to option_px (virtual assets - need their own space)
	asset_scale_type[ASSET_OPTION] = SCALE_OPTION;
	asset_scale_type[ASSET_OPTION_WHITE] = SCALE_OPTION;

	// Icons that get centered in pills need even pixel difference
	asset_scale_type[ASSET_BRIGHTNESS] = SCALE_CENTERED;
	asset_scale_type[ASSET_VOLUME_MUTE] = SCALE_CENTERED;
	asset_scale_type[ASSET_VOLUME] = SCALE_CENTERED;
	asset_scale_type[ASSET_WIFI] = SCALE_CENTERED;

	// Count virtual assets to calculate extra space needed
	int virtual_asset_count = 0;
	for (int i = 0; i < ASSET_COUNT; i++) {
		if (asset_scale_type[i] == SCALE_OPTION)
			virtual_asset_count++;
	}

	// Always scale if we have virtual assets or dp_scale doesn't match asset tier
	int needs_scaling =
	    (fabsf(gfx_dp_scale - (float)asset_scale) > 0.01f) || (virtual_asset_count > 0);

	if (needs_scaling) {
		// Calculate destination sheet dimensions
		// Add extra row for virtual assets (those scaled to custom sizes)
		int sheet_w = (int)(loaded_assets->w * scale_ratio + 0.5f);
		int base_h = (int)(loaded_assets->h * scale_ratio + 0.5f);
		int virtual_row_h = (virtual_asset_count > 0) ? option_px : 0;
		int sheet_h = base_h + virtual_row_h;

		// Create destination surface in RGBA8888 format
		gfx.assets = SDL_CreateRGBSurface(0, sheet_w, sheet_h, 32, RGBA_MASK_8888);

		// Track position for virtual assets
		int virtual_x = 0;
		int virtual_y = base_h;

		// Process each asset individually
		for (int i = 0; i < ASSET_COUNT; i++) {
			// Source rectangle in the @Nx sheet
			SDL_Rect src_rect = {asset_rects[i].x, asset_rects[i].y, asset_rects[i].w,
			                     asset_rects[i].h};

			// Destination position (scaled proportionally by default)
			SDL_Rect dst_rect = {(int)(src_rect.x * scale_ratio + 0.5f),
			                     (int)(src_rect.y * scale_ratio + 0.5f), 0, 0};

			int target_w, target_h;

			// Determine target dimensions based on scale type
			switch (asset_scale_type[i]) {
			case SCALE_PILL:
				target_w = target_h = pill_px;
				break;

			case SCALE_BUTTON:
				target_w = target_h = button_px;
				break;

			case SCALE_OPTION:
				// Virtual asset - place in dedicated area at bottom of sheet
				target_w = target_h = option_px;
				dst_rect.x = virtual_x;
				dst_rect.y = virtual_y;
				virtual_x += option_px;
				break;

			case SCALE_CENTERED:
				// Proportional but ensure even offset from pill for centering
				target_w = (int)(src_rect.w * scale_ratio + 0.5f);
				target_h = (int)(src_rect.h * scale_ratio + 0.5f);
				if ((pill_px - target_w) % 2 != 0)
					target_w++;
				if ((pill_px - target_h) % 2 != 0)
					target_h++;
				break;

			case SCALE_PROPORTIONAL:
			default:
				target_w = (int)(src_rect.w * scale_ratio + 0.5f);
				target_h = (int)(src_rect.h * scale_ratio + 0.5f);
				break;
			}

			// Extract this asset region from source sheet into RGBA8888 surface
			SDL_Surface* extracted =
			    SDL_CreateRGBSurface(0, src_rect.w, src_rect.h, 32, RGBA_MASK_8888);
			// Disable alpha-blending to copy RGBA data directly (not blend with dst)
			SDLX_SetAlpha(loaded_assets, 0, 0);
			SDL_BlitSurface(loaded_assets, &src_rect, extracted, &(SDL_Rect){0, 0});
			SDLX_SetAlpha(loaded_assets, SDL_SRCALPHA, 0); // Re-enable for later

			// Scale this specific asset to its target size (keep in RGBA8888)
			SDL_Surface* scaled = SDL_CreateRGBSurface(0, target_w, target_h, 32, RGBA_MASK_8888);
			GFX_scaleBilinear(extracted, scaled); // Direct pixel copy preserves alpha

			// Place the scaled asset into the destination sheet
			// Disable alpha-blending to copy RGBA data directly
			SDLX_SetAlpha(scaled, 0, 0);
			SDL_BlitSurface(scaled, NULL, gfx.assets, &dst_rect);

			// Update asset rectangle with new position and dimensions
			asset_rects[i] = (SDL_Rect){dst_rect.x, dst_rect.y, target_w, target_h};

			// Clean up temporary surfaces for this asset
			SDL_FreeSurface(extracted);
			SDL_FreeSurface(scaled);
		}

		// Done with source sheet
		SDL_FreeSurface(loaded_assets);

		LOG_info("GFX_init: Scaled %d assets from @%dx to dp_scale=%.2f (bilinear)\n", ASSET_COUNT,
		         asset_scale, gfx_dp_scale);
	} else {
		// Perfect match, use assets as-is
		gfx.assets = loaded_assets;
		LOG_info("GFX_init: Using assets@%dx (exact match for dp_scale=%.2f)\n", asset_scale,
		         gfx_dp_scale);
	}

#undef ASSET_SCALE4

	TTF_Init();
	font.large = TTF_OpenFont(FONT_PATH, DP(FONT_LARGE));
	font.medium = TTF_OpenFont(FONT_PATH, DP(FONT_MEDIUM));
	font.small = TTF_OpenFont(FONT_PATH, DP(FONT_SMALL));
	font.tiny = TTF_OpenFont(FONT_PATH, DP(FONT_TINY));

	return gfx.screen;
}

/**
 * Shuts down the graphics subsystem and frees all resources.
 *
 * Closes all fonts, frees the asset texture, clears video memory,
 * and calls platform-specific cleanup.
 *
 * @note Should be called before program exit to prevent resource leaks
 */
void GFX_quit(void) {
	TTF_CloseFont(font.large);
	TTF_CloseFont(font.medium);
	TTF_CloseFont(font.small);
	TTF_CloseFont(font.tiny);

	SDL_FreeSurface(gfx.assets);

	GFX_freeAAScaler();

	GFX_clearAll();

	PLAT_quitVideo();
}

/**
 * Sets the display mode for UI rendering.
 *
 * @param mode MODE_MAIN (launcher) or MODE_MENU (in-game menu)
 *
 * @note Affects UI styling - main mode uses darker backgrounds
 */
void GFX_setMode(int mode) {
	gfx.mode = mode;
}

/**
 * Gets the current vsync setting.
 *
 * @return VSYNC_OFF, VSYNC_LENIENT, or VSYNC_STRICT
 */
int GFX_getVsync(void) {
	return gfx.vsync;
}

/**
 * Sets the vsync behavior for frame synchronization.
 *
 * Vsync modes:
 * - VSYNC_OFF: No frame limiting (uses SDL_Delay fallback)
 * - VSYNC_LENIENT: Skip vsync if frame took too long (default)
 * - VSYNC_STRICT: Always vsync, even if it causes slowdown
 *
 * @param vsync Vsync mode (VSYNC_OFF, VSYNC_LENIENT, VSYNC_STRICT)
 */
void GFX_setVsync(int vsync) {
	PLAT_setVsync(vsync);
	gfx.vsync = vsync;
}

/**
 * Detects if HDMI connection state has changed.
 *
 * Tracks whether HDMI was connected/disconnected since last check.
 * Used to trigger display reconfiguration when needed.
 *
 * @return 1 if HDMI state changed, 0 otherwise
 */
int GFX_hdmiChanged(void) {
	static int had_hdmi = -1;
	int has_hdmi = GetHDMI();
	if (had_hdmi == -1)
		had_hdmi = has_hdmi;
	if (had_hdmi == has_hdmi)
		return 0;
	had_hdmi = has_hdmi;
	return 1;
}

// Target frame time in milliseconds (60fps)
#define FRAME_BUDGET 17
static uint32_t frame_start = 0;

/**
 * Marks the beginning of a new frame for timing purposes.
 *
 * Records the current timestamp to calculate frame duration.
 * Used in conjunction with GFX_sync() to maintain consistent frame rate.
 *
 * @note Call this at the start of your render loop
 */
void GFX_startFrame(void) {
	frame_start = SDL_GetTicks();
}

/**
 * Presents the rendered frame to the display.
 *
 * Decides whether to use vsync based on the current vsync mode
 * and frame timing. With VSYNC_LENIENT, skips vsync if the frame
 * took longer than FRAME_BUDGET to avoid slowdown.
 *
 * @param screen SDL surface to flip to the display
 *
 * @note Call GFX_startFrame() before rendering for proper timing
 */
void GFX_flip(SDL_Surface* screen) {
	int should_vsync = (gfx.vsync != VSYNC_OFF && (gfx.vsync == VSYNC_STRICT || frame_start == 0 ||
	                                               SDL_GetTicks() - frame_start < FRAME_BUDGET));
	PLAT_flip(screen, should_vsync);
}

/**
 * Synchronizes to maintain 60fps when not flipping this frame.
 *
 * Call this if you skip rendering a frame but still want to maintain
 * consistent timing. Waits for the remainder of the frame budget using
 * vsync or SDL_Delay depending on settings.
 *
 * This helps SuperFX games run smoother by maintaining frame timing
 * even when frames are dropped.
 */
void GFX_sync(void) {
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (gfx.vsync != VSYNC_OFF) {
		// this limiting condition helps SuperFX chip games
		if (gfx.vsync == VSYNC_STRICT || frame_start == 0 ||
		    frame_duration < FRAME_BUDGET) { // only wait if we're under frame budget
			PLAT_vsync(FRAME_BUDGET - frame_duration);
		}
	} else {
		if (frame_duration < FRAME_BUDGET)
			SDL_Delay(FRAME_BUDGET - frame_duration);
	}
}

/**
 * Checks if the platform supports overscan adjustment.
 *
 * Default implementation returns 0 (no overscan support).
 * Platforms can override this weak symbol to enable overscan.
 *
 * @return 1 if overscan is supported, 0 otherwise
 */
FALLBACK_IMPLEMENTATION int PLAT_supportsOverscan(void) {
	return 0;
}

/**
 * Sets the color for screen effects (scanlines, grids).
 *
 * Default implementation does nothing. Platforms with effect
 * support override this weak symbol.
 *
 * @param next_color Color index for the effect
 */
FALLBACK_IMPLEMENTATION void PLAT_setEffectColor(int next_color) {}

///////////////////////////////
// Graphics - Text rendering and formatting
///////////////////////////////

/**
 * Truncates text to fit within a maximum width, adding ellipsis if needed.
 *
 * Progressively removes characters from the end and adds "..." until
 * the text fits. Modifies out_name in place.
 *
 * @param font TTF font to measure text with
 * @param in_name Input text string
 * @param out_name Output buffer for truncated text
 * @param max_width Maximum width in pixels
 * @param padding Additional padding to account for in width
 * @return Final width of the text in pixels (including padding)
 */
// Note: GFX_truncateText() moved to workspace/all/common/gfx_text.c for testability

// Note: GFX_wrapText() moved to workspace/all/common/gfx_text.c for testability

///////////////////////////////
// Graphics - Anti-aliased scaling (from picoarch)
///////////////////////////////

// Blend arguments structure for anti-aliased scaling
// Stores the ratio and blend point calculations for both dimensions
struct blend_args {
	int w_ratio_in;
	int w_ratio_out;
	uint16_t w_bp[2];
	int h_ratio_in;
	int h_ratio_out;
	uint16_t h_bp[2];
	uint16_t* blend_line; // Temporary buffer for blended scanlines
} blend_args;

// Color averaging macros using math_utils functions from utils.c
// Skip averaging if colors are identical for optimization
#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#define AVERAGE16(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE16_NOCHK((c1), (c2)))
// 1:3 weighted average (closer to c2)
#define AVERAGE16_1_3(c1, c2)                                                                      \
	((c1) == (c2) ? (c1) : (AVERAGE16_NOCHK(AVERAGE16_NOCHK((c1), (c2)), (c2))))

#define AVERAGE32(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE32_NOCHK((c1), (c2)))
// 1:3 weighted average for paired RGB565 pixels
#define AVERAGE32_1_3(c1, c2)                                                                      \
	((c1) == (c2) ? (c1) : (AVERAGE32_NOCHK(AVERAGE32_NOCHK((c1), (c2)), (c2))))

/**
 * Anti-aliased scaler implementation using bilinear interpolation.
 *
 * Scales RGB565 source image to destination with anti-aliasing for smoother
 * results than nearest-neighbor. Uses quintic blending zones for smooth
 * transitions between pixels.
 *
 * Algorithm divides each source pixel into 5 zones when scaling:
 * - Zone 1 (outer): 100% source pixel A
 * - Zone 2: 75% A, 25% B (1:3 blend)
 * - Zone 3 (center): 50% A, 50% B (even blend)
 * - Zone 4: 25% A, 75% B (1:3 blend)
 * - Zone 5 (outer): 100% source pixel B
 *
 * @param src Source image data (RGB565 format)
 * @param dst Destination image buffer
 * @param w Source width in pixels
 * @param h Source height in pixels
 * @param pitch Source pitch in bytes
 * @param dst_w Destination width in pixels
 * @param dst_h Destination height in pixels
 * @param dst_p Destination pitch in bytes
 *
 * @note Requires blend_args to be initialized via GFX_getAAScaler first
 */
static void scaleAA(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h,
                    uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_p) {
	int dy = 0;
	int lines = h;

	int rat_w = blend_args.w_ratio_in;
	int rat_dst_w = blend_args.w_ratio_out;
	const uint16_t* bw = blend_args.w_bp;

	int rat_h = blend_args.h_ratio_in;
	int rat_dst_h = blend_args.h_ratio_out;
	const uint16_t* bh = blend_args.h_bp;

	while (lines--) {
		while (dy < rat_dst_h) {
			uint16_t* dst16 = (uint16_t*)dst;
			uint16_t* pblend = (uint16_t*)blend_args.blend_line;
			int col = w;
			int dx = 0;

			uint16_t* pnext = (uint16_t*)(src + pitch);
			if (!lines)
				pnext -= (pitch / sizeof(uint16_t));

			if (dy > rat_dst_h - bh[0]) {
				pblend = pnext;
			} else if (dy <= bh[0]) {
				/* Drops const, won't get touched later though */
				pblend = (uint16_t*)src;
			} else {
				const uint32_t* src32 = (const uint32_t*)src;
				const uint32_t* pnext32 = (const uint32_t*)pnext;
				uint32_t* pblend32 = (uint32_t*)pblend;
				int count = w / 2;

				if (dy <= bh[1]) {
					const uint32_t* tmp = pnext32;
					pnext32 = src32;
					src32 = tmp;
				}

				if (dy > rat_dst_h - bh[1] || dy <= bh[1]) {
					while (count--) {
						*pblend32++ = AVERAGE32_1_3(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				} else {
					while (count--) {
						*pblend32++ = AVERAGE32(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				}
			}

			while (col--) {
				uint16_t a, b, out;

				a = *pblend;
				b = *(pblend + 1);

				while (dx < rat_dst_w) {
					if (a == b) {
						out = a;
					} else if (dx > rat_dst_w - bw[0]) { // top quintile, bbbb
						out = b;
					} else if (dx <= bw[0]) { // last quintile, aaaa
						out = a;
					} else {
						if (dx > rat_dst_w - bw[1]) { // 2nd quintile, abbb
							a = AVERAGE16_NOCHK(a, b);
						} else if (dx <= bw[1]) { // 4th quintile, aaab
							b = AVERAGE16_NOCHK(a, b);
						}

						out = AVERAGE16_NOCHK(a, b); // also 3rd quintile, aabb
					}
					*dst16++ = out;
					dx += rat_w;
				}

				dx -= rat_dst_w;
				pblend++;
			}

			dy += rat_h;
			dst += dst_p;
		}

		dy -= rat_dst_h;
		src += pitch;
	}
}

/**
 * Initializes the anti-aliased scaler for a given renderer configuration.
 *
 * Calculates blend ratios and breakpoints based on the GCD of source
 * and destination dimensions. Allocates a temporary scanline buffer
 * for blending operations.
 *
 * The blend_denominator controls blend zone widths:
 * - 5.0 for downscaling (sharper)
 * - 2.5 for upscaling (smoother)
 *
 * @param renderer Renderer configuration with source/dest dimensions
 * @return Function pointer to scaleAA scaler implementation
 *
 * @note Allocates blend_line buffer - call GFX_freeAAScaler to free
 */
scaler_t GFX_getAAScaler(const GFX_Renderer* renderer) {
	int gcd_w, div_w, gcd_h, div_h;
	blend_args.blend_line = calloc(renderer->src_w, sizeof(uint16_t));

	gcd_w = gcd(renderer->src_w, renderer->dst_w);
	blend_args.w_ratio_in = renderer->src_w / gcd_w;
	blend_args.w_ratio_out = renderer->dst_w / gcd_w;

	double blend_denominator = (renderer->src_w > renderer->dst_w)
	                               ? 5
	                               : 2.5; // TODO: these values are really only good for the nano...
	// blend_denominator = 5.0; // better for trimui
	// LOG_info("blend_denominator: %f (%i && %i)\n", blend_denominator, HAS_SKINNY_SCREEN, renderer->dst_w>renderer->src_w);

	div_w = round(blend_args.w_ratio_out / blend_denominator);
	blend_args.w_bp[0] = div_w;
	blend_args.w_bp[1] = blend_args.w_ratio_out >> 1;

	gcd_h = gcd(renderer->src_h, renderer->dst_h);
	blend_args.h_ratio_in = renderer->src_h / gcd_h;
	blend_args.h_ratio_out = renderer->dst_h / gcd_h;

	div_h = round(blend_args.h_ratio_out / blend_denominator);
	blend_args.h_bp[0] = div_h;
	blend_args.h_bp[1] = blend_args.h_ratio_out >> 1;

	return scaleAA;
}

/**
 * Frees resources allocated by the anti-aliased scaler.
 *
 * Deallocates the temporary scanline buffer used for blending.
 * Safe to call even if scaler was never initialized.
 */
void GFX_freeAAScaler(void) {
	if (blend_args.blend_line != NULL) {
		free(blend_args.blend_line);
		blend_args.blend_line = NULL;
	}
}

///////////////////////////////
// Graphics - Asset and UI element rendering
///////////////////////////////

/**
 * Blits a UI asset from the asset texture to a destination surface.
 *
 * Assets are pre-defined rectangular regions in the asset PNG file.
 * The src_rect parameter allows blitting only part of an asset.
 *
 * @param asset Asset ID (e.g., ASSET_BUTTON, ASSET_BATTERY)
 * @param src_rect Optional source rectangle within the asset (NULL for full asset)
 * @param dst Destination surface
 * @param dst_rect Destination rectangle (NULL to blit at 0,0)
 */
void GFX_blitAsset(int asset, const SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect) {
	const SDL_Rect* rect = &asset_rects[asset];
	SDL_Rect adj_rect = {
	    .x = rect->x,
	    .y = rect->y,
	    .w = rect->w,
	    .h = rect->h,
	};
	if (src_rect) {
		adj_rect.x += src_rect->x;
		adj_rect.y += src_rect->y;
		adj_rect.w = src_rect->w;
		adj_rect.h = src_rect->h;
	}
	SDL_BlitSurface(gfx.assets, &adj_rect, dst, dst_rect);
}

/**
 * Renders a rounded pill-shaped UI element.
 *
 * Pills are composed of rounded ends from the asset texture with a
 * stretched middle section. Used for buttons, progress bars, and
 * status indicators.
 *
 * @param asset Asset to use for pill caps (determines color/style)
 * @param dst Destination surface
 * @param dst_rect Desired pill dimensions and position
 *
 * @note If height is 0, uses asset's default height
 */
void GFX_blitPill(int asset, SDL_Surface* dst, const SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;

	if (h == 0)
		h = asset_rects[asset].h;

	// Asset is a square (pill_px × pill_px), split into left and right halves
	// For odd heights, left cap gets the extra pixel to avoid clipping
	int asset_w = asset_rects[asset].w;
	int left_cap = (asset_w + 1) / 2; // rounds up for odd widths
	int right_cap = asset_w / 2; // rounds down for odd widths

	if (w < h)
		w = h;
	int middle = w - h;

	GFX_blitAsset(asset, &(SDL_Rect){0, 0, left_cap, h}, dst, &(SDL_Rect){x, y});
	x += left_cap;
	if (middle > 0) {
		SDL_FillRect(dst, &(SDL_Rect){x, y, middle, h}, asset_rgbs[asset]);
		x += middle;
	}
	GFX_blitAsset(asset, &(SDL_Rect){left_cap, 0, right_cap, h}, dst, &(SDL_Rect){x, y});
}

/**
 * Renders a rounded rectangle UI element with stretched corners.
 *
 * Similar to pills but for rectangular regions. Blits the four
 * corners from the asset and fills edges and center with solid color.
 *
 * @param asset Asset to use for corner sprites and fill color
 * @param dst Destination surface
 * @param dst_rect Rectangle dimensions and position
 */
void GFX_blitRect(int asset, SDL_Surface* dst, const SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;
	int c = asset_rgbs[asset];

	const SDL_Rect* rect = &asset_rects[asset];
	int d = rect->w;
	int r = d / 2;

	GFX_blitAsset(asset, &(SDL_Rect){0, 0, r, r}, dst, &(SDL_Rect){x, y});
	SDL_FillRect(dst, &(SDL_Rect){x + r, y, w - d, r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r, 0, r, r}, dst, &(SDL_Rect){x + w - r, y});
	SDL_FillRect(dst, &(SDL_Rect){x, y + r, w, h - d}, c);
	GFX_blitAsset(asset, &(SDL_Rect){0, r, r, r}, dst, &(SDL_Rect){x, y + h - r});
	SDL_FillRect(dst, &(SDL_Rect){x + r, y + h - r, w - d, r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r, r, r, r}, dst, &(SDL_Rect){x + w - r, y + h - r});
}

/**
 * Renders the battery status indicator.
 *
 * Displays either:
 * - Battery icon with charge level fill (when not charging)
 * - Battery icon with charging bolt (when charging)
 *
 * Battery color changes to red when charge is <= 10%.
 * The fill bar shows percentage visually.
 *
 * @param dst Destination surface
 * @param dst_rect Position for battery indicator
 *
 * @note Uses global pwr.is_charging and pwr.charge values
 */
void GFX_blitBattery(SDL_Surface* dst, const SDL_Rect* dst_rect) {
	// LOG_info("dst: %p\n", dst);
	int x = 0;
	int y = 0;
	if (dst_rect) {
		x = dst_rect->x;
		y = dst_rect->y;
	}
	SDL_Rect rect = asset_rects[ASSET_BATTERY];
	int x_off = (DP(ui.pill_height) - rect.w) / 2;
	int y_off = (DP(ui.pill_height) - rect.h) / 2;

	static int logged = 0;
	if (!logged) {
		LOG_info("GFX_blitBattery: pill=%dpx, battery=%dx%d, offset=(%d,%d)\n", DP(ui.pill_height),
		         rect.w, rect.h, x_off, y_off);
		logged = 1;
	}

	x += x_off;
	y += y_off;

	// Battery fill/bolt offsets must scale with the actual battery asset size
	// At @1x design: fill is 3px right, 2px down from battery top-left
	// Scale this based on actual battery width: rect.w / 17 (17 = @1x battery width)
	int fill_x_offset = (rect.w * 3 + 8) / 17; // (rect.w * 3/17), rounded
	int fill_y_offset = (rect.h * 2 + 5) / 10; // (rect.h * 2/10), rounded

	if (pwr.is_charging) {
		if (!logged) {
			LOG_info("Battery bolt: offset=(%d,%d) from battery corner\n", fill_x_offset,
			         fill_y_offset);
		}
		GFX_blitAsset(ASSET_BATTERY, NULL, dst, &(SDL_Rect){x, y});
		GFX_blitAsset(ASSET_BATTERY_BOLT, NULL, dst,
		              &(SDL_Rect){x + fill_x_offset, y + fill_y_offset});
	} else {
		int percent = pwr.charge;
		GFX_blitAsset(percent <= 10 ? ASSET_BATTERY_LOW : ASSET_BATTERY, NULL, dst,
		              &(SDL_Rect){x, y});

		rect = asset_rects[ASSET_BATTERY_FILL];

		if (!logged) {
			LOG_info("Battery fill: %dx%d, offset=(%d,%d)\n", rect.w, rect.h, fill_x_offset,
			         fill_y_offset);
		}

		SDL_Rect clip = rect;
		clip.w *= percent;
		clip.w /= 100;
		if (clip.w <= 0)
			return;
		clip.x = rect.w - clip.w;
		clip.y = 0;

		GFX_blitAsset(percent <= 20 ? ASSET_BATTERY_FILL_LOW : ASSET_BATTERY_FILL, &clip, dst,
		              &(SDL_Rect){x + fill_x_offset + clip.x, y + fill_y_offset});
	}
}

/**
 * Calculates the total width needed for a button with hint text.
 *
 * Buttons consist of:
 * - Icon/button label (circular or pill-shaped)
 * - Spacing
 * - Hint text
 *
 * @param hint Hint text to display next to button
 * @param button Button label (e.g., "A", "B", "START")
 * @return Total width in pixels needed for button and hint
 *
 * @note Special handling for BRIGHTNESS_BUTTON_LABEL ("+ -")
 */
int GFX_getButtonWidth(char* hint, char* button) {
	int button_width = 0;
	int width;

	int special_case = !strcmp(button, BRIGHTNESS_BUTTON_LABEL); // TODO: oof

	if (strlen(button) == 1) {
		button_width += DP(ui.button_size);
	} else {
		button_width += DP(ui.button_size) / 2;
		TTF_SizeUTF8(special_case ? font.large : font.tiny, button, &width, NULL);
		button_width += width;
	}
	button_width += DP(ui.button_margin);

	TTF_SizeUTF8(font.small, hint, &width, NULL);
	button_width += width + DP(ui.button_margin);
	return button_width;
}

/**
 * Renders a button with its label and hint text.
 *
 * Single-character labels (A, B, X, Y) are rendered in circular buttons.
 * Multi-character labels (START, SELECT) are rendered in pill-shaped buttons.
 *
 * @param hint Hint text displayed next to button (e.g., "CONFIRM")
 * @param button Button label (e.g., "A", "START")
 * @param dst Destination surface
 * @param dst_rect Position to render button at
 *
 * @note Hint text appears to the right of the button icon
 */
void GFX_blitButton(char* hint, char* button, SDL_Surface* dst, SDL_Rect* dst_rect) {
	SDL_Surface* text;
	int ox = 0;

	int special_case = !strcmp(button, BRIGHTNESS_BUTTON_LABEL); // TODO: oof

	// button
	if (strlen(button) == 1) {
		GFX_blitAsset(ASSET_BUTTON, NULL, dst, dst_rect);

		// label - center glyph in button using precise glyph metrics
		text = TTF_RenderUTF8_Blended(font.medium, button, COLOR_BUTTON_TEXT);
		int offset_x, offset_y;
		GFX_centerGlyph(DP(ui.button_size), DP(ui.button_size), font.medium, (Uint16)button[0],
		                &offset_x, &offset_y);
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){dst_rect->x + offset_x, dst_rect->y + offset_y});
		ox += DP(ui.button_size);
		SDL_FreeSurface(text);
	} else {
		text = TTF_RenderUTF8_Blended(special_case ? font.large : font.tiny, button,
		                              COLOR_BUTTON_TEXT);
		GFX_blitPill(ASSET_BUTTON, dst,
		             &(SDL_Rect){dst_rect->x, dst_rect->y, DP(ui.button_size) / 2 + text->w,
		                         DP(ui.button_size)});
		ox += DP(ui.button_size) / 4;

		int oy = special_case ? DP(-2) : 0;
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){ox + dst_rect->x,
		                            oy + dst_rect->y + DP_CENTER_PX(ui.button_size, text->h),
		                            text->w, text->h});
		ox += text->w;
		ox += DP(ui.button_size) / 4;
		SDL_FreeSurface(text);
	}

	ox += DP(ui.button_margin);

	// hint text
	text = TTF_RenderUTF8_Blended(font.small, hint, COLOR_WHITE);
	SDL_BlitSurface(text, NULL, dst,
	                &(SDL_Rect){ox + dst_rect->x,
	                            dst_rect->y + DP_CENTER_PX(ui.button_size, text->h), text->w,
	                            text->h});
	SDL_FreeSurface(text);
}

/**
 * Renders a multi-line text message centered in a rectangular area.
 *
 * Splits the message into lines and renders them vertically centered
 * within the destination rectangle. Each line is horizontally centered.
 *
 * @param font TTF font to render with
 * @param msg Message text (can contain newlines)
 * @param dst Destination surface
 * @param dst_rect Area to center message in (NULL for full surface)
 *
 * @note Maximum 16 lines supported, line height is fixed at 24 (scaled)
 */
void GFX_blitMessage(TTF_Font* ttf_font, char* msg, SDL_Surface* dst, const SDL_Rect* dst_rect) {
	if (!dst_rect)
		dst_rect = &(SDL_Rect){0, 0, dst->w, dst->h};

	// LOG_info("GFX_blitMessage: %p (%ix%i)", dst, dst_rect->w,dst_rect->h);

	SDL_Surface* text;
#define TEXT_BOX_MAX_ROWS 16
#define LINE_HEIGHT 24
	char* rows[TEXT_BOX_MAX_ROWS];
	int row_count = splitTextLines(msg, rows, TEXT_BOX_MAX_ROWS);
	if (row_count == 0)
		return;

	int rendered_height = DP(LINE_HEIGHT) * row_count;
	int y = dst_rect->y;
	y += (dst_rect->h - rendered_height) / 2;

	char line[256];
	for (int i = 0; i < row_count; i++) {
		int len;
		if (i + 1 < row_count) {
			len = rows[i + 1] - rows[i] - 1;
			if (len)
				strncpy(line, rows[i], len);
			line[len] = '\0';
		} else {
			len = strlen(rows[i]);
			strcpy(line, rows[i]);
		}


		if (len) {
			text = TTF_RenderUTF8_Blended(ttf_font, line, COLOR_WHITE);
			int x = dst_rect->x;
			x += (dst_rect->w - text->w) / 2;
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x, y});
			SDL_FreeSurface(text);
		}
		y += DP(LINE_HEIGHT);
	}
}

/**
 * Renders the hardware status group (battery, wifi, brightness/volume).
 *
 * Displays in top-right corner of screen. Shows either:
 * - Setting adjustment UI (brightness/volume slider with icon)
 * - Status icons (battery, optional wifi)
 *
 * @param dst Destination surface
 * @param show_setting 0=status, 1=brightness, 2=volume
 * @return Width of the rendered group in pixels
 *
 * @note Does not display on HDMI output (except status icons)
 */
int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting) {
	int ox;
	int oy;
	int ow = 0;

	if (show_setting && !GetHDMI()) {
		ow = DP(ui.pill_height + ui.settings_width + ui.padding + 4);
		ox = ui.screen_width_px - DP(ui.edge_padding) - ow;
		oy = DP(ui.edge_padding);
		GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst,
		             &(SDL_Rect){ox, oy, ow, DP(ui.pill_height)});

		int setting_value;
		int setting_min;
		int setting_max;
		if (show_setting == 1) {
			setting_value = GetBrightness();
			setting_min = BRIGHTNESS_MIN;
			setting_max = BRIGHTNESS_MAX;
		} else {
			setting_value = GetVolume();
			setting_min = VOLUME_MIN;
			setting_max = VOLUME_MAX;
		}

		int asset = show_setting == 1 ? ASSET_BRIGHTNESS
		                              : (setting_value > 0 ? ASSET_VOLUME : ASSET_VOLUME_MUTE);
		int ax = ox + (show_setting == 1 ? DP(6) : DP(8));
		int ay = oy + (show_setting == 1 ? DP(5) : DP(7));
		GFX_blitAsset(asset, NULL, dst, &(SDL_Rect){ax, ay});

		ox += DP(ui.pill_height);
		oy += (DP(ui.pill_height) - DP(ui.settings_size)) / 2;
		GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_BAR_BG : ASSET_BAR_BG_MENU, dst,
		             &(SDL_Rect){ox, oy, DP(ui.settings_width), DP(ui.settings_size)});

		float percent = ((float)(setting_value - setting_min) / (setting_max - setting_min));
		if (show_setting == 1 || setting_value > 0) {
			GFX_blitPill(
			    ASSET_BAR, dst,
			    &(SDL_Rect){ox, oy, DP(ui.settings_width) * percent, DP(ui.settings_size)});
		}
	} else {
		// TODO: handle wifi
		int show_wifi = PLAT_isOnline(); // NOOOOO! not every frame!

		int ww = DP(ui.pill_height - 3);
		ow = DP(ui.pill_height);
		if (show_wifi)
			ow += ww;

		ox = ui.screen_width_px - DP(ui.edge_padding) - ow;
		oy = DP(ui.edge_padding);
		GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst,
		             &(SDL_Rect){ox, oy, ow, DP(ui.pill_height)});
		if (show_wifi) {
			SDL_Rect rect = asset_rects[ASSET_WIFI];
			int x = ox;
			int y = oy;
			x += (DP(ui.pill_height) - rect.w) / 2;
			y += (DP(ui.pill_height) - rect.h) / 2;

			GFX_blitAsset(ASSET_WIFI, NULL, dst, &(SDL_Rect){x, y});
			ox += ww;
		}
		GFX_blitBattery(dst, &(SDL_Rect){ox, oy});
	}

	return ow;
}

/**
 * Renders hardware control button hints at bottom of screen.
 *
 * Shows appropriate button combinations for brightness/volume control
 * based on the current platform's button mapping.
 *
 * @param dst Destination surface
 * @param show_setting Which setting hint to show (1=brightness, 2=volume)
 */
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting) {
	if (BTN_MOD_VOLUME == BTN_SELECT && BTN_MOD_BRIGHTNESS == BTN_START) {
		if (show_setting == 1)
			GFX_blitButtonGroup((char*[]){"SELECT", "VOLUME", NULL}, 0, dst, 0);
		else
			GFX_blitButtonGroup((char*[]){"START", "BRIGHTNESS", NULL}, 0, dst, 0);
	} else {
		if (show_setting == 1)
			GFX_blitButtonGroup((char*[]){BRIGHTNESS_BUTTON_LABEL, "BRIGHTNESS", NULL}, 0, dst, 0);
		else
			GFX_blitButtonGroup((char*[]){"MENU", "BRIGHTNESS", NULL}, 0, dst, 0);
	}
}

/**
 * Renders a group of buttons with hints in a single pill container.
 *
 * Displays up to 2 button-hint pairs in a styled pill background.
 * On narrow screens, only shows the primary button to save space.
 *
 * @param pairs Array of [button, hint, button, hint, NULL] strings
 * @param primary Which button index is primary (0 or 1)
 * @param dst Destination surface
 * @param align_right 1 to right-align, 0 to left-align
 * @return Total width of the rendered button group in pixels
 */
int GFX_blitButtonGroup(char** pairs, int primary, SDL_Surface* dst, int align_right) {
	int ox;
	int oy;
	int ow;
	char* hint;
	char* button;

	struct Hint {
		char* hint;
		char* button;
		int ow;
	} hints[2];
	int w = 0; // individual button dimension
	int h = 0; // hints index
	ow = 0; // full pill width
	ox = align_right ? dst->w - DP(ui.edge_padding) : DP(ui.edge_padding);
	oy = dst->h - DP(ui.edge_padding + ui.pill_height);

	for (int i = 0; i < 2; i++) {
		if (!pairs[i * 2])
			break;
		if (HAS_SKINNY_SCREEN && i != primary)
			continue; // space saving

		button = pairs[i * 2];
		hint = pairs[i * 2 + 1];
		w = GFX_getButtonWidth(hint, button);
		hints[h].hint = hint;
		hints[h].button = button;
		hints[h].ow = w;
		h += 1;
		ow += DP(ui.button_margin) + w;
	}

	ow += DP(ui.button_margin);
	if (align_right)
		ox -= ow;
	GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst,
	             &(SDL_Rect){ox, oy, ow, DP(ui.pill_height)});

	ox += DP(ui.button_margin);
	oy += DP(ui.button_margin);
	for (int i = 0; i < h; i++) {
		GFX_blitButton(hints[i].hint, hints[i].button, dst, &(SDL_Rect){ox, oy});
		ox += hints[i].ow + DP(ui.button_margin);
	}
	return ow;
}

#define MAX_TEXT_LINES 16

/**
 * Calculates the dimensions of multi-line text.
 *
 * Splits text by newlines and measures the widest line.
 * Height is calculated as line count * leading.
 *
 * @param font TTF font to measure with
 * @param str Text to measure (may contain newlines)
 * @param leading Line height in pixels
 * @param w Output: width of widest line
 * @param h Output: total height (lines * leading)
 */
// Note: GFX_sizeText() moved to workspace/all/common/gfx_text.c for testability

/**
 * Renders multi-line text centered in a rectangular area.
 *
 * Splits text by newlines and renders each line horizontally centered.
 * Lines are spaced vertically by the leading parameter.
 *
 * @param font TTF font to render with
 * @param str Text to render (may contain newlines)
 * @param leading Line spacing in pixels
 * @param color Text color
 * @param dst Destination surface
 * @param dst_rect Area to center text in (NULL for full surface)
 *
 * @note Maximum 16 lines supported
 */
void GFX_blitText(TTF_Font* ttf_font, char* str, int leading, SDL_Color color, SDL_Surface* dst,
                  SDL_Rect* dst_rect) {
	if (dst_rect == NULL)
		dst_rect = &(SDL_Rect){0, 0, dst->w, dst->h};

	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, MAX_TEXT_LINES);
	int x = dst_rect->x;
	int y = dst_rect->y;

	SDL_Surface* text;
	char line[256];
	for (int i = 0; i < count; i++) {
		int len;
		if (i + 1 < count) {
			len = lines[i + 1] - lines[i] - 1;
			if (len)
				strncpy(line, lines[i], len);
			line[len] = '\0';
		} else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}

		if (len) {
			text = TTF_RenderUTF8_Blended(ttf_font, line, color);
			SDL_BlitSurface(text, NULL, dst,
			                &(SDL_Rect){x + ((dst_rect->w - text->w) / 2), y + (i * leading)});
			SDL_FreeSurface(text);
		}
	}
}

///////////////////////////////
// Sound system - Ring buffer-based audio mixer
// (Based on picoarch's audio implementation)
///////////////////////////////

#define MAX_SAMPLE_RATE 48000
#define BATCH_SIZE 100 // Max frames to batch per write
#ifndef SAMPLES
#define SAMPLES 512 // SDL audio buffer size (default)
#endif

#define ms SDL_GetTicks // Shorthand for timestamp

// Sound context manages the ring buffer and resampling
static struct SND_Context {
	int initialized;
	double frame_rate;

	int sample_rate_in;
	int sample_rate_out;

	int buffer_seconds; // current_audio_buffer_size
	SND_Frame* buffer; // buf
	size_t frame_count; // buf_len

	int frame_in; // buf_w
	int frame_out; // buf_r
	int frame_filled; // max_buf_w

	// Linear interpolation resampler with dynamic rate control
	AudioResampler resampler;
} snd = {0};

/**
 * SDL audio callback - consumes samples from the ring buffer.
 *
 * This is called by SDL on the audio thread when it needs more audio data.
 * Reads samples from the ring buffer and writes them to the output stream.
 * If buffer runs dry, repeats last sample or outputs silence.
 *
 * @param userdata Unused user data pointer
 * @param stream Output audio buffer to fill
 * @param len Length of output buffer in bytes
 *
 * @note Runs on SDL's audio thread, not the main thread
 */
static void SND_audioCallback(void* userdata, uint8_t* stream, int len) { // plat_sound_callback

	// return (void)memset(stream,0,len); // TODO: tmp, silent

	if (snd.frame_count == 0)
		return;

	int16_t* out = (int16_t*)stream;
	len /= (sizeof(int16_t) * 2);
	// int full_len = len;

	// if (snd.frame_out!=snd.frame_in) LOG_info("%8i consuming samples (%i frames)\n", ms(), len);

	while (snd.frame_out != snd.frame_in && len > 0) {
		*out++ = snd.buffer[snd.frame_out].left;
		*out++ = snd.buffer[snd.frame_out].right;

		snd.frame_filled = snd.frame_out;

		snd.frame_out += 1;
		len -= 1;

		if (snd.frame_out >= (int)snd.frame_count)
			snd.frame_out = 0;
	}

	// Handle underrun: repeat last frame or output silence
	if (len > 0) {
		if (snd.frame_filled >= 0 && snd.frame_filled < (int)snd.frame_count) {
			// Repeat last consumed frame to avoid click
			SND_Frame last = snd.buffer[snd.frame_filled];
			while (len > 0) {
				*out++ = last.left;
				*out++ = last.right;
				len--;
			}
		} else {
			// No valid last frame - output silence
			memset(out, 0, len * sizeof(int16_t) * 2);
		}
	}
}

/**
 * Resizes the audio ring buffer based on sample rate and frame rate.
 *
 * Calculates buffer size to hold buffer_seconds worth of audio.
 * Locks audio thread during resize to prevent corruption.
 *
 * @note Called during init and when audio parameters change
 */
static void SND_resizeBuffer(void) { // plat_sound_resize_buffer
	snd.frame_count = snd.buffer_seconds * snd.sample_rate_in / snd.frame_rate;
	if (snd.frame_count == 0)
		return;

	// LOG_info("frame_count: %i (%i * %i / %f)\n", snd.frame_count, snd.buffer_seconds, snd.sample_rate_in, snd.frame_rate);
	// snd.frame_count *= 2; // no help

	SDL_LockAudio();

	int buffer_bytes = snd.frame_count * sizeof(SND_Frame);
	void* new_buffer = realloc(snd.buffer, buffer_bytes);
	if (!new_buffer) {
		LOG_error("Failed to allocate audio buffer (%d bytes)\n", buffer_bytes);
		SDL_UnlockAudio();
		return;
	}
	snd.buffer = new_buffer;

	memset(snd.buffer, 0, buffer_bytes);

	snd.frame_in = 0;
	snd.frame_out = 0;
	snd.frame_filled = snd.frame_count - 1;

	SDL_UnlockAudio();
}

/**
 * Calculates buffer fill level as a fraction (0.0 to 1.0).
 *
 * @return Fill level where 0.0 = empty, 1.0 = full
 */
static float SND_getBufferFillLevel(void) {
	if (snd.frame_count == 0)
		return 0.0f;

	int filled;
	if (snd.frame_in >= snd.frame_out) {
		filled = snd.frame_in - snd.frame_out;
	} else {
		filled = snd.frame_count - snd.frame_out + snd.frame_in;
	}

	return (float)filled / (float)snd.frame_count;
}

/**
 * Calculates dynamic rate adjustment based on buffer fill level.
 *
 * Uses a simple proportional controller to keep the buffer around 50% full:
 * - Buffer too empty (<30%): slow down (ratio < 1.0) to produce more output
 * - Buffer too full (>70%): speed up (ratio > 1.0) to consume more input
 * - Buffer near target (30-70%): minimal adjustment
 *
 * @return Rate adjustment factor (typically 0.98 to 1.02)
 */
static float SND_calculateRateAdjust(void) {
	float fill = SND_getBufferFillLevel();

	// Deadband: 30% to 70% (minimal adjustment in this range)
	// Target is implicitly 50% (midpoint of deadband)
	const float deadband_low = 0.3f;
	const float deadband_high = 0.7f;

	// Maximum adjustment: ±2% (0.98 to 1.02)
	// This is subtle enough to be inaudible but effective for drift correction
	const float max_adjust = 0.02f;

	float adjust = 1.0f;

	if (fill < deadband_low) {
		// Buffer getting empty - slow down to produce more output
		// At 0% fill, adjust = 0.98 (slow down 2%)
		float urgency = (deadband_low - fill) / deadband_low;
		adjust = 1.0f - (urgency * max_adjust);
	} else if (fill > deadband_high) {
		// Buffer getting full - speed up to consume more input
		// At 100% fill, adjust = 1.02 (speed up 2%)
		float urgency = (fill - deadband_high) / (1.0f - deadband_high);
		adjust = 1.0f + (urgency * max_adjust);
	}

	return adjust;
}

/**
 * Writes a batch of audio samples to the ring buffer.
 *
 * Uses linear interpolation resampling with dynamic rate control:
 * - Linear interpolation: Smoothly blends between adjacent samples
 * - Dynamic rate control: Adjusts playback speed ±2% based on buffer fill
 *
 * This combination eliminates the clicks/pops of nearest-neighbor resampling
 * while preventing buffer underruns (crackling) and overruns (audio lag).
 *
 * @param frames Array of audio frames to write
 * @param frame_count Number of frames in array
 * @return Number of frames consumed
 *
 * @note May block briefly if ring buffer is full
 */
size_t SND_batchSamples(const SND_Frame* frames,
                        size_t frame_count) { // plat_sound_write / plat_sound_write_resample

	if (snd.frame_count == 0)
		return 0;

	SDL_LockAudio();

	// Calculate dynamic rate adjustment based on buffer fill level
	float rate_adjust = SND_calculateRateAdjust();

	// Estimate how many OUTPUT frames we'll produce (may be more than input when upsampling)
	int estimated_output = AudioResampler_estimateOutput(&snd.resampler, frame_count, rate_adjust);

	// Calculate how much space is available in the ring buffer
	int available;
	if (snd.frame_in >= snd.frame_out) {
		available = snd.frame_count - (snd.frame_in - snd.frame_out) - 1;
	} else {
		available = snd.frame_out - snd.frame_in - 1;
	}

	// If buffer doesn't have room for estimated output, wait a bit
	int tries = 0;
	while (tries < 10 && available < estimated_output) {
		tries++;
		SDL_UnlockAudio();
		SDL_Delay(1);
		SDL_LockAudio();

		// Recalculate available space
		if (snd.frame_in >= snd.frame_out) {
			available = snd.frame_count - (snd.frame_in - snd.frame_out) - 1;
		} else {
			available = snd.frame_out - snd.frame_in - 1;
		}
	}

	// Set up ring buffer wrapper for the resampler
	AudioRingBuffer ring = {
	    .frames = snd.buffer,
	    .capacity = snd.frame_count,
	    .write_pos = snd.frame_in,
	    .read_pos = snd.frame_out,
	};

	// Resample the audio with linear interpolation
	ResampleResult result =
	    AudioResampler_resample(&snd.resampler, &ring, frames, frame_count, rate_adjust);

	// Update ring buffer write position
	snd.frame_in = ring.write_pos;

	// Note: frame_filled is managed by the audio callback (SND_audioCallback)
	// to track what has been consumed. We don't update it here.

	SDL_UnlockAudio();

	return result.frames_consumed;
}

/**
 * Initializes the audio subsystem.
 *
 * Sets up SDL audio, allocates the ring buffer, configures resampling.
 * Audio starts playing immediately after this call.
 *
 * @param sample_rate Input sample rate from emulator/game
 * @param frame_rate Frame rate of the game (e.g., 60.0 for 60fps)
 *
 * @note Platform may adjust sample_rate via PLAT_pickSampleRate
 */
void SND_init(double sample_rate, double frame_rate) { // plat_sound_init
	LOG_info("SND_init\n");

	SDL_InitSubSystem(SDL_INIT_AUDIO);

#if defined(USE_SDL2)
	LOG_debug("Available audio drivers:\n");
	for (int i = 0; i < SDL_GetNumAudioDrivers(); i++) {
		LOG_debug("- %s\n", SDL_GetAudioDriver(i));
	}
	LOG_debug("Current audio driver: %s\n", SDL_GetCurrentAudioDriver());
#endif

	memset(&snd, 0, sizeof(struct SND_Context));
	snd.frame_rate = frame_rate;

	SDL_AudioSpec spec_in;
	SDL_AudioSpec spec_out;

	spec_in.freq = PLAT_pickSampleRate(sample_rate, MAX_SAMPLE_RATE);
	spec_in.format = AUDIO_S16;
	spec_in.channels = 2;
	spec_in.samples = SAMPLES;
	spec_in.callback = SND_audioCallback;

	if (SDL_OpenAudio(&spec_in, &spec_out) < 0)
		LOG_error("SDL_OpenAudio error: %s", SDL_GetError());

	snd.buffer_seconds = 5;
	snd.sample_rate_in = sample_rate;
	snd.sample_rate_out = spec_out.freq;

	// Initialize the linear interpolation resampler
	AudioResampler_init(&snd.resampler, snd.sample_rate_in, snd.sample_rate_out);
	SND_resizeBuffer();

	SDL_PauseAudio(0);

	LOG_info("sample rate: %i (req) %i (rec) [samples %i]\n", snd.sample_rate_in,
	         snd.sample_rate_out, SAMPLES);
	snd.initialized = 1;
}

/**
 * Gets current audio buffer fill level as a percentage.
 *
 * Used by libretro cores for audio-based frameskip decisions.
 * Thread-safe: locks audio to read consistent buffer state.
 *
 * @return Fill level 0-100 (0 = empty, 100 = full)
 */
unsigned SND_getBufferOccupancy(void) {
	SDL_LockAudio();
	float fill = SND_getBufferFillLevel();
	SDL_UnlockAudio();
	return (unsigned)(fill * 100.0f);
}

/**
 * Shuts down the audio subsystem and frees resources.
 *
 * Pauses audio, closes SDL audio device, frees ring buffer.
 * Safe to call even if audio was never initialized.
 */
void SND_quit(void) { // plat_sound_finish
	if (!snd.initialized)
		return;

	SDL_PauseAudio(1);
	SDL_CloseAudio();

	if (snd.buffer) {
		free(snd.buffer);
		snd.buffer = NULL;
	}
}

///////////////////////////////
// Input - Lid detection (clamshell devices)
///////////////////////////////

// Global lid state for devices with flip-lid hardware
LID_Context lid = {
    .has_lid = 0,
    .is_open = 1,
};

/**
 * Initializes lid detection hardware.
 *
 * Default implementation does nothing. Platforms with lid hardware
 * (e.g., GKD Pixel) override this to set up GPIO or sensors.
 */
FALLBACK_IMPLEMENTATION void PLAT_initLid(void) {}

/**
 * Checks if lid state has changed.
 *
 * Default implementation returns 0 (no lid). Platforms override
 * this to detect lid open/close events.
 *
 * @param state Output: current lid state (1=open, 0=closed), may be NULL
 * @return 1 if state changed since last call, 0 otherwise
 */
FALLBACK_IMPLEMENTATION int PLAT_lidChanged(int* state) {
	return 0;
}

///////////////////////////////
// Input - Button and analog stick handling
///////////////////////////////

// Note: PAD_* logic functions are now in pad.c for better testability.
// This file only contains the SDL-dependent input polling (PLAT_pollInput).

/**
 * Polls input devices and updates global button state.
 *
 * Default implementation handles:
 * - SDL keyboard events (mapped to buttons)
 * - SDL joystick/gamepad button events
 * - SDL joystick hat (d-pad) events
 * - SDL analog stick axis events
 * - Button repeat timing
 * - Lid close detection (triggers BTN_SLEEP)
 *
 * Called once per frame. Updates pad.just_pressed, pad.is_pressed,
 * pad.just_released, and pad.just_repeated bitmasks.
 *
 * Button repeat behavior:
 * - Initial delay: PAD_REPEAT_DELAY (300ms)
 * - Repeat interval: PAD_REPEAT_INTERVAL (100ms)
 *
 * @note Platforms can override this to handle custom input hardware
 */
FALLBACK_IMPLEMENTATION void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i = 0; i < BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick >= pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	// the actual poll
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		int btn = BTN_NONE;
		int pressed = 0; // 0=up,1=down
		int id = -1;
		if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
			int code = event.key.keysym.scancode;
			pressed = event.type == SDL_KEYDOWN;
			// LOG_info("key event: %i (%i)\n", code,pressed);
			if (code == CODE_UP) {
				btn = BTN_DPAD_UP;
				id = BTN_ID_DPAD_UP;
			} else if (code == CODE_DOWN) {
				btn = BTN_DPAD_DOWN;
				id = BTN_ID_DPAD_DOWN;
			} else if (code == CODE_LEFT) {
				btn = BTN_DPAD_LEFT;
				id = BTN_ID_DPAD_LEFT;
			} else if (code == CODE_RIGHT) {
				btn = BTN_DPAD_RIGHT;
				id = BTN_ID_DPAD_RIGHT;
			} else if (code == CODE_A) {
				btn = BTN_A;
				id = BTN_ID_A;
			} else if (code == CODE_B) {
				btn = BTN_B;
				id = BTN_ID_B;
			} else if (code == CODE_X) {
				btn = BTN_X;
				id = BTN_ID_X;
			} else if (code == CODE_Y) {
				btn = BTN_Y;
				id = BTN_ID_Y;
			} else if (code == CODE_START) {
				btn = BTN_START;
				id = BTN_ID_START;
			} else if (code == CODE_SELECT) {
				btn = BTN_SELECT;
				id = BTN_ID_SELECT;
			} else if (code == CODE_MENU) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (code == CODE_MENU_ALT) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (code == CODE_L1) {
				btn = BTN_L1;
				id = BTN_ID_L1;
			} else if (code == CODE_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
			} else if (code == CODE_L3) {
				btn = BTN_L3;
				id = BTN_ID_L3;
			} else if (code == CODE_R1) {
				btn = BTN_R1;
				id = BTN_ID_R1;
			} else if (code == CODE_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
			} else if (code == CODE_R3) {
				btn = BTN_R3;
				id = BTN_ID_R3;
			} else if (code == CODE_PLUS) {
				btn = BTN_PLUS;
				id = BTN_ID_PLUS;
			} else if (code == CODE_MINUS) {
				btn = BTN_MINUS;
				id = BTN_ID_MINUS;
			} else if (code == CODE_POWER) {
				btn = BTN_POWER;
				id = BTN_ID_POWER;
			} else if (code == CODE_POWEROFF) {
				btn = BTN_POWEROFF;
				id = BTN_ID_POWEROFF;
			} // nano-only
		} else if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) {
			int joy = event.jbutton.button;
			pressed = event.type == SDL_JOYBUTTONDOWN;
			// LOG_info("joy event: %i (%i)\n", joy,pressed);
			if (joy == JOY_UP) {
				btn = BTN_DPAD_UP;
				id = BTN_ID_DPAD_UP;
			} else if (joy == JOY_DOWN) {
				btn = BTN_DPAD_DOWN;
				id = BTN_ID_DPAD_DOWN;
			} else if (joy == JOY_LEFT) {
				btn = BTN_DPAD_LEFT;
				id = BTN_ID_DPAD_LEFT;
			} else if (joy == JOY_RIGHT) {
				btn = BTN_DPAD_RIGHT;
				id = BTN_ID_DPAD_RIGHT;
			} else if (joy == JOY_A) {
				btn = BTN_A;
				id = BTN_ID_A;
			} else if (joy == JOY_B) {
				btn = BTN_B;
				id = BTN_ID_B;
			} else if (joy == JOY_X) {
				btn = BTN_X;
				id = BTN_ID_X;
			} else if (joy == JOY_Y) {
				btn = BTN_Y;
				id = BTN_ID_Y;
			} else if (joy == JOY_START) {
				btn = BTN_START;
				id = BTN_ID_START;
			} else if (joy == JOY_SELECT) {
				btn = BTN_SELECT;
				id = BTN_ID_SELECT;
			} else if (joy == JOY_MENU) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (joy == JOY_MENU_ALT) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (joy == JOY_MENU_ALT2) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (joy == JOY_L1) {
				btn = BTN_L1;
				id = BTN_ID_L1;
			} else if (joy == JOY_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
			} else if (joy == JOY_L3) {
				btn = BTN_L3;
				id = BTN_ID_L3;
			} else if (joy == JOY_R1) {
				btn = BTN_R1;
				id = BTN_ID_R1;
			} else if (joy == JOY_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
			} else if (joy == JOY_R3) {
				btn = BTN_R3;
				id = BTN_ID_R3;
			} else if (joy == JOY_PLUS) {
				btn = BTN_PLUS;
				id = BTN_ID_PLUS;
			} else if (joy == JOY_MINUS) {
				btn = BTN_MINUS;
				id = BTN_ID_MINUS;
			} else if (joy == JOY_POWER) {
				btn = BTN_POWER;
				id = BTN_ID_POWER;
			}
		} else if (event.type == SDL_JOYHATMOTION) {
			int hats[4] = {-1, -1, -1, -1}; // -1=no change,0=up,1=down,2=left,3=right btn_ids
			int hat = event.jhat.value;
			// LOG_info("hat event: %i\n", hat);
			// TODO: safe to assume hats will always be the primary dpad?
			// TODO: this is literally a bitmask, make it one (oh, except there's 3 states...)
			switch (hat) {
			case SDL_HAT_UP:
				hats[0] = 1;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 0;
				break;
			case SDL_HAT_DOWN:
				hats[0] = 0;
				hats[1] = 1;
				hats[2] = 0;
				hats[3] = 0;
				break;
			case SDL_HAT_LEFT:
				hats[0] = 0;
				hats[1] = 0;
				hats[2] = 1;
				hats[3] = 0;
				break;
			case SDL_HAT_RIGHT:
				hats[0] = 0;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 1;
				break;
			case SDL_HAT_LEFTUP:
				hats[0] = 1;
				hats[1] = 0;
				hats[2] = 1;
				hats[3] = 0;
				break;
			case SDL_HAT_LEFTDOWN:
				hats[0] = 0;
				hats[1] = 1;
				hats[2] = 1;
				hats[3] = 0;
				break;
			case SDL_HAT_RIGHTUP:
				hats[0] = 1;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 1;
				break;
			case SDL_HAT_RIGHTDOWN:
				hats[0] = 0;
				hats[1] = 1;
				hats[2] = 0;
				hats[3] = 1;
				break;
			case SDL_HAT_CENTERED:
				hats[0] = 0;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 0;
				break;
			default:
				break;
			}

			for (id = 0; id < 4; id++) {
				int state = hats[id];
				btn = 1 << id;
				if (state == 0) {
					pad.is_pressed &= ~btn; // unset
					pad.just_repeated &= ~btn; // unset
					pad.just_released |= btn; // set
				} else if (state == 1 && (pad.is_pressed & btn) == BTN_NONE) {
					pad.just_pressed |= btn; // set
					pad.just_repeated |= btn; // set
					pad.is_pressed |= btn; // set
					pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
				}
			}
			btn = BTN_NONE; // already handled, force continue
		} else if (event.type == SDL_JOYAXISMOTION) {
			int axis = event.jaxis.axis;
			int val = event.jaxis.value;
			// LOG_info("axis: %i (%i)\n", axis,val);

			// triggers on tg5040
			if (axis == AXIS_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
				pressed = val > 0;
			} else if (axis == AXIS_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
				pressed = val > 0;
			}

			else if (axis == AXIS_LX) {
				pad.laxis.x = val;
				PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, val,
				              tick + PAD_REPEAT_DELAY);
			} else if (axis == AXIS_LY) {
				pad.laxis.y = val;
				PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, val, tick + PAD_REPEAT_DELAY);
			} else if (axis == AXIS_RX)
				pad.raxis.x = val;
			else if (axis == AXIS_RY)
				pad.raxis.y = val;

			// axis will fire off what looks like a release
			// before the first press but you can't release
			// a button that wasn't pressed
			if (!pressed && btn != BTN_NONE && !(pad.is_pressed & btn)) {
				// LOG_info("cancel: %i\n", axis);
				btn = BTN_NONE;
			}
		}
		// else if (event.type==SDL_QUIT) PWR_powerOff(); // added for macOS debug

		if (btn == BTN_NONE)
			continue;

		if (!pressed) {
			pad.is_pressed &= ~btn; // unset
			pad.just_repeated &= ~btn; // unset
			pad.just_released |= btn; // set
		} else if ((pad.is_pressed & btn) == BTN_NONE) {
			pad.just_pressed |= btn; // set
			pad.just_repeated |= btn; // set
			pad.is_pressed |= btn; // set
			pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
		}
	}

	if (lid.has_lid && PLAT_lidChanged(NULL))
		pad.just_released |= BTN_SLEEP;
}

/**
 * Checks if device should wake from sleep.
 *
 * Polls for wake button (BTN_POWER or BTN_MENU depending on platform).
 * Also checks lid state - if lid is closed, wake button is ignored.
 *
 * @return 1 if device should wake, 0 otherwise
 *
 * @note Consumes the wake button event to prevent double-triggering
 */
FALLBACK_IMPLEMENTATION int PLAT_shouldWake(void) {
	int lid_open = 1; // assume open by default
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open)
		return 1;


	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_KEYUP) {
			int code = event.key.keysym.scancode;
			if ((BTN_WAKE == BTN_POWER && code == CODE_POWER) ||
			    (BTN_WAKE == BTN_MENU && (code == CODE_MENU || code == CODE_MENU_ALT))) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open)
					return 0; // do it here so we eat the input
				return 1;
			}
		} else if (event.type == SDL_JOYBUTTONUP) {
			int joy = event.jbutton.button;
			if ((BTN_WAKE == BTN_POWER && joy == JOY_POWER) ||
			    (BTN_WAKE == BTN_MENU && (joy == JOY_MENU || joy == JOY_MENU_ALT))) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open)
					return 0; // do it here so we eat the input
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////
// Vibration - Rumble motor control
///////////////////////////////

// Vibration context with deferred state changes to minimize motor wear
static struct VIB_Context {
	int initialized;
	pthread_t pt;
	int queued_strength;
	int strength; // Current applied strength
} vib = {0};

/**
 * Vibration worker thread that applies deferred strength changes.
 *
 * Defers strength changes for 3 frames to prevent rapid on/off cycling
 * which can damage rumble motors. Runs continuously at ~60Hz.
 *
 * @param arg Unused thread argument
 * @return Never returns (infinite loop)
 */
static void* VIB_thread(void* arg) {
#define DEFER_FRAMES 3
	static int defer = 0;
	while (1) {
		SDL_Delay(17);
		if (vib.queued_strength != vib.strength) {
			if (defer < DEFER_FRAMES &&
			    vib.queued_strength ==
			        0) { // minimize vacillation between 0 and some number (which this motor doesn't like)
				defer += 1;
				continue;
			}
			vib.strength = vib.queued_strength;
			defer = 0;

			PLAT_setRumble(vib.strength);
		}
	}
	return 0;
}

/**
 * Initializes the vibration subsystem.
 *
 * Starts the vibration worker thread. Call this before using vibration.
 */
void VIB_init(void) {
	vib.queued_strength = vib.strength = 0;
	pthread_create(&vib.pt, NULL, &VIB_thread, NULL);
	vib.initialized = 1;
}

/**
 * Shuts down the vibration subsystem.
 *
 * Stops vibration and terminates the worker thread.
 * Safe to call even if not initialized.
 */
void VIB_quit(void) {
	if (!vib.initialized)
		return;

	VIB_setStrength(0);
	pthread_cancel(vib.pt);
	pthread_join(vib.pt, NULL);
}

/**
 * Queues a vibration strength change.
 *
 * Change is deferred by 3 frames to prevent rapid motor cycling.
 * No-op if strength hasn't changed.
 *
 * @param strength Vibration strength (0=off, higher=stronger)
 */
void VIB_setStrength(int strength) {
	if (vib.queued_strength == strength)
		return;
	vib.queued_strength = strength;
}

/**
 * Gets the current applied vibration strength.
 *
 * @return Current strength (not queued strength)
 */
int VIB_getStrength(void) {
	return vib.strength;
}

///////////////////////////////
// Power management - Battery, sleep, brightness, volume
///////////////////////////////

// Overlay surface for fallback implementation (used if platform doesn't provide its own)
static SDL_Surface* fallback_overlay = NULL;

/**
 * Fallback overlay initialization for simple platforms.
 *
 * Creates a standard SDL surface for the battery warning overlay.
 * Platforms with hardware overlays (e.g., rg35xx) provide their own implementation.
 *
 * @return SDL surface for overlay
 */
FALLBACK_IMPLEMENTATION SDL_Surface* PLAT_initOverlay(void) {
	int overlay_size = DP(ui.pill_height);
	fallback_overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, overlay_size, overlay_size, 16,
	                                        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000); // ARGB
	return fallback_overlay;
}

/**
 * Fallback overlay cleanup.
 */
FALLBACK_IMPLEMENTATION void PLAT_quitOverlay(void) {
	if (fallback_overlay) {
		SDL_FreeSurface(fallback_overlay);
		fallback_overlay = NULL;
	}
}

/**
 * Fallback overlay enable/disable (no-op for software compositing).
 *
 * @param enable Unused - software overlays are always composited
 */
FALLBACK_IMPLEMENTATION void PLAT_enableOverlay(int enable) {
	(void)enable; // Overlay composited in software, no hardware control needed
}

/**
 * Initializes the low battery warning overlay.
 *
 * Creates an overlay surface showing a low battery icon.
 * The overlay is toggled on/off based on battery charge level.
 */
static void PWR_initOverlay(void) {
	// setup surface
	pwr.overlay = PLAT_initOverlay();

	// draw battery
	SDLX_SetAlpha(gfx.assets, 0, 0);
	GFX_blitAsset(ASSET_BLACK_PILL, NULL, pwr.overlay, NULL);
	SDLX_SetAlpha(gfx.assets, SDL_SRCALPHA, 0);
	GFX_blitBattery(pwr.overlay, NULL);
}

/**
 * Updates battery charging state and charge level.
 *
 * Queries platform for current battery status and updates overlay
 * visibility based on charge level and warning state.
 */
static void PWR_updateBatteryStatus(void) {
	PLAT_getBatteryStatus(&pwr.is_charging, &pwr.charge);
	PLAT_enableOverlay(pwr.should_warn && pwr.charge <= PWR_LOW_CHARGE);
}

/**
 * Battery monitoring worker thread.
 *
 * Polls battery status every 5 seconds and updates the UI overlay.
 * Runs continuously in the background.
 *
 * @param arg Unused thread argument
 * @return Never returns (infinite loop)
 */
static void* PWR_monitorBattery(void* arg) {
	while (1) {
		// TODO: the frequency of checking could depend on whether
		// we're in game (less frequent) or menu (more frequent)
		sleep(5);
		PWR_updateBatteryStatus();
	}
	return NULL;
}

/**
 * Initializes the power management subsystem.
 *
 * Sets up battery monitoring, initializes overlay, starts background thread.
 * Configures default power management flags (sleep/poweroff enabled).
 */
void PWR_init(void) {
	pwr.can_sleep = 1;
	pwr.can_poweroff = 1;
	pwr.can_autosleep = 1;

	pwr.requested_sleep = 0;
	pwr.requested_wake = 0;

	pwr.should_warn = 0;
	pwr.charge = PWR_LOW_CHARGE;

	PWR_initOverlay();

	PWR_updateBatteryStatus();
	pthread_create(&pwr.battery_pt, NULL, &PWR_monitorBattery, NULL);
	pwr.initialized = 1;
}

/**
 * Shuts down the power management subsystem.
 *
 * Terminates battery monitoring thread and frees overlay resources.
 * Safe to call even if not initialized.
 */
void PWR_quit(void) {
	if (!pwr.initialized)
		return;

	PLAT_quitOverlay();

	// cancel battery thread
	pthread_cancel(pwr.battery_pt);
	pthread_join(pwr.battery_pt, NULL);
}

/**
 * Enables or disables low battery warning overlay.
 *
 * @param enable 1 to show warning when battery low, 0 to hide
 */
void PWR_warn(int enable) {
	pwr.should_warn = enable;
	PLAT_enableOverlay(pwr.should_warn && pwr.charge <= PWR_LOW_CHARGE);
}

/**
 * Checks if a button press should be ignored during settings adjustment.
 *
 * Returns true for PLUS/MINUS buttons when brightness/volume overlay is showing.
 * Prevents these buttons from affecting the game/app while adjusting settings.
 *
 * @param btn Button that was pressed
 * @param show_setting Current settings overlay state (1=brightness, 2=volume)
 * @return 1 if button should be ignored, 0 otherwise
 */
int PWR_ignoreSettingInput(int btn, int show_setting) {
	return show_setting && (btn == BTN_MOD_PLUS || btn == BTN_MOD_MINUS);
}

/**
 * Main power management update function, called each frame.
 *
 * Handles:
 * - Autosleep after 30 seconds of inactivity
 * - Manual sleep button (lid close or sleep button)
 * - Power button hold detection (1 second to power off)
 * - Brightness/volume overlay display timing
 * - Charging state change detection
 * - Display refresh dirty flag management
 *
 * @param _dirty Pointer to dirty flag (set to 1 when display needs refresh)
 * @param _show_setting Pointer to settings overlay state (0/1/2)
 * @param before_sleep Callback invoked before entering sleep
 * @param after_sleep Callback invoked after waking from sleep
 *
 * @note All pointer parameters may be NULL if not needed
 */
void PWR_update(int* _dirty, int* _show_setting, PWR_callback_t before_sleep,
                PWR_callback_t after_sleep) {
	int dirty = _dirty ? *_dirty : 0;
	int show_setting = _show_setting ? *_show_setting : 0;

	static uint32_t last_input_at = 0; // timestamp of last input (autosleep)
	static uint32_t checked_charge_at = 0; // timestamp of last time checking charge
	static uint32_t setting_shown_at = 0; // timestamp when settings started being shown
	static uint32_t power_pressed_at = 0; // timestamp when power button was just pressed
	static uint32_t mod_unpressed_at =
	    0; // timestamp of last time settings modifier key was NOT down
	static uint32_t was_muted = (uint32_t)-1;
	if (was_muted == (uint32_t)-1)
		was_muted = GetMute();

	static int was_charging = -1;
	if (was_charging == -1)
		was_charging = pwr.is_charging;

	uint32_t now = SDL_GetTicks();
	if (was_charging || PAD_anyPressed() || last_input_at == 0)
		last_input_at = now;

#define CHARGE_DELAY 1000
	if (dirty || now - checked_charge_at >= CHARGE_DELAY) {
		int is_charging = pwr.is_charging;
		if (was_charging != is_charging) {
			was_charging = is_charging;
			dirty = 1;
		}
		checked_charge_at = now;
	}

	if (PAD_justReleased(BTN_POWEROFF) || (power_pressed_at && now - power_pressed_at >= 1000)) {
		if (before_sleep)
			before_sleep();
		PWR_powerOff();
	}

	if (PAD_justPressed(BTN_POWER)) {
		power_pressed_at = now;
	}

#define SLEEP_DELAY 30000 // 30 seconds
	if (now - last_input_at >= SLEEP_DELAY && PWR_preventAutosleep())
		last_input_at = now;

	if (pwr.requested_sleep || // hardware requested sleep
	    now - last_input_at >= SLEEP_DELAY || // autosleep
	    (pwr.can_sleep && PAD_justReleased(BTN_SLEEP)) // manual sleep
	) {
		pwr.requested_sleep = 0;
		if (before_sleep)
			before_sleep();
		PWR_fauxSleep();
		if (after_sleep)
			after_sleep();

		last_input_at = now = SDL_GetTicks();
		power_pressed_at = 0;
		dirty = 1;
	}

	// TODO: only delay hiding setting changes if that setting didn't require a modifier button be held, otherwise release as soon as modifier is released

	int delay_settings =
	    BTN_MOD_BRIGHTNESS ==
	    BTN_MENU; // when both volume and brighness require a modifier hide settings as soon as it is released
#define SETTING_DELAY 500
	if (show_setting && (now - setting_shown_at >= SETTING_DELAY || !delay_settings) &&
	    !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		show_setting = 0;
		dirty = 1;
	}

	if (!show_setting && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		mod_unpressed_at = now; // this feels backwards but is correct
	}

#define MOD_DELAY 250
	if (((PAD_isPressed(BTN_MOD_VOLUME) || PAD_isPressed(BTN_MOD_BRIGHTNESS)) &&
	     (!delay_settings || now - mod_unpressed_at >= MOD_DELAY)) ||
	    ((!BTN_MOD_VOLUME || !BTN_MOD_BRIGHTNESS) &&
	     (PAD_justRepeated(BTN_MOD_PLUS) || PAD_justRepeated(BTN_MOD_MINUS)))) {
		setting_shown_at = now;
		if (PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
			show_setting = 1;
		} else {
			show_setting = 2;
		}
	}

	int muted = GetMute();
	if ((uint32_t)muted != was_muted) {
		was_muted = muted;
		show_setting = 2;
		setting_shown_at = now;
	}

	if (show_setting)
		dirty = 1; // shm is slow or keymon is catching input on the next frame
	if (_dirty)
		*_dirty = dirty;
	if (_show_setting)
		*_show_setting = show_setting;
}

/**
 * Disables manual sleep (sleep button/lid close).
 *
 * Prevents device from sleeping when sleep button is pressed.
 * Autosleep and power off may still occur.
 */
void PWR_disableSleep(void) {
	pwr.can_sleep = 0;
}
/**
 * Re-enables manual sleep.
 */
void PWR_enableSleep(void) {
	pwr.can_sleep = 1;
}

/**
 * Disables power off functionality.
 *
 * Prevents device from powering off completely.
 * Useful during critical operations.
 */
void PWR_disablePowerOff(void) {
	pwr.can_poweroff = 0;
}

/**
 * Powers off the device.
 *
 * Displays "powering off" message and calls platform power off.
 * Checks for auto-resume quicksave and adjusts message accordingly.
 * Only works if power off hasn't been disabled.
 *
 * @note This function does not return if power off succeeds
 */
void PWR_powerOff(void) {
	if (pwr.can_poweroff) {
		int w = FIXED_WIDTH;
		int h = FIXED_HEIGHT;
		int p = FIXED_PITCH;
		if (GetHDMI()) {
			w = HDMI_WIDTH;
			h = HDMI_HEIGHT;
			p = HDMI_PITCH;
		}
		gfx.screen = GFX_resize(w, h, p);

		char* msg;
		if (HAS_POWER_BUTTON || HAS_POWEROFF_BUTTON)
			msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npowering off" : "Powering off";
		else
			msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npower off now" : "Power off now";

		// LOG_info("PWR_powerOff %s (%ix%i)\n", gfx.screen, gfx.screen->w, gfx.screen->h);

		PLAT_clearVideo(gfx.screen);
		GFX_blitMessage_DP(font.large, msg, gfx.screen, 0, 0, ui.screen_width, ui.screen_height);
		GFX_flip(gfx.screen);
		PLAT_powerOff();
	}
}

/**
 * Enters sleep mode (low power state).
 *
 * On HDMI: Clears screen
 * On device screen: Mutes audio, disables backlight
 * Pauses keymon daemon and syncs filesystem.
 */
static void PWR_enterSleep(void) {
	SDL_PauseAudio(1);
	if (GetHDMI()) {
		PLAT_clearVideo(gfx.screen);
		PLAT_flip(gfx.screen, 0);
	} else {
		SetRawVolume(MUTE_VOLUME_RAW);
		PLAT_enableBacklight(0);
	}
	system("killall -STOP keymon.elf");

	sync();
}

/**
 * Exits sleep mode and restores normal operation.
 *
 * Restores backlight, volume, resumes keymon, and unpauses audio.
 */
static void PWR_exitSleep(void) {
	system("killall -CONT keymon.elf");
	if (GetHDMI()) {
		// buh
	} else {
		PLAT_enableBacklight(1);
		SetVolume(GetVolume());
	}
	SDL_PauseAudio(0);

	sync();
}

/**
 * Waits in sleep mode until wake condition occurs.
 *
 * Polls wake button every 200ms. If sleeping for > 2 minutes and not
 * charging, powers off automatically. Charging extends wait time.
 */
static void PWR_waitForWake(void) {
	uint32_t sleep_ticks = SDL_GetTicks();
	while (!PAD_wake()) {
		if (pwr.requested_wake) {
			pwr.requested_wake = 0;
			break;
		}
		SDL_Delay(200);
		if (pwr.can_poweroff &&
		    SDL_GetTicks() - sleep_ticks >= 120000) { // increased to two minutes
			if (pwr.is_charging)
				sleep_ticks += 60000; // check again in a minute
			else
				PWR_powerOff();
		}
	}

	return;
}

/**
 * Performs a "fake sleep" by entering and exiting sleep mode.
 *
 * Clears screen, resets input, enters sleep, waits for wake,
 * then exits sleep and resets input again. This is the main
 * sleep function called by applications.
 */
void PWR_fauxSleep(void) {
	GFX_clear(gfx.screen);
	PAD_reset();
	PWR_enterSleep();
	PWR_waitForWake();
	PWR_exitSleep();
	PAD_reset();
}

/**
 * Disables automatic sleep after 30 seconds of inactivity.
 */
void PWR_disableAutosleep(void) {
	pwr.can_autosleep = 0;
}

/**
 * Re-enables automatic sleep.
 */
void PWR_enableAutosleep(void) {
	pwr.can_autosleep = 1;
}

/**
 * Checks if autosleep should be prevented.
 *
 * Autosleep is prevented when:
 * - Device is charging
 * - Autosleep has been disabled
 * - HDMI is connected
 *
 * @return 1 if autosleep should be prevented, 0 otherwise
 */
int PWR_preventAutosleep(void) {
	return pwr.is_charging || !pwr.can_autosleep || GetHDMI();
}

/**
 * Checks if device is currently charging.
 *
 * @return 1 if charging, 0 otherwise
 *
 * @note Value updated every 5 seconds by battery monitoring thread
 */
int PWR_isCharging(void) {
	return pwr.is_charging;
}

/**
 * Gets current battery charge level.
 *
 * @return Charge percentage (10-100 in 10-20% increments)
 *
 * @note Value updated every 5 seconds by battery monitoring thread
 */
int PWR_getBattery(void) { // 10-100 in 10-20% fragments
	return pwr.charge;
}

///////////////////////////////
// Platform utility functions
///////////////////////////////

/**
 * Sets the system date and time.
 *
 * Executes date command and syncs to hardware clock.
 * This is a temporary implementation - platforms may override.
 *
 * @param y Year
 * @param m Month (1-12)
 * @param d Day (1-31)
 * @param h Hour (0-23)
 * @param i Minute (0-59)
 * @param s Second (0-59)
 * @return Always returns 0
 */
int PLAT_setDateTime(int y, int m, int d, int h, int i, int s) {
	char cmd[512];
	sprintf(cmd, "date -s '%d-%d-%d %d:%d:%d'; hwclock --utc -w", y, m, d, h, i, s);
	system(cmd);
	return 0; // why does this return an int?
}
