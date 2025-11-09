/**
 * pad.c - Button and analog stick input handling
 *
 * Pure logic layer for input state management. No SDL dependencies.
 * This module is separated from api.c for testability.
 *
 * Responsibilities:
 * - Analog stick to digital button conversion (with deadzone)
 * - Button state tracking (pressed, released, repeated)
 * - Menu tap detection (quick tap vs hold)
 */

#include "pad.h"

///////////////////////////////
// Input - Button and analog stick handling
///////////////////////////////

// Global input state, polled each frame
PAD_Context pad;

/**
 * Processes analog stick movement and updates button state.
 *
 * Converts analog axis value to digital button presses (up/down/left/right).
 * Handles deadzone, button repeat, and opposite direction cancellation.
 *
 * @param neg_id Button ID for negative direction (left/up)
 * @param pos_id Button ID for positive direction (right/down)
 * @param value Analog axis value (-32768 to 32767)
 * @param repeat_at Timestamp when button should start repeating
 *
 * @note Called internally by PLAT_pollInput for analog stick axes
 */
void PAD_setAnalog(int neg_id, int pos_id, int value, int repeat_at) {
	// LOG_info("neg %i pos %i value %i\n", neg_id, pos_id, value);
	int neg = 1 << neg_id;
	int pos = 1 << pos_id;
	if (value > AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed & pos)) { // not pressing
			pad.is_pressed |= pos; // set
			pad.just_pressed |= pos; // set
			pad.just_repeated |= pos; // set
			pad.repeat_at[pos_id] = repeat_at;

			if (pad.is_pressed & neg) { // was pressing opposite
				pad.is_pressed &= ~neg; // unset
				pad.just_repeated &= ~neg; // unset
				pad.just_released |= neg; // set
			}
		}
	} else if (value < -AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed & neg)) { // not pressing
			pad.is_pressed |= neg; // set
			pad.just_pressed |= neg; // set
			pad.just_repeated |= neg; // set
			pad.repeat_at[neg_id] = repeat_at;

			if (pad.is_pressed & pos) { // was pressing opposite
				pad.is_pressed &= ~pos; // unset
				pad.just_repeated &= ~pos; // unset
				pad.just_released |= pos; // set
			}
		}
	} else { // not pressing
		if (pad.is_pressed & neg) { // was pressing
			pad.is_pressed &= ~neg; // unset
			pad.just_repeated &= ~neg; // unset
			pad.just_released |= neg; // set
		}
		if (pad.is_pressed & pos) { // was pressing
			pad.is_pressed &= ~pos; // unset
			pad.just_repeated &= ~pos; // unset
			pad.just_released |= pos; // set
		}
	}
}

/**
 * Resets all button states to unpressed.
 *
 * Clears all button press/release/repeat flags.
 * Call this when changing contexts (e.g., entering/exiting sleep).
 */
void PAD_reset(void) {
	// LOG_info("PAD_reset");
	pad.just_pressed = BTN_NONE;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;
}

/**
 * Checks if any button was just pressed this frame.
 *
 * @return 1 if any button was just pressed, 0 otherwise
 */
int PAD_anyJustPressed(void) {
	return pad.just_pressed != BTN_NONE;
}

/**
 * Checks if any button is currently held down.
 *
 * @return 1 if any button is pressed, 0 otherwise
 */
int PAD_anyPressed(void) {
	return pad.is_pressed != BTN_NONE;
}

/**
 * Checks if any button was just released this frame.
 *
 * @return 1 if any button was just released, 0 otherwise
 */
int PAD_anyJustReleased(void) {
	return pad.just_released != BTN_NONE;
}

/**
 * Checks if a specific button was just pressed this frame.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button was just pressed, 0 otherwise
 */
int PAD_justPressed(int btn) {
	return pad.just_pressed & btn;
}

/**
 * Checks if a specific button is currently held down.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button is pressed, 0 otherwise
 */
int PAD_isPressed(int btn) {
	return pad.is_pressed & btn;
}

/**
 * Checks if a specific button was just released this frame.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button was just released, 0 otherwise
 */
int PAD_justReleased(int btn) {
	return pad.just_released & btn;
}

/**
 * Checks if a specific button is repeating (held for repeat interval).
 *
 * @param btn Button bitmask (e.g., BTN_DPAD_UP)
 * @return 1 if button is repeating this frame, 0 otherwise
 */
int PAD_justRepeated(int btn) {
	return pad.just_repeated & btn;
}

/**
 * Detects a quick tap of the menu button.
 *
 * Returns true if menu button was pressed and released within MENU_DELAY (250ms)
 * without any brightness adjustment (PLUS/MINUS) being triggered.
 *
 * @param now Current timestamp in milliseconds
 * @return 1 if menu was tapped (not held), 0 otherwise
 *
 * @note Used to distinguish menu tap from menu+brightness adjustment
 */
int PAD_tappedMenu(uint32_t now) {
#define MENU_DELAY 250 // also in PWR_update()
	static uint32_t menu_start = 0;
	static int ignore_menu = 0;
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
		menu_start = now;
	} else if (PAD_isPressed(BTN_MENU) && BTN_MOD_BRIGHTNESS == BTN_MENU &&
	           (PAD_justPressed(BTN_MOD_PLUS) || PAD_justPressed(BTN_MOD_MINUS))) {
		ignore_menu = 1;
	}
	return (!ignore_menu && PAD_justReleased(BTN_MENU) && now - menu_start < MENU_DELAY);
}
