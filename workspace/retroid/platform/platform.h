/**
 * retroid/platform/platform.h - Platform definitions for Retroid Pocket SM8250 devices
 *
 * Supports multiple device variants in the Retroid Pocket family:
 * - Retroid Pocket 5: 5.5" 1920x1080 AMOLED (16:9)
 * - Retroid Pocket Flip 2: 5.5" 1920x1080 AMOLED (16:9, clamshell)
 * - Retroid Pocket Mini V1: 3.7" 1280x960 AMOLED (4:3)
 * - Retroid Pocket Mini V2: 3.92" 1240x1080 AMOLED (~31:27, near square)
 *
 * Hardware features:
 * - Qualcomm Snapdragon 865 (SM8250) SoC
 * - Adreno 650 GPU with Vulkan/OpenGL ES support
 * - Dual analog sticks with hall effect sensors
 * - Analog L2/R2 triggers
 * - HDMI output support
 * - WiFi 6 + Bluetooth 5.1
 * - Active cooling
 *
 * OS: LessOS (ROCKNIX-based)
 *
 * LessOS Environment Variables (provided at boot):
 * - LESSOS_PLATFORM: SoC family ("SM8250")
 * - LESSOS_DEVICE: Device model ("Retroid Pocket 5", "Retroid Pocket Mini V2", etc.)
 * - LESSOS_STORAGE: Writable storage path ("/storage")
 *
 * See workspace/retroid/README.md for LessOS/ROCKNIX upstream references.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Platform Identification
///////////////////////////////

#define PLATFORM "retroid"

///////////////////////////////
// Hardware Capabilities
///////////////////////////////

#define HAS_OPENGLES 1 // Adreno 650 GPU supports OpenGL ES 3.2

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

// Runtime-configurable based on variant detection
// - FHD devices (5.5" screens): 1.0f scale with 10px padding
// - Mini V2 (3.92" screen): 1.0f scale with 10px padding (high DPI)
// Default to standard layout; both variants have good screen real estate
#define SCALE_MODIFIER 1.0f
#define EDGE_PADDING 10

///////////////////////////////
// Dependencies
///////////////////////////////

#include "platform_variant.h"
#include "sdl.h"

///////////////////////////////
// Platform Variant Detection
///////////////////////////////

// Retroid Pocket SM8250 family variants
#define VARIANT_RETROID_FHD (VARIANT_PLATFORM_BASE + 0) // 1920x1080 (Pocket 5, Flip 2)
#define VARIANT_RETROID_MINI_V1 (VARIANT_PLATFORM_BASE + 1) // 1280x960 (Mini V1)
#define VARIANT_RETROID_MINI_V2 (VARIANT_PLATFORM_BASE + 2) // 1240x1080 (Mini V2)

///////////////////////////////
// SDL Keyboard Button Mappings
// Retroid uses evdev input, not SDL keyboard
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
#define BUTTON_POWER BUTTON_NA
#define BUTTON_PLUS BUTTON_NA
#define BUTTON_MINUS BUTTON_NA

///////////////////////////////
// Evdev/Keyboard Input Codes
// From Retroid Pocket Gamepad kernel driver (retroid.c)
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
#define CODE_POWER 116 // KEY_POWER

#define CODE_PLUS 115 // KEY_VOLUMEUP
#define CODE_MINUS 114 // KEY_VOLUMEDOWN

///////////////////////////////
// Joystick Button Mappings
// From Retroid Pocket Gamepad kernel driver keymap[]
// Uses standard Linux BTN_* codes
///////////////////////////////

#define JOY_UP 13 // BTN_DPAD_UP (bit 0 in keymap)
#define JOY_DOWN 14 // BTN_DPAD_DOWN (bit 1)
#define JOY_LEFT 15 // BTN_DPAD_LEFT (bit 2)
#define JOY_RIGHT 16 // BTN_DPAD_RIGHT (bit 3)

#define JOY_SELECT 10 // BTN_SELECT (bit 10)
#define JOY_START 11 // BTN_START (bit 11)

// Retroid uses Nintendo-style layout: A=East, B=South, Y=West, X=North
#define JOY_A 6 // BTN_EAST (bit 6)
#define JOY_B 7 // BTN_SOUTH (bit 7)
#define JOY_X 4 // BTN_NORTH (bit 4)
#define JOY_Y 5 // BTN_WEST (bit 5)

#define JOY_L1 8 // BTN_TL (bit 8)
#define JOY_R1 9 // BTN_TR (bit 9)
#define JOY_L2 JOY_NA // Analog trigger, handled via ABS_HAT2X
#define JOY_R2 JOY_NA // Analog trigger, handled via ABS_HAT2Y
#define JOY_L3 JOY_NA // Repurposed as menu button (see RAW_MENU1)
#define JOY_R3 JOY_NA // Repurposed as menu button (see RAW_MENU2)

#define JOY_MENU 14 // BTN_MODE (bit 14) - Home button
#define JOY_MENU_ALT JOY_NA
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
// Runtime-configurable for device variants
///////////////////////////////

#define SCREEN_DIAGONAL (platform_variant.screen_diagonal)
#define FIXED_WIDTH (platform_variant.screen_width)
#define FIXED_HEIGHT (platform_variant.screen_height)

///////////////////////////////
// HDMI Output Specifications
///////////////////////////////

#define HAS_HDMI 1 // HDMI output supported via USB-C DisplayPort
#define HDMI_WIDTH 1920 // HDMI width in pixels
#define HDMI_HEIGHT 1080 // HDMI height in pixels (1080p)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/storage/lessui" // LessOS default, overridden by LESSOS_STORAGE
#define MUTE_VOLUME_RAW 0 // Raw value for muted volume

///////////////////////////////
// Keymon Configuration
///////////////////////////////

#define KEYMON_BUTTON_MENU 316 // BTN_MODE (Home button)
#define KEYMON_BUTTON_MENU_ALT -1 // Not used
#define KEYMON_BUTTON_PLUS 115 // KEY_VOLUMEUP
#define KEYMON_BUTTON_MINUS 114 // KEY_VOLUMEDOWN

#define KEYMON_HAS_HDMI 1
#define KEYMON_HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

#define KEYMON_HAS_JACK 0 // Headphone jack detection TBD

// Use libudev for dynamic input device discovery
// This is more robust than hardcoding event paths and handles
// devices appearing in any order during boot
#define KEYMON_USE_LIBUDEV 1

///////////////////////////////

#endif
