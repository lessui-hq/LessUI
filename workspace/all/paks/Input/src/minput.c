/**
 * minput.c - Interactive button mapping visualization
 *
 * Visual input tester that displays all available buttons on the current
 * device and highlights them in real-time as they are pressed. Useful for
 * verifying button mappings and troubleshooting input issues.
 *
 * Features:
 * - Automatically detects available buttons (L2/R2, L3/R3, volume, power, menu)
 * - Visual layout mimics physical controller layout
 * - Real-time feedback - buttons highlight when pressed
 * - Exit with SELECT + START combination
 *
 * The layout adapts based on platform capabilities defined in platform.h:
 * - Shoulder buttons (L1, L2, R1, R2)
 * - Face buttons (A, B, X, Y)
 * - D-pad (Up, Down, Left, Right)
 * - Meta buttons (Start, Select)
 * - Analog stick buttons (L3, R3)
 * - System buttons (Volume +/-, Menu, Power)
 */

#include <msettings.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "api.h"
#include "defines.h"
#include "sdl.h"
#include "utils.h"

/**
 * Calculates the display width needed for a button label.
 *
 * Single-character and two-character labels fit within a square button.
 * Longer labels (like "VOL. +", "MENU") need wider pill-shaped buttons.
 *
 * @param label Button label text
 * @return Width in pixels (pre-scaled)
 */
static int getButtonWidth(char* label) {
	int w = 0;

	if (strlen(label) <= 2)
		w = DP(ui.button_size);
	else {
		SDL_Surface* text = TTF_RenderUTF8_Blended(font.tiny, label, COLOR_BUTTON_TEXT);
		w = DP(ui.button_size) + text->w;
		SDL_FreeSurface(text);
	}
	return w;
}

/**
 * Renders a button visual with label, showing pressed/unpressed state.
 *
 * Uses different rendering approaches based on label length:
 * - 1-2 chars: Square button with centered text
 * - 3+ chars: Pill-shaped button with wider layout
 *
 * Pressed state changes the button appearance from a hole to a raised button.
 *
 * @param label Button label text
 * @param dst Destination surface (screen)
 * @param pressed 1 if button is currently pressed, 0 otherwise
 * @param x X position (pre-scaled)
 * @param y Y position (pre-scaled)
 * @param w Width override (0 to auto-calculate)
 */
static void blitButton(char* label, SDL_Surface* dst, int pressed, int x, int y, int w) {
	SDL_Rect point = {x, y};
	SDL_Surface* text;

	int len = strlen(label);
	if (len <= 2) {
		// Short labels: use square button with larger font
		text =
		    TTF_RenderUTF8_Blended(len == 2 ? font.small : font.medium, label, COLOR_BUTTON_TEXT);
		GFX_blitAsset(pressed ? ASSET_BUTTON : ASSET_HOLE, NULL, dst, &point);
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){point.x + (DP(ui.button_size) - text->w) / 2,
		                            point.y + (DP(ui.button_size) - text->h) / 2});
	} else {
		// Long labels: use pill-shaped button with smaller font
		text = TTF_RenderUTF8_Blended(font.tiny, label, COLOR_BUTTON_TEXT);
		w = w ? w : DP(ui.button_size) / 2 + text->w;
		GFX_blitPill(pressed ? ASSET_BUTTON : ASSET_HOLE, dst,
		             &(SDL_Rect){point.x, point.y, w, DP(ui.button_size)});
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){point.x + (w - text->w) / 2,
		                            point.y + (DP(ui.button_size) - text->h) / 2, text->w,
		                            text->h});
	}

	SDL_FreeSurface(text);
}

/**
 * Main entry point for the input tester application.
 *
 * Displays an interactive button map showing all available buttons on the
 * current device. Buttons highlight as they are pressed. The layout adapts
 * based on which buttons are defined in platform.h.
 *
 * @return EXIT_SUCCESS on normal exit
 */
int main(int argc, char* argv[]) {
	PWR_setCPUSpeed(CPU_SPEED_MENU);

	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	InitSettings();

	// Detect which buttons are available on this platform
	// Each button can be mapped via scancode, keycode, joystick button, or axis
	int has_L2 =
	    (BUTTON_L2 != BUTTON_NA || CODE_L2 != CODE_NA || JOY_L2 != JOY_NA || AXIS_L2 != AXIS_NA);
	int has_R2 =
	    (BUTTON_R2 != BUTTON_NA || CODE_R2 != CODE_NA || JOY_R2 != JOY_NA || AXIS_R2 != AXIS_NA);
	int has_L3 = (BUTTON_L3 != BUTTON_NA || CODE_L3 != CODE_NA || JOY_L3 != JOY_NA);
	int has_R3 = (BUTTON_R3 != BUTTON_NA || CODE_R3 != CODE_NA || JOY_R3 != JOY_NA);

	int has_volume = (BUTTON_PLUS != BUTTON_NA || CODE_PLUS != CODE_NA || JOY_PLUS != JOY_NA);
	int has_power = HAS_POWER_BUTTON;
	int has_menu = HAS_MENU_BUTTON;
	int has_both = (has_power && has_menu);

	// Adjust vertical offset if L3/R3 not present (reclaim space)
	int oy = DP(ui.edge_padding);
	if (!has_L3 && !has_R3)
		oy += DP(ui.pill_height);

	int quit = 0;
	int dirty = 1;

	// Main event loop - redraw on any button press/release
	while (!quit) {
		PAD_poll();

		// Mark dirty if any input detected
		if (PAD_anyPressed() || PAD_anyJustReleased())
			dirty = 1;

		// Exit on SELECT + START combination
		if (PAD_isPressed(BTN_SELECT) && PAD_isPressed(BTN_START))
			quit = 1;

		// Redraw screen if anything changed
		if (dirty) {
			GFX_clear(screen);

			///////////////////////////////
			// Left shoulder buttons (L1, L2)
			///////////////////////////////
			{
				int x = DP(ui.button_margin + PADDING);
				int y = oy;
				int w = 0;
				int ox = 0;

				w = getButtonWidth("L1") + DP(ui.button_margin) * 2;
				ox = w;

				if (has_L2)
					w += getButtonWidth("L2") + DP(ui.button_margin);
				// Offset if L2 not present to maintain visual balance
				if (!has_L2)
					x += DP(ui.pill_height);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, w});

				blitButton("L1", screen, PAD_isPressed(BTN_L1), x + DP(ui.button_margin),
				           y + DP(ui.button_margin), 0);
				if (has_L2)
					blitButton("L2", screen, PAD_isPressed(BTN_L2), x + ox,
					           y + DP(ui.button_margin), 0);
			}

			///////////////////////////////
			// Right shoulder buttons (R1, R2)
			///////////////////////////////
			{
				int x = 0;
				int y = oy;
				int w = 0;
				int ox = 0;

				w = getButtonWidth("R1") + DP(ui.button_margin) * 2;
				ox = w;

				if (has_R2)
					w += getButtonWidth("R2") + DP(ui.button_margin);

				// Right-align the button group
				x = FIXED_WIDTH - w - DP(ui.button_margin + PADDING);
				if (!has_R2)
					x -= DP(ui.pill_height);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, w});

				// R2 comes first visually (left position), then R1
				blitButton(has_R2 ? "R2" : "R1", screen, PAD_isPressed(has_R2 ? BTN_R2 : BTN_R1),
				           x + DP(ui.button_margin), y + DP(ui.button_margin), 0);
				if (has_R2)
					blitButton("R1", screen, PAD_isPressed(BTN_R1), x + ox,
					           y + DP(ui.button_margin), 0);
			}

			///////////////////////////////
			// D-pad (Up, Down, Left, Right)
			///////////////////////////////
			{
				int x = DP(ui.edge_padding + ui.pill_height);
				int y = oy + DP(ui.pill_height * 2);
				int o = DP(ui.button_margin);

				// Vertical bar connecting Up and Down buttons
				SDL_FillRect(screen,
				             &(SDL_Rect){x, y + DP(ui.pill_height) / 2, DP(ui.pill_height),
				                         DP(ui.pill_height * 2)},
				             RGB_DARK_GRAY);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("U", screen, PAD_isPressed(BTN_DPAD_UP), x + o, y + o, 0);

				y += DP(ui.pill_height * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("D", screen, PAD_isPressed(BTN_DPAD_DOWN), x + o, y + o, 0);

				x -= DP(ui.pill_height);
				y -= DP(ui.pill_height);

				// Horizontal bar connecting Left and Right buttons
				SDL_FillRect(screen,
				             &(SDL_Rect){x + DP(ui.pill_height) / 2, y, DP(ui.pill_height * 2),
				                         DP(ui.pill_height)},
				             RGB_DARK_GRAY);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("L", screen, PAD_isPressed(BTN_DPAD_LEFT), x + o, y + o, 0);

				x += DP(ui.pill_height * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("R", screen, PAD_isPressed(BTN_DPAD_RIGHT), x + o, y + o, 0);
			}

			///////////////////////////////
			// Face buttons (A, B, X, Y)
			///////////////////////////////
			{
				int x = FIXED_WIDTH - DP(ui.edge_padding + ui.pill_height * 3) + DP(ui.pill_height);
				int y = oy + DP(ui.pill_height * 2);
				int o = DP(ui.button_margin);

				// X (top)
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("X", screen, PAD_isPressed(BTN_X), x + o, y + o, 0);

				// B (bottom)
				y += DP(ui.pill_height * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("B", screen, PAD_isPressed(BTN_B), x + o, y + o, 0);

				x -= DP(ui.pill_height);
				y -= DP(ui.pill_height);

				// Y (left)
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("Y", screen, PAD_isPressed(BTN_Y), x + o, y + o, 0);

				// A (right)
				x += DP(ui.pill_height * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("A", screen, PAD_isPressed(BTN_A), x + o, y + o, 0);
			}

			///////////////////////////////
			// Volume buttons (if available)
			///////////////////////////////
			if (has_volume) {
				int x = (FIXED_WIDTH - DP(99)) / 2;
				int y = oy + DP(ui.pill_height);
				int w = DP(42);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, DP(98)});
				x += DP(ui.button_margin);
				y += DP(ui.button_margin);
				blitButton("VOL. -", screen, PAD_isPressed(BTN_MINUS), x, y, w);
				x += w + DP(ui.button_margin);
				blitButton("VOL. +", screen, PAD_isPressed(BTN_PLUS), x, y, w);
			}

			///////////////////////////////
			// System buttons (Menu, Power)
			///////////////////////////////
			if (has_power || has_menu) {
				int bw = 42;
				int pw = has_both ? (bw * 2 + ui.button_margin * 3) : (bw + ui.button_margin * 2);

				int x = (FIXED_WIDTH - DP(pw)) / 2;
				int y = oy + DP(ui.pill_height * 3);
				int w = DP(bw);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, DP(pw)});
				x += DP(ui.button_margin);
				y += DP(ui.button_margin);
				if (has_menu) {
					blitButton("MENU", screen, PAD_isPressed(BTN_MENU), x, y, w);
					x += w + DP(ui.button_margin);
				}
				if (has_power) {
					blitButton("POWER", screen, PAD_isPressed(BTN_POWER), x, y, w);
				}
			}

			///////////////////////////////
			// Meta buttons (Select, Start) with quit hint
			///////////////////////////////
			{
				int x = (FIXED_WIDTH - DP(99)) / 2;
				int y = oy + DP(ui.pill_height * 5);
				int w = DP(42);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, DP(130)});
				x += DP(ui.button_margin);
				y += DP(ui.button_margin);
				blitButton("SELECT", screen, PAD_isPressed(BTN_SELECT), x, y, w);
				x += w + DP(ui.button_margin);
				blitButton("START", screen, PAD_isPressed(BTN_START), x, y, w);
				x += w + DP(ui.button_margin);

				// Display "QUIT" hint - press both together to exit
				SDL_Surface* text = TTF_RenderUTF8_Blended(font.tiny, "QUIT", COLOR_LIGHT_TEXT);
				SDL_BlitSurface(text, NULL, screen,
				                &(SDL_Rect){x, y + (DP(ui.button_size) - text->h) / 2});
				SDL_FreeSurface(text);
			}

			///////////////////////////////
			// Analog stick buttons (if available)
			///////////////////////////////
			if (has_L3) {
				int x = DP(ui.edge_padding + ui.pill_height);
				int y = oy + DP(ui.pill_height * 6);
				int o = DP(ui.button_margin);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("L3", screen, PAD_isPressed(BTN_L3), x + o, y + o, 0);
			}

			if (has_R3) {
				int x = FIXED_WIDTH - DP(ui.edge_padding + ui.pill_height * 3) + DP(ui.pill_height);
				int y = oy + DP(ui.pill_height * 6);
				int o = DP(ui.button_margin);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("R3", screen, PAD_isPressed(BTN_R3), x + o, y + o, 0);
			}

			GFX_flip(screen);
			dirty = 0;
		} else
			GFX_sync();
	}

	// Cleanup
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

	return EXIT_SUCCESS;
}
