/**
 * ui_layout.h - Display Points (DP) UI layout system
 *
 * Provides resolution-independent UI layout calculations based on screen PPI.
 * Automatically determines optimal pill height, row count, and spacing to fill
 * the screen perfectly across devices with different resolutions and sizes.
 *
 * Key Features:
 * - PPI-based scaling (120 DPI baseline)
 * - Pixel-accurate row fitting to prevent rounding accumulation
 * - Even-pixel preference for cleaner rendering
 * - Platform-specific modifiers (SCALE_MODIFIER, EDGE_PADDING)
 */

#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

/**
 * Runtime-calculated UI layout parameters.
 *
 * These values are computed by UI_initLayout() based on screen dimensions
 * to optimally fill the display without per-platform manual configuration.
 *
 * IMPORTANT: This struct contains BOTH Display Point (DP) values and pixel values.
 * Understanding when to use each is critical:
 *
 * DP VALUES (int foo):
 *   - Used for PROPORTIONAL calculations (e.g., button_size = pill_height * 2/3)
 *   - Each DP() conversion involves rounding, so repeated conversions accumulate error
 *   - Example: ui.pill_height, ui.edge_padding, ui.option_size
 *
 * PIXEL VALUES (int foo_px):
 *   - Used for EXACT LAYOUT POSITIONING (e.g., y = edge_padding_px + row * pill_height_px)
 *   - These are pre-calculated ONCE to avoid rounding accumulation
 *   - Prevents overlap bugs caused by repeated DP() conversions
 *   - Example: ui.pill_height_px, ui.edge_padding_px, ui.option_size_px
 *
 * USAGE PATTERN:
 *   // WRONG - repeated DP() calls accumulate rounding error:
 *   for (int row = 0; row < 6; row++) {
 *       int y = DP(ui.edge_padding) + DP(row * ui.pill_height);  // BAD!
 *   }
 *
 *   // CORRECT - use pre-calculated pixel values for layout:
 *   for (int row = 0; row < 6; row++) {
 *       int y = ui.edge_padding_px + (row * ui.pill_height_px);  // GOOD!
 *   }
 *
 *   // CORRECT - use DP values for proportional calculations:
 *   int icon_size = (ui.pill_height * 2) / 3;  // GOOD! (then convert once: DP(icon_size))
 */
typedef struct UI_Layout {
	// Screen dimensions
	int screen_width; // Screen width in dp (for proportional layout)
	int screen_height; // Screen height in dp (for proportional layout)
	int screen_width_px; // Screen width in pixels (cached for convenience)
	int screen_height_px; // Screen height in pixels (cached for convenience)

	// Main menu pills (the large selectable rows)
	int pill_height; // Pill height in dp (for proportional calculations like icon sizing)
	int pill_height_px; // Pill height in EXACT pixels (for row positioning - avoids DP rounding drift)
	int row_count; // Number of visible content rows (not including footer)

	// Spacing and padding
	int padding; // Internal spacing between UI elements in dp
	int edge_padding; // Distance from screen edges in dp (reduced on bezel devices)
	int edge_padding_px; // Distance from screen edges in EXACT pixels (for positioning - avoids DP rounding drift)

	// Button elements (action hints, icons)
	int button_size; // Size of button icons in dp
	int button_margin; // Margin around buttons in dp
	int button_padding; // Padding inside buttons in dp

	// Submenu option rows (smaller than main pills, used in settings menus)
	int option_size; // Option row height in dp (for proportional calculations)
	int option_size_px; // Option row height in EXACT pixels (for positioning - avoids DP rounding drift)

	// Settings indicators (brightness, volume sliders)
	int settings_size; // Size of setting indicators in dp
	int settings_width; // Width of setting indicators in dp

	// Pixel-perfect text centering offsets (computed from font metrics after font load)
	// These are Y offsets in pixels to vertically center text within their respective containers
	int text_offset_px; // Y offset to center font.large in pill_height_px
	int option_offset_px; // Y offset to center font.medium in option_size_px
	int option_value_offset_px; // Y offset to center font.small in option_size_px (right-aligned values)
	int button_text_offset_px; // Y offset to center font.small in button_size (action hints)
	int button_label_offset_px; // Y offset to center font.tiny in button_size (MENU, POWER labels)
} UI_Layout;

extern UI_Layout ui;

/**
 * Initializes the DP scaling system and UI layout.
 *
 * Calculates dp_scale from screen PPI, then computes optimal pill height,
 * row count, and padding to fill the screen perfectly.
 *
 * @param screen_width Physical screen width in pixels
 * @param screen_height Physical screen height in pixels
 * @param diagonal_inches Physical screen diagonal in inches
 *
 * @note Called automatically from GFX_init()
 */
void UI_initLayout(int screen_width, int screen_height, float diagonal_inches);

#endif // UI_LAYOUT_H
