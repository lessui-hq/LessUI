/**
 * rg35xxplus/platform/platform.h - Platform definitions for Anbernic RG35XX Plus/H/SP variants
 *
 * The Anbernic RG35XX Plus series includes multiple device variants:
 * - RG35XX Plus: 640x480 display
 * - RG35XX H (CubeXX): 720x720 square display
 * - RG35XX SP (RG34XX): 720x480 widescreen display
 * - 1280x720 HDMI output support (all variants)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu and power buttons with volume controls
 * - Uses joystick input
 * - NEON SIMD optimization support
 * - Runtime detection of device variant and HDMI
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Platform Identification
///////////////////////////////

#define PLATFORM "rg35xxplus"

///////////////////////////////
// Dependencies
///////////////////////////////

#include "platform_variant.h"
#include "sdl.h"

///////////////////////////////
// Platform Variant Detection
///////////////////////////////

// RG35XX Plus family variants
#define VARIANT_RG35XX_VGA (VARIANT_PLATFORM_BASE + 0) // 640x480 (most devices)
#define VARIANT_RG35XX_SQUARE (VARIANT_PLATFORM_BASE + 1) // 720x720 (CubeXX)
#define VARIANT_RG35XX_WIDE (VARIANT_PLATFORM_BASE + 2) // 720x480 (RG34XX)

///////////////////////////////
// SDL Keyboard Button Mappings
// RG35XX Plus does not use SDL keyboard input
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
// Hardware keycodes (power button only)
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

#define CODE_PLUS CODE_NA
#define CODE_MINUS CODE_NA

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick indices
///////////////////////////////

#define JOY_UP 13
#define JOY_DOWN 16
#define JOY_LEFT 14
#define JOY_RIGHT 15

#define JOY_SELECT 6
#define JOY_START 7

#define JOY_A 0
#define JOY_B 1
#define JOY_X 3
#define JOY_Y 2

#define JOY_L1 4
#define JOY_R1 5
#define JOY_L2 9
#define JOY_R2 10
#define JOY_L3 JOY_NA
#define JOY_R3 JOY_NA

#define JOY_MENU 8
#define JOY_POWER JOY_NA
#define JOY_PLUS 18 // Volume up
#define JOY_MINUS 17 // Volume down

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
// Runtime-configurable for device variants
///////////////////////////////

#define SCREEN_DIAGONAL (platform_variant.screen_diagonal)
#define FIXED_WIDTH (platform_variant.screen_width)
#define FIXED_HEIGHT (platform_variant.screen_height)

///////////////////////////////
// HDMI Output Specifications
///////////////////////////////

#define HAS_HDMI 1 // HDMI output supported
#define HDMI_WIDTH 1280 // HDMI width in pixels
#define HDMI_HEIGHT 720 // HDMI height in pixels (720p)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/sdcard" // Path to SD card mount point (lowercase)
#define MUTE_VOLUME_RAW 0 // Raw value for muted volume
#define HAS_NEON // ARM NEON SIMD optimizations available

///////////////////////////////
// Keymon Configuration
///////////////////////////////

#define KEYMON_BUTTON_MENU 312
#define KEYMON_BUTTON_MENU_ALT -1
#define KEYMON_BUTTON_PLUS 115
#define KEYMON_BUTTON_MINUS 114

#define KEYMON_HAS_HDMI 1
#define KEYMON_HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"
#define KEYMON_HAS_JACK 0

// Uses /dev/input/event1 (not event0!)
#define KEYMON_INPUT_DEVICE "/dev/input/event1"

///////////////////////////////

#endif
