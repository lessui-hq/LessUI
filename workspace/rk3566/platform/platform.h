/**
 * RK3566/platform/platform.h - Platform definitions for Rockchip RK3566 devices
 *
 * Supported devices (Rockchip RK3566, Cortex-A55):
 * - PowKiddy RGB30: 4.0" 720x720 display
 * - Anbernic RG353P/M/V/VS: Various display configurations
 *
 * Hardware features:
 * - 720x720 square display (1:1 aspect ratio)
 * - 1280x720 HDMI output support
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu buttons (primary and alternate)
 * - Uses hybrid input (minimal SDL keyboard + joystick)
 * - Larger UI with increased row count and padding
 *
 * @note Power button uses SDL keyboard mapping, volume controls use evdev codes
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Platform Identification
///////////////////////////////

#define PLATFORM "rk3566"

///////////////////////////////
// Hardware Capabilities
///////////////////////////////

#define HAS_OPENGLES 1 // Mali-G52 GPU supports OpenGL ES 2.0

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
// RK3566 devices use minimal SDL keyboard input (power button only)
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
#define BUTTON_POWER SDLK_POWER // Power key mapped to SDL
#define BUTTON_PLUS BUTTON_NA // Available but not used (commented: SDLK_RSUPER)
#define BUTTON_MINUS BUTTON_NA // Available but not used (commented: SDLK_LSUPER)

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
#define CODE_MENU_ALT CODE_NA
#define CODE_POWER 102 // KEY_HOME

#define CODE_PLUS 129 // Volume up (swapped from keymon)
#define CODE_MINUS 128 // Volume down (swapped from keymon)

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick indices
///////////////////////////////

#define JOY_UP 13
#define JOY_DOWN 14
#define JOY_LEFT 15
#define JOY_RIGHT 16

#define JOY_SELECT 8
#define JOY_START 9

#define JOY_A 1
#define JOY_B 0
#define JOY_X 2
#define JOY_Y 3

#define JOY_L1 4
#define JOY_R1 5
#define JOY_L2 6
#define JOY_R2 7
#define JOY_L3 JOY_NA
#define JOY_R3 JOY_NA

#define JOY_MENU 11
#define JOY_MENU_ALT 12 // Secondary menu button
#define JOY_POWER JOY_NA
#define JOY_PLUS JOY_NA
#define JOY_MINUS JOY_NA

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

#define SCREEN_DIAGONAL 4.0f // Physical screen diagonal in inches
#define FIXED_WIDTH 720 // Screen width in pixels (square display)
#define FIXED_HEIGHT 720 // Screen height in pixels (1:1 aspect ratio)

///////////////////////////////
// HDMI Output Specifications
///////////////////////////////

#define HAS_HDMI 1 // HDMI output supported
#define HDMI_WIDTH 1280 // HDMI width in pixels
#define HDMI_HEIGHT 720 // HDMI height in pixels (720p)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH                                                                                \
	"/storage" // Path to SD card mount point (LessOS default, overridden by LESSOS_STORAGE)
#define MUTE_VOLUME_RAW 0 // Raw value for muted volume

///////////////////////////////
// Keymon Configuration
///////////////////////////////

#define KEYMON_BUTTON_MENU 317
#define KEYMON_BUTTON_MENU_ALT 318
#define KEYMON_BUTTON_PLUS 114
#define KEYMON_BUTTON_MINUS 115

#define KEYMON_HAS_HDMI 1
#define KEYMON_HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

#define KEYMON_HAS_JACK 1
#define KEYMON_JACK_STATE_PATH "/sys/bus/platform/devices/singleadc-joypad/hp"

// Uses event0-4 plus js0 (joystick) for menu button detection
#define KEYMON_INPUT_COUNT 6
#define KEYMON_INPUT_DEVICE_0 "/dev/input/event0"
#define KEYMON_INPUT_DEVICE_1 "/dev/input/event1"
#define KEYMON_INPUT_DEVICE_2 "/dev/input/event2"
#define KEYMON_INPUT_DEVICE_3 "/dev/input/event3"
#define KEYMON_INPUT_DEVICE_4 "/dev/input/event4"
#define KEYMON_INPUT_DEVICE_5 "/dev/input/js0"

///////////////////////////////

#endif
