/**
 * pad.h - Button and analog stick input handling
 *
 * Pure logic layer for input state management. No SDL dependencies.
 * This module is separated from api.c for testability.
 *
 * Responsibilities:
 * - Analog stick to digital button conversion (with deadzone)
 * - Button state tracking (pressed, released, repeated)
 * - Menu tap detection (quick tap vs hold)
 */

#ifndef __PAD_H__
#define __PAD_H__

#include "defines.h"
#include <stdint.h>

// Type definitions
// When included from api.c, api.h is included first and defines these
// When included standalone (tests, pad.c itself), we need to define them
#ifndef __API_H__
/**
 * Analog stick axis values.
 */
typedef struct PAD_Axis {
	int x; // X-axis value (-32768 to 32767)
	int y; // Y-axis value (-32768 to 32767)
} PAD_Axis;

/**
 * Input context containing all button state.
 */
typedef struct PAD_Context {
	int is_pressed; // Bitmask of currently pressed buttons
	int just_pressed; // Bitmask of buttons pressed this frame
	int just_released; // Bitmask of buttons released this frame
	int just_repeated; // Bitmask of buttons auto-repeated this frame
	uint32_t repeat_at[BTN_ID_COUNT]; // Timestamp for next repeat per button
	PAD_Axis laxis; // Left analog stick state
	PAD_Axis raxis; // Right analog stick state
} PAD_Context;

/**
 * Global input state, polled each frame.
 */
extern PAD_Context pad;
#endif

/**
 * Analog stick deadzone (threshold for registering input).
 */
#define AXIS_DEADZONE 0x4000

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
void PAD_setAnalog(int neg_id, int pos_id, int value, int repeat_at);

/**
 * Resets all button states to unpressed.
 *
 * Clears all button press/release/repeat flags.
 * Call this when changing contexts (e.g., entering/exiting sleep).
 */
void PAD_reset(void);

/**
 * Checks if any button was just pressed this frame.
 *
 * @return 1 if any button was just pressed, 0 otherwise
 */
int PAD_anyJustPressed(void);

/**
 * Checks if any button is currently held down.
 *
 * @return 1 if any button is pressed, 0 otherwise
 */
int PAD_anyPressed(void);

/**
 * Checks if any button was just released this frame.
 *
 * @return 1 if any button was just released, 0 otherwise
 */
int PAD_anyJustReleased(void);

/**
 * Checks if a specific button was just pressed this frame.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button was just pressed, 0 otherwise
 */
int PAD_justPressed(int btn);

/**
 * Checks if a specific button is currently held down.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button is pressed, 0 otherwise
 */
int PAD_isPressed(int btn);

/**
 * Checks if a specific button was just released this frame.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button was just released, 0 otherwise
 */
int PAD_justReleased(int btn);

/**
 * Checks if a specific button is repeating (held for repeat interval).
 *
 * @param btn Button bitmask (e.g., BTN_DPAD_UP)
 * @return 1 if button is repeating this frame, 0 otherwise
 */
int PAD_justRepeated(int btn);

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
int PAD_tappedMenu(uint32_t now);

#endif // __PAD_H__
