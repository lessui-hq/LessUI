/**
 * my355/platform/platform.h - Platform definitions for MY355 / Miyoo Flip
 *
 * Supported devices (Rockchip RK3566, Cortex-A55):
 * - MY355 / Miyoo Flip: 3.5" 640x480 display
 *
 * Hardware features:
 * - 640x480 display (VGA resolution, 2x scaled)
 * - 1280x720 HDMI output support
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2) with L3/R3 support
 * - Analog sticks (left and right)
 * - Menu and power buttons with volume controls
 * - Uses evdev input codes
 * - Runtime HDMI detection
 *
 * @note Different from miyoomini platform (R16/SSD202D) and MY285 Mini Flip (clamshell)
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Platform Identification
///////////////////////////////

#define PLATFORM "my355"

///////////////////////////////
// Audio Configuration
///////////////////////////////

// More aggressive rate control for device with timing variance
#define SND_RATE_CONTROL_D 0.015f

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// Platform Variant Detection
///////////////////////////////

// No device variants (single hardware configuration)
// Runtime HDMI detection handled via on_hdmi variable

extern int on_hdmi; // Set to 1 when HDMI output is active

///////////////////////////////
// SDL Keyboard Button Mappings
// MY355 does not use SDL keyboard input
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
#define BUTTON_POWER BUTTON_NA
#define BUTTON_PLUS BUTTON_NA
#define BUTTON_MINUS BUTTON_NA

///////////////////////////////
// Evdev/Keyboard Input Codes
// Hardware keycodes from kernel input subsystem
///////////////////////////////

#define CODE_UP 82 // HID Up Arrow
#define CODE_DOWN 81 // HID Down Arrow
#define CODE_LEFT 80 // HID Left Arrow
#define CODE_RIGHT 79 // HID Right Arrow

#define CODE_SELECT 228 // HID Left GUI
#define CODE_START 40 // HID Enter

#define CODE_A 44 // HID Space
#define CODE_B 224 // HID Left Control
#define CODE_X 225 // HID Left Shift
#define CODE_Y 226 // HID Left Alt

#define CODE_L1 43 // HID Tab
#define CODE_R1 42 // HID Backspace
#define CODE_L2 75 // HID Page Up
#define CODE_R2 78 // HID Page Down
#define CODE_L3 230 // HID Left Alt (alt mapping)
#define CODE_R3 229 // HID Right Shift

#define CODE_MENU 41 // HID Escape
#define CODE_POWER 102 // KEY_HOME

#define CODE_PLUS 128 // Volume up
#define CODE_MINUS 129 // Volume down

///////////////////////////////
// Joystick Button Mappings
// MY355 does not use joystick input
///////////////////////////////

#define JOY_UP JOY_NA
#define JOY_DOWN JOY_NA
#define JOY_LEFT JOY_NA
#define JOY_RIGHT JOY_NA

#define JOY_SELECT JOY_NA
#define JOY_START JOY_NA

#define JOY_A JOY_NA
#define JOY_B JOY_NA
#define JOY_X JOY_NA
#define JOY_Y JOY_NA

#define JOY_L1 JOY_NA
#define JOY_R1 JOY_NA
#define JOY_L2 JOY_NA
#define JOY_R2 JOY_NA
#define JOY_L3 JOY_NA
#define JOY_R3 JOY_NA

#define JOY_MENU JOY_NA
#define JOY_POWER JOY_NA
#define JOY_PLUS JOY_NA
#define JOY_MINUS JOY_NA

///////////////////////////////
// Analog Stick Axis Mappings
// Hardware analog stick axes
///////////////////////////////

#define AXIS_LX 0 // Left stick X-axis
#define AXIS_LY 1 // Left stick Y-axis
#define AXIS_RX 3 // Right stick X-axis
#define AXIS_RY 4 // Right stick Y-axis

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

#define SCREEN_DIAGONAL 3.5f // Physical screen diagonal in inches (Miyoo Flip)
#define FIXED_WIDTH 640 // Screen width in pixels
#define FIXED_HEIGHT 480 // Screen height in pixels (VGA)

///////////////////////////////
// HDMI Output Specifications
///////////////////////////////

#define HAS_HDMI 1 // HDMI output supported
#define HDMI_WIDTH 1280 // HDMI width in pixels
#define HDMI_HEIGHT 720 // HDMI height in pixels (720p)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD" // Path to SD card mount point
#define MUTE_VOLUME_RAW 0 // Raw value for muted volume

///////////////////////////////

#endif

///////////////////////////////
// Keymon Configuration
///////////////////////////////

#define KEYMON_BUTTON_MENU 1
#define KEYMON_BUTTON_MENU_ALT -1
#define KEYMON_BUTTON_PLUS 115
#define KEYMON_BUTTON_MINUS 114

#define KEYMON_HAS_HDMI 1
#define KEYMON_HDMI_STATE_PATH "/sys/class/drm/card0-HDMI-A-1/status"
#define KEYMON_HDMI_USE_STRING 1

#define KEYMON_HAS_JACK 1
#define KEYMON_JACK_STATE_PATH "/sys/class/gpio/gpio150/value"
#define KEYMON_JACK_INVERTED 1
