/**
 * ui_layout.c - Display Points (DP) UI layout system implementation
 *
 * Implements resolution-independent UI layout calculations based on screen PPI.
 * This module is extracted from api.c to enable proper unit testing of the
 * complex layout algorithm.
 */

#include <math.h>

#include "defines.h"
#include "log.h"
#include "ui_layout.h"

// External reference to gfx_dp_scale (defined in api.c)
extern float gfx_dp_scale;

// DP macro (convert display points to pixels)
// NOTE: This is duplicated from api.h to avoid circular includes.
// Must stay synchronized: if DP rounding logic changes in api.h, update here too.
#define DP(x) ((int)((x) * gfx_dp_scale + 0.5f))

// Runtime-calculated UI layout parameters
UI_Layout ui = {
    .screen_width = 320, // Default (updated by UI_initLayout)
    .screen_height = 240, // Default (updated by UI_initLayout)
    .screen_width_px = 640, // Default: 320dp * 2.0 scale = 640px (updated by UI_initLayout)
    .screen_height_px = 480, // Default: 240dp * 2.0 scale = 480px (updated by UI_initLayout)
    .pill_height = 30,
    .pill_height_px = 60, // Default: 30dp * 2.0 scale = 60px (updated by UI_initLayout)
    .row_count = 6,
    .padding = 10,
    .edge_padding = 10,
    .edge_padding_px = 20, // Default: 10dp * 2.0 scale = 20px (updated by UI_initLayout)
    .button_size = 20, // (30 * 2) / 3 = 20 for 30dp pill (button icons)
    .button_margin = 5,
    .option_size = 22, // (30 * 3) / 4 = 22 for 30dp pill (submenu rows)
    .option_size_px = 44, // Default: 22dp * 2.0 scale = 44px (updated by UI_initLayout)
    .text_offset_px = 0, // Set by GFX_init after font load
    .option_offset_px = 0, // Set by GFX_init after font load
    .option_value_offset_px = 0, // Set by GFX_init after font load
    .button_text_offset_px = 0, // Set by GFX_init after font load
    .button_label_offset_px = 0, // Set by GFX_init after font load
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
 *   2. Calculate raw dp_scale: ppi / 120.0 (120 DPI baseline)
 *   3. Apply optional SCALE_MODIFIER if defined in platform.h
 *
 * Row Fitting Algorithm:
 *   - Search from maximum possible rows down to 1 (prefer more content)
 *   - For each row count, calculate pill height to fill available space
 *   - Prefer configurations with even-pixel pill heights for cleaner rendering
 *   - Select first configuration where pill fits 28-32dp range
 *
 * @param screen_width Screen width in physical pixels
 * @param screen_height Screen height in physical pixels
 * @param diagonal_inches Physical screen diagonal in inches (from platform.h)
 *
 * @note Sets global gfx_dp_scale and ui struct values
 * @note Must be called before any DP() macro usage
 */
void UI_initLayout(int screen_width, int screen_height, float diagonal_inches) {
	LOG_debug("UI_initLayout: Starting layout calculation (%dx%d @ %.2f\")", screen_width,
	          screen_height, diagonal_inches);

	// Calculate PPI and dp_scale
	float diagonal_px = sqrtf((float)(screen_width * screen_width + screen_height * screen_height));
	float ppi = diagonal_px / diagonal_inches;
	float raw_dp_scale = ppi / 120.0f;
	LOG_debug("UI_initLayout: Calculated PPI=%.0f, raw_dp_scale=%.2f", ppi, raw_dp_scale);

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

	// ============================================================================
	// PIXEL-ACCURATE ROW FITTING
	// ============================================================================
	// We calculate in PIXEL space (not DP space) to prevent rounding accumulation.
	//
	// WHY THIS MATTERS:
	//   The old approach calculated row count in DP, then converted to pixels for
	//   layout. Each DP() conversion rounds to the nearest integer. When laying out
	//   multiple rows (e.g., y = DP(edge_padding) + DP(row * pill_height)), these
	//   rounding errors accumulate and can cause the last content row to overlap
	//   with the footer row.
	//
	// THE FIX:
	//   1. Convert DP bounds to pixels ONCE at the start
	//   2. Calculate row count and pill size entirely in pixels
	//   3. Store exact pixel values in ui.*_px fields for layout
	//   4. Convert back to DP only for proportional calculations
	//
	// This ensures layout uses exact pixel arithmetic with no rounding drift.
	// ============================================================================

	// Convert DP constraints to pixel space (once)
	int edge_padding_px = DP(edge_padding);
	int min_pill_px = DP(MIN_PILL);
	int max_pill_px = DP(MAX_PILL);
	int available_px = screen_height - (2 * edge_padding_px);

	// Search for the best row count
	// Priority: 1) Most rows (prefer more content), 2) Even pixels (cleaner rendering)
	int best_pill_px = 0;
	int best_rows = 0;
	int best_is_even = 0;

	// Calculate maximum possible rows (upper bound for search)
	int max_search_rows = (available_px / min_pill_px);
	if (max_search_rows < 1)
		max_search_rows = 1;

	// Search from maximum rows down to 1 (prefer more content)
	for (int content_rows = max_search_rows; content_rows >= 1; content_rows--) {
		int total_rows = content_rows + 1; // +1 for footer row
		int pill_px = available_px / total_rows;

		// Skip pills outside acceptable range (with ±2px tolerance for edge cases)
		if (pill_px < min_pill_px - 2 || pill_px > max_pill_px + 2)
			continue;

		int is_even = (pill_px % 2 == 0);
		int in_range = (pill_px >= min_pill_px && pill_px <= max_pill_px);

		// Scoring logic: prefer in-range, then even pixels
		if (in_range && is_even) {
			// Perfect: in range AND even pixels
			best_pill_px = pill_px;
			best_rows = content_rows;
			best_is_even = 1;
			LOG_info("Row calc: %d rows → %dpx (even, in range) ✓\n", content_rows, pill_px);
			break; // Can't do better than this
		} else if (in_range && best_rows == 0) {
			// Good: in range but odd pixels (keep as backup)
			best_pill_px = pill_px;
			best_rows = content_rows;
			LOG_info("Row calc: %d rows → %dpx (odd, in range) - backup\n", content_rows, pill_px);
		} else if (!in_range && best_rows == 0) {
			// Acceptable: outside range but within tolerance (last resort)
			best_pill_px = pill_px;
			best_rows = content_rows;
			LOG_info("Row calc: %d rows → %dpx (outside range) - fallback\n", content_rows,
			         pill_px);
		}
	}

	// Verify we found a valid configuration
	if (best_rows == 0) {
		// Emergency fallback (should never happen with reasonable MIN_PILL)
		best_pill_px = min_pill_px;
		best_rows = 1;
		LOG_warn("Row calc: EMERGENCY FALLBACK to %dpx, 1 row\n", min_pill_px);
	} else if (best_is_even) {
		LOG_info("Row calc: Using even-pixel configuration\n");
	} else {
		LOG_info("Row calc: Using odd-pixel fallback (no even option available)\n");
	}

	// ============================================================================
	// STORE CALCULATED VALUES
	// ============================================================================
	// Store both pixel and DP values for different use cases:
	//   - *_px fields: Use for exact layout positioning
	//   - non-px fields: Use for proportional calculations
	// ============================================================================

	int screen_height_dp = (int)(screen_height / gfx_dp_scale + 0.5f);

	// Screen dimensions
	ui.screen_width = (int)(screen_width / gfx_dp_scale + 0.5f);
	ui.screen_height = screen_height_dp;
	ui.screen_width_px = screen_width;
	ui.screen_height_px = screen_height;

	// Layout values - store BOTH pixel and DP versions
	ui.pill_height_px = best_pill_px; // EXACT pixels for layout positioning
	ui.pill_height = (int)(best_pill_px / gfx_dp_scale + 0.5f); // DP for proportional calculations
	ui.row_count = best_rows;
	ui.edge_padding = edge_padding;
	ui.edge_padding_px = edge_padding_px; // EXACT pixels for layout positioning
	ui.padding = internal_padding;

	// Verify layout is correct (no overlap between content and footer)
	int content_bottom_px = edge_padding_px + (ui.row_count * best_pill_px);
	int footer_top_px = screen_height - edge_padding_px - best_pill_px;
	int gap_px = footer_top_px - content_bottom_px;

	LOG_info("Row calc: FINAL → %d rows, %ddp (%dpx) pills, %dpx gap\n", ui.row_count,
	         ui.pill_height, ui.pill_height_px, gap_px);

	// ============================================================================
	// DERIVED PROPORTIONAL SIZES
	// ============================================================================
	// Calculate secondary UI element sizes proportionally from pill_height.
	// These use DP values for calculation, then convert to pixels once.
	// We prefer even pixel sizes for cleaner rendering.
	// ============================================================================

	// Button icons (smaller square elements within pills)
	ui.button_size = (ui.pill_height * 2) / 3; // ~20dp for 30dp pill
	int button_px = DP(ui.button_size);
	if (button_px % 2 != 0)
		ui.button_size++; // Adjust to even pixels for cleaner rendering

	ui.button_margin = (ui.pill_height - ui.button_size) / 2; // Center button in pill
	ui.button_padding = (ui.pill_height * 2) / 5; // ~12dp for 30dp pill

	// Submenu option rows (smaller than main pills, used in settings menus)
	ui.option_size = (ui.pill_height * 3) / 4; // ~22dp for 30dp pill
	ui.option_size_px = DP(ui.option_size);
	if (ui.option_size_px % 2 != 0) {
		ui.option_size++; // Adjust to even pixels for cleaner rendering
		ui.option_size_px = DP(ui.option_size); // Recalculate after adjustment
	}


	// Settings indicators
	ui.settings_size = ui.pill_height / 8; // ~4dp for 30dp pill
	ui.settings_width = 80; // Fixed 80dp width (hardware icons need consistent spacing)

	LOG_info("UI_initLayout: %dx%d @ %.2f\" → PPI=%.0f, dp_scale=%.2f\n", screen_width,
	         screen_height, diagonal_inches, ppi, gfx_dp_scale);
	LOG_info("UI_initLayout: pill=%ddp, rows=%d, padding=%ddp, edge_padding=%ddp\n", ui.pill_height,
	         ui.row_count, ui.padding, ui.edge_padding);
}
