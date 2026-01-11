/**
 * tg5050/platform/platform.h - Platform definitions for TrimUI Smart Pro S
 *
 * The TG5050 is a single-device platform (no variants):
 * - 1280x720 widescreen display
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1) with analog L2/R2 triggers
 * - Analog sticks (left and right) with L3/R3 click buttons
 * - Menu and power buttons with volume controls
 * - Uses joystick input with HAT for D-pad
 *
 * Key hardware differences from TG5040:
 * - Allwinner A523 SoC (8x Cortex-A55, dual cluster)
 * - Backlight via sysfs (not /dev/disp ioctl)
 * - Different audio mixer controls (DAC Volume)
 * - Rumble on GPIO 236 (not 227)
 * - Speaker mute via sysfs
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Platform Identification
///////////////////////////////

#define PLATFORM "tg5050"

///////////////////////////////
// Hardware Capabilities
///////////////////////////////

#define HAS_OPENGLES 1 // Mali GPU supports OpenGL ES 3.2

///////////////////////////////
// Audio Configuration
///////////////////////////////

// Uses default SND_RATE_CONTROL_D (0.012f) - cubic safety boost handles edge cases

///////////////////////////////
// Video Buffer Scaling
///////////////////////////////

// Uses default BUFFER_SCALE_FACTOR (1.0f) - GPU hardware scaler handles all scaling

///////////////////////////////
// UI Scaling
///////////////////////////////

// Reduced edge padding - bezel provides visual margin
#define EDGE_PADDING 5

///////////////////////////////
// Dependencies
///////////////////////////////

#include "platform_variant.h"
#include "sdl.h"

///////////////////////////////
// SDL Keyboard Button Mappings
// TG5050 does not use SDL keyboard input
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

#define JOY_UP JOY_NA // D-pad handled via HAT
#define JOY_DOWN JOY_NA
#define JOY_LEFT JOY_NA
#define JOY_RIGHT JOY_NA

#define JOY_SELECT 6
#define JOY_START 7

// Button mappings (may need verification on actual hardware)
#define JOY_A 1
#define JOY_B 0
#define JOY_X 3
#define JOY_Y 2

#define JOY_L1 4
#define JOY_R1 5
#define JOY_L2 JOY_NA // Analog trigger (handled via axis)
#define JOY_R2 JOY_NA // Analog trigger (handled via axis)
#define JOY_L3 9 // Stick click buttons available
#define JOY_R3 10

#define JOY_MENU 8
#define JOY_POWER 102 // Matches CODE_POWER
#define JOY_PLUS 128
#define JOY_MINUS 129

///////////////////////////////
// Analog Stick and Trigger Axis Mappings
// Hardware analog axes
///////////////////////////////

#define AXIS_L2 2 // ABSZ - Left trigger analog input
#define AXIS_R2 5 // RABSZ - Right trigger analog input

#define AXIS_LX 0 // ABS_X - Left stick X-axis (-30k left to 30k right)
#define AXIS_LY 1 // ABS_Y - Left stick Y-axis (-30k up to 30k down)
#define AXIS_RX 3 // ABS_RX - Right stick X-axis (-30k left to 30k right)
#define AXIS_RY 4 // ABS_RY - Right stick Y-axis (-30k up to 30k down)

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
// Single device - no variants needed
///////////////////////////////

#define SCREEN_DIAGONAL 4.95f
#define FIXED_WIDTH 1280
#define FIXED_HEIGHT 720

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD" // Path to SD card mount point
#define MUTE_VOLUME_RAW 0 // Raw value for muted volume

///////////////////////////////
// Keymon Configuration
///////////////////////////////

// tg5050 has similar menu button codes to tg5040
#define KEYMON_BUTTON_MENU 314
#define KEYMON_BUTTON_MENU_ALT 315
#define KEYMON_BUTTON_MENU_ALT2 316
#define KEYMON_BUTTON_PLUS 115
#define KEYMON_BUTTON_MINUS 114

// Uses multiple input devices
// event0-3: keyboard, vibrator, power, headphones
// event4: TRIMUI Player1 gamepad (where MENU button 314 comes from)
#define KEYMON_INPUT_COUNT 5

#define KEYMON_HAS_HDMI 0 // TODO: Verify HDMI support on TG5050

#define KEYMON_HAS_JACK 1
#define KEYMON_JACK_STATE_PATH "/sys/bus/platform/devices/singleadc-joypad/hp"

// tg5050 also uses EV_SW switch events for jack detection
#define KEYMON_HAS_JACK_SWITCH 1

// tg5050 FN switch is on GPIO 363
#define KEYMON_HAS_MUTE 1
#define KEYMON_MUTE_STATE_PATH "/sys/class/gpio/gpio363/value"

///////////////////////////////

#endif
