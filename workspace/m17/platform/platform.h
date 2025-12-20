/**
 * m17/platform/platform.h - Platform definitions for M17
 *
 * Supported devices:
 * - M17: 480x272 display (16:9 widescreen, 4.3" diagonal)
 *
 * Hardware features:
 * - 480x272 widescreen display (16:9 aspect ratio)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu button with alternate
 * - Uses hybrid input (SDL keyboard + joystick)
 * - NEON optimization support (potentially available)
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Platform Identification
///////////////////////////////

#define PLATFORM "m17"

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

// Uses default SCALE_MODIFIER (1.0f) and EDGE_PADDING (10) for standard layout

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
// Maps physical buttons to SDL key codes
///////////////////////////////

#define BUTTON_UP SDLK_UP
#define BUTTON_DOWN SDLK_DOWN
#define BUTTON_LEFT SDLK_LEFT
#define BUTTON_RIGHT SDLK_RIGHT

#define BUTTON_SELECT SDLK_RSHIFT
#define BUTTON_START SDLK_RETURN

// Note: A/B and X/Y labels are swapped from typical layout
#define BUTTON_A SDLK_B
#define BUTTON_B SDLK_A
#define BUTTON_X SDLK_Y
#define BUTTON_Y SDLK_X

#define BUTTON_L1 SDLK_L
#define BUTTON_R1 SDLK_R
#define BUTTON_L2 SDLK_z
#define BUTTON_R2 SDLK_c
#define BUTTON_L3 BUTTON_NA
#define BUTTON_R3 BUTTON_NA

#define BUTTON_MENU BUTTON_NA
#define BUTTON_MENU_ALT BUTTON_NA
#define BUTTON_POWER BUTTON_NA
#define BUTTON_PLUS BUTTON_NA
#define BUTTON_MINUS BUTTON_NA

///////////////////////////////
// Evdev/Keyboard Input Codes
// M17 does not use keyboard input codes
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
#define CODE_MENU_ALT CODE_NA
#define CODE_POWER CODE_NA

#define CODE_PLUS CODE_NA
#define CODE_MINUS CODE_NA

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick button indices
///////////////////////////////

#define JOY_UP 11
#define JOY_DOWN 14
#define JOY_LEFT 12
#define JOY_RIGHT 13

#define JOY_SELECT 10
#define JOY_START 3

#define JOY_A 9
#define JOY_B 4
#define JOY_X 2
#define JOY_Y 7

#define JOY_L1 5
#define JOY_R1 1
#define JOY_L2 6
#define JOY_R2 8
#define JOY_L3 JOY_NA
#define JOY_R3 JOY_NA

#define JOY_MENU 15
#define JOY_MENU_ALT 16 // Secondary menu button
#define JOY_POWER JOY_NA
#define JOY_PLUS JOY_NA
#define JOY_MINUS JOY_NA

///////////////////////////////
// Function Button Mappings
// System-level button combinations
///////////////////////////////

#define BTN_RESUME BTN_X // Button to resume from save state
#define BTN_SLEEP BTN_MENU // Button to enter sleep mode
#define BTN_WAKE BTN_MENU // Button to wake from sleep
#define BTN_MOD_VOLUME BTN_SELECT // Hold SELECT for volume control
#define BTN_MOD_BRIGHTNESS BTN_START // Hold START for brightness control
#define BTN_MOD_PLUS BTN_R1 // Increase with R1
#define BTN_MOD_MINUS BTN_L1 // Decrease with L1

///////////////////////////////
// Display Specifications
///////////////////////////////

#define SCREEN_DIAGONAL 4.3f // Physical screen diagonal in inches
#define FIXED_WIDTH 480 // Screen width in pixels
#define FIXED_HEIGHT 272 // Screen height in pixels (16:9 widescreen)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/sdcard" // Path to SD card mount point
#define MUTE_VOLUME_RAW 0 // Raw value for muted volume

///////////////////////////////
// Keymon Configuration
///////////////////////////////

// m17 uses SELECT/START instead of MENU button (like trimuismart)
#define KEYMON_USE_SELECT_START 1
#define KEYMON_BUTTON_SELECT 54
#define KEYMON_BUTTON_START 28
#define KEYMON_BUTTON_L1 38
#define KEYMON_BUTTON_R1 19

// m17 has hardware volume buttons that need quirk handling
// (separate from the R1/L1 brightness/volume combos)
#define KEYMON_BUTTON_PLUS 115
#define KEYMON_BUTTON_MINUS 114
#define KEYMON_HAS_VOLUME_QUIRK 1

#define KEYMON_HAS_HDMI 1
#define KEYMON_HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

#define KEYMON_HAS_JACK 1
#define KEYMON_JACK_STATE_PATH "/sys/devices/virtual/switch/h2w/state"

// Uses 4 input devices
#define KEYMON_INPUT_COUNT 4

///////////////////////////////

#endif
