/**
 * zero28/platform/platform.h - Platform definitions for MagicX Mini Zero 28
 *
 * Supported devices (Allwinner A133 Plus, Cortex-A53):
 * - MagicX Mini Zero 28: 2.8" 640x480 display
 *
 * Hardware features:
 * - 640x480 display (VGA resolution, 2x scaled)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2) with L3/R3 support
 * - Analog sticks (left and right)
 * - Menu and power buttons with volume controls
 * - Uses joystick input with HAT for D-pad
 * - Inverted volume scale (63 = mute, 0 = max)
 *
 * @note A/B and X/Y button mappings were swapped in first public stock release
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Platform Identification
///////////////////////////////

#define PLATFORM "zero28"

///////////////////////////////
// Audio Configuration
///////////////////////////////

// Uses default SND_RATE_CONTROL_D (0.012f) for standard timing

///////////////////////////////
// Video Buffer Scaling
///////////////////////////////

// Uses default BUFFER_SCALE_FACTOR (1.0f) - GPU hardware scaler handles all scaling

///////////////////////////////
// UI Scaling
///////////////////////////////

// Reduce UI size to fit more content on small 2.8" screen
#define SCALE_MODIFIER 0.92f

// Minimal edge padding - bezel provides visual margin
#define EDGE_PADDING 5

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// Platform Variant Detection
///////////////////////////////

// No device variants (single hardware configuration)

///////////////////////////////
// SDL Keyboard Button Mappings
// Zero28 does not use SDL keyboard input
///////////////////////////////

#define BUTTON_UP BUTTON_NA
#define BUTTON_DOWN BUTTON_NA
#define BUTTON_LEFT BUTTON_NA
#define BUTTON_RIGHT BUTTON_NA

#define BUTTON_SELECT BUTTON_NA
#define BUTTON_START BUTTON_NA

#define BUTTON_A BUTTON_NA
#define BUTTON_B BUTTON_NA
#define BUTTON_X BUTTON_NA
#define BUTTON_Y BUTTON_NA

#define BUTTON_L1 BUTTON_NA
#define BUTTON_R1 BUTTON_NA
#define BUTTON_L2 BUTTON_NA
#define BUTTON_R2 BUTTON_NA
#define BUTTON_L3 BUTTON_NA
#define BUTTON_R3 BUTTON_NA

#define BUTTON_MENU BUTTON_NA
#define BUTTON_MENU_ALT BUTTON_NA
#define BUTTON_POWER 116 // Direct power button code (not SDL)
#define BUTTON_PLUS BUTTON_NA
#define BUTTON_MINUS BUTTON_NA

///////////////////////////////
// Evdev/Keyboard Input Codes
// Hardware keycodes from kernel input subsystem
///////////////////////////////

#define CODE_UP CODE_NA
#define CODE_DOWN CODE_NA
#define CODE_LEFT CODE_NA
#define CODE_RIGHT CODE_NA

#define CODE_SELECT CODE_NA
#define CODE_START CODE_NA

#define CODE_A CODE_NA
#define CODE_B CODE_NA
#define CODE_X CODE_NA
#define CODE_Y CODE_NA

#define CODE_L1 CODE_NA
#define CODE_R1 CODE_NA
#define CODE_L2 CODE_NA
#define CODE_R2 CODE_NA
#define CODE_L3 CODE_NA
#define CODE_R3 CODE_NA

#define CODE_MENU CODE_NA
#define CODE_POWER 102 // KEY_HOME

#define CODE_PLUS 128 // Volume up
#define CODE_MINUS 129 // Volume down

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick indices (D-pad uses HAT)
///////////////////////////////

#define JOY_UP 13 // D-pad up (HAT)
#define JOY_DOWN 16 // D-pad down (HAT)
#define JOY_LEFT 14 // D-pad left (HAT)
#define JOY_RIGHT 15 // D-pad right (HAT)

#define JOY_SELECT 8
#define JOY_START 9

// Button mappings were swapped in first public stock release
#define JOY_A 0
#define JOY_B 1
#define JOY_X 2
#define JOY_Y 3

#define JOY_L1 4
#define JOY_R1 5
#define JOY_L2 6
#define JOY_R2 7
#define JOY_L3 10 // Left stick button
#define JOY_R3 11 // Right stick button

#define JOY_MENU 19
#define JOY_POWER 102 // Matches CODE_POWER
#define JOY_PLUS 18 // Volume up button
#define JOY_MINUS 17 // Volume down button

///////////////////////////////
// Analog Stick Axis Mappings
// Hardware analog stick axes
///////////////////////////////

#define AXIS_LX 0 // ABS_X - Left stick X-axis (-30k left to 30k right)
#define AXIS_LY 1 // ABS_Y - Left stick Y-axis (-30k up to 30k down)
#define AXIS_RX 2 // ABS_RX - Right stick X-axis (-30k left to 30k right)
#define AXIS_RY 3 // ABS_RY - Right stick Y-axis (-30k up to 30k down)

///////////////////////////////
// Function Button Mappings
// System-level button combinations
///////////////////////////////

#define BTN_RESUME BTN_X // Button to resume from save state
#define BTN_SLEEP BTN_POWER // Button to enter sleep mode
#define BTN_WAKE BTN_POWER // Button to wake from sleep
#define BTN_MOD_VOLUME BTN_NONE // Modifier for volume control (none - direct buttons)
#define BTN_MOD_BRIGHTNESS BTN_MENU // Hold MENU for brightness control
#define BTN_MOD_PLUS BTN_PLUS // Increase with PLUS
#define BTN_MOD_MINUS BTN_MINUS // Decrease with MINUS

///////////////////////////////
// Display Specifications
///////////////////////////////

#define SCREEN_DIAGONAL 2.8f // Physical screen diagonal in inches
#define FIXED_WIDTH 640 // Screen width in pixels
#define FIXED_HEIGHT 480 // Screen height in pixels (VGA)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD" // Path to SD card mount point
#define MUTE_VOLUME_RAW 63 // Volume scale is inverted: 63 = mute, 0 = max volume

///////////////////////////////
// Keymon Configuration
///////////////////////////////

#define KEYMON_BUTTON_MENU 158
#define KEYMON_BUTTON_MENU_ALT -1
#define KEYMON_BUTTON_PLUS 115
#define KEYMON_BUTTON_MINUS 114

#define KEYMON_HAS_HDMI 0
#define KEYMON_HAS_JACK 0

#define KEYMON_INPUT_COUNT 2
#define KEYMON_INPUT_DEVICE_0 "/dev/input/event1"
#define KEYMON_INPUT_DEVICE_1 "/dev/input/event2"

#define KEYMON_HAS_JACK_SWITCH 1 // Uses EV_SW events for jack detection

///////////////////////////////
// Show Configuration
///////////////////////////////

// zero28 uses clockwise rotation (90° = rotate 1) instead of CCW (270° = rotate 3)
#define SHOW_ROTATION_CLOCKWISE 1

///////////////////////////////

#endif
