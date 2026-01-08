/**
 * defines.h - Platform-specific constants and path definitions for Launcher
 *
 * This file builds upon platform.h to create derived constants used throughout
 * the codebase. All paths are constructed from SDCARD_PATH and PLATFORM macros
 * defined in each platform's platform.h file.
 */

#ifndef __DEFINES_H__
#define __DEFINES_H__

#include "platform.h"

///////////////////////////////
// Hardware setting ranges
///////////////////////////////

/**
 * Volume control range (0 = mute, 20 = maximum).
 */
#define VOLUME_MIN 0
#define VOLUME_MAX 20

/**
 * Brightness control range (0 = dimmest, 10 = brightest).
 */
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 10

/**
 * Maximum path length for all file operations.
 */
#define MAX_PATH 512

///////////////////////////////
// Filesystem paths
///////////////////////////////

/**
 * Root directory for ROM files.
 * Each system has its own subdirectory (e.g., /Roms/GB, /Roms/NES).
 */
#define ROMS_PATH SDCARD_PATH "/Roms"

/**
 * Root directory for system files (shared across platforms).
 */
#define ROOT_SYSTEM_PATH SDCARD_PATH "/.system/"

/**
 * Platform-specific system directory.
 * Contains executables, libraries, and platform-specific resources.
 */
#define SYSTEM_PATH SDCARD_PATH "/.system/" PLATFORM

/**
 * Shared resources directory (graphics, fonts, etc.).
 */
#define RES_PATH SDCARD_PATH "/.system/res"

/**
 * Path to the main UI font file.
 */
#define FONT_PATH RES_PATH "/InterTight-Bold.ttf"

/**
 * Platform-specific user data directory.
 * Stores settings and save states specific to this platform.
 */
#define USERDATA_PATH SDCARD_PATH "/.userdata/" PLATFORM

/**
 * Shared user data directory.
 * Stores settings shared across all platforms on the same SD card.
 */
#define SHARED_USERDATA_PATH SDCARD_PATH "/.userdata/shared"

/**
 * Platform-specific packages directory.
 * Contains .pak bundles for apps and tools.
 */
#define PAKS_PATH SYSTEM_PATH "/paks"

/**
 * Recently played games list (shared across platforms).
 */
#define RECENT_PATH SHARED_USERDATA_PATH "/.launcher/recent.txt"

/**
 * Simple mode enable flag file.
 * If this file exists, Launcher shows a simplified interface.
 */
#define SIMPLE_MODE_PATH SHARED_USERDATA_PATH "/enable-simple-mode"

/**
 * Auto-resume save state tracking file.
 * Stores the last game played for automatic resume on startup.
 */
#define AUTO_RESUME_PATH SHARED_USERDATA_PATH "/.launcher/auto_resume.txt"

/**
 * Save state slot used for auto-resume feature.
 */
#define AUTO_RESUME_SLOT 9

/**
 * Symlink to recently played list (visible to user in file browser).
 */
#define FAUX_RECENT_PATH SDCARD_PATH "/Recently Played"

/**
 * User-created game collections directory.
 */
#define COLLECTIONS_PATH SDCARD_PATH "/Collections"

/**
 * Temporary file storing the last launched ROM path.
 * Used for resume functionality.
 */
#define LAST_PATH "/tmp/last.txt"

/**
 * Temporary file for multi-disc game disc changing.
 * Contains path to the new disc image.
 */
#define CHANGE_DISC_PATH "/tmp/change_disc.txt"

/**
 * Temporary file specifying save state slot to resume from.
 */
#define RESUME_SLOT_PATH "/tmp/resume_slot.txt"

/**
 * Temporary file flag to disable UI overlays during gameplay.
 */
#define NOUI_PATH "/tmp/noui"

///////////////////////////////
// UI color definitions
///////////////////////////////

/**
 * RGB triplets for standard UI colors.
 * These are used to construct SDL_Color or pixel format-specific values.
 */
#define TRIAD_WHITE 0xff, 0xff, 0xff
#define TRIAD_BLACK 0x00, 0x00, 0x00
#define TRIAD_LIGHT_GRAY 0x7f, 0x7f, 0x7f
#define TRIAD_GRAY 0x99, 0x99, 0x99
#define TRIAD_DARK_GRAY 0x26, 0x26, 0x26

#define TRIAD_LIGHT_TEXT 0xcc, 0xcc, 0xcc
#define TRIAD_DARK_TEXT 0x66, 0x66, 0x66

/**
 * SDL_Color macros for text and UI rendering.
 */
#define COLOR_WHITE                                                                                \
	(SDL_Color) {                                                                                  \
		TRIAD_WHITE, 0                                                                             \
	}
#define COLOR_GRAY                                                                                 \
	(SDL_Color) {                                                                                  \
		TRIAD_GRAY, 0                                                                              \
	}
#define COLOR_BLACK                                                                                \
	(SDL_Color) {                                                                                  \
		TRIAD_BLACK, 0                                                                             \
	}
#define COLOR_LIGHT_TEXT                                                                           \
	(SDL_Color) {                                                                                  \
		TRIAD_LIGHT_TEXT, 0                                                                        \
	}
#define COLOR_DARK_TEXT                                                                            \
	(SDL_Color) {                                                                                  \
		TRIAD_DARK_TEXT, 0                                                                         \
	}
#define COLOR_BUTTON_TEXT                                                                          \
	(SDL_Color) {                                                                                  \
		TRIAD_GRAY, 0                                                                              \
	}

///////////////////////////////
// UI layout constants (before scaling)
///////////////////////////////

/**
 * Screen padding in logical pixels.
 *
 * Used for button margin calculations in minput.
 */
#ifndef PADDING
#define PADDING 10
#endif

/**
 * Font sizes in points.
 */
#define FONT_LARGE 16 // Menu items
#define FONT_MEDIUM 14 // Single character button labels
#define FONT_SMALL 12 // Button hints
#define FONT_TINY 10 // Multi-character button labels

///////////////////////////////
// Utility macros
///////////////////////////////

/**
 * Stringification macros.
 *
 * STR(x) converts a macro value to a string literal.
 * Example: STR(PLATFORM) -> "miyoomini"
 */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/**
 * Math utility macros.
 */
#define MAX(a, b) (a) > (b) ? (a) : (b)
#define MIN(a, b) (a) < (b) ? (a) : (b)
#define CEIL_DIV(a, b) ((a) + (b) - 1) / (b) // Integer ceiling division

///////////////////////////////
// Platform capability detection
///////////////////////////////

/**
 * OpenGL ES hardware rendering support.
 * Platforms with GPU capability define HAS_OPENGLES 1 in platform.h.
 * Used by player for hardware-accelerated cores (Flycast, PPSSPP, etc.)
 */
#ifndef HAS_OPENGLES
#define HAS_OPENGLES 0
#endif

/**
 * Detects if platform has a power button.
 */
#define HAS_POWER_BUTTON (BUTTON_POWER != BUTTON_NA || CODE_POWER != CODE_NA || JOY_POWER != JOY_NA)

/**
 * Detects if platform has a dedicated power-off button.
 */
#define HAS_POWEROFF_BUTTON (BUTTON_POWEROFF != BUTTON_NA)

/**
 * Detects if platform has a menu button.
 */
#define HAS_MENU_BUTTON (BUTTON_MENU != BUTTON_NA || CODE_MENU != CODE_NA || JOY_MENU != JOY_NA)

/**
 * Detects if platform has a narrow screen (less than 320px wide).
 * Used to adjust UI layout for small displays.
 */
#define HAS_SKINNY_SCREEN (FIXED_WIDTH < 320)

///////////////////////////////
// Input mapping defaults
///////////////////////////////

/**
 * Sentinel value for "not available" in input mappings.
 * Platforms use this to indicate unsupported buttons/axes.
 */
#define BUTTON_NA -1
#define CODE_NA -1
#define JOY_NA -1
#define AXIS_NA -1

/**
 * Optional power-off button mapping.
 * Not all platforms have a separate power-off button.
 */
#ifndef BUTTON_POWEROFF
#define BUTTON_POWEROFF BUTTON_NA
#endif
#ifndef CODE_POWEROFF
#define CODE_POWEROFF CODE_NA
#endif

/**
 * Alternative menu button mappings.
 * Some platforms have multiple ways to access the menu.
 */
#ifndef BUTTON_MENU_ALT
#define BUTTON_MENU_ALT BUTTON_NA
#endif
#ifndef CODE_MENU_ALT
#define CODE_MENU_ALT CODE_NA
#endif
#ifndef JOY_MENU_ALT
#define JOY_MENU_ALT JOY_NA
#endif

/**
 * Second alternative menu button (for platforms with multiple options).
 */
#ifndef JOY_MENU_ALT2
#define JOY_MENU_ALT2 JOY_NA
#endif

/**
 * Analog trigger axes (L2/R2).
 * Not all devices have analog triggers.
 */
#ifndef AXIS_L2
#define AXIS_L2 AXIS_NA
#define AXIS_R2 AXIS_NA
#endif

/**
 * Analog stick axes.
 * Not all devices have analog sticks.
 */
#ifndef AXIS_LX
#define AXIS_LX AXIS_NA
#define AXIS_LY AXIS_NA
#define AXIS_RX AXIS_NA
#define AXIS_RY AXIS_NA
#endif

///////////////////////////////
// Derived display constants
///////////////////////////////

/**
 * Standard display buffer calculations.
 *
 * Platforms define FIXED_WIDTH and FIXED_HEIGHT in their platform.h:
 * - Fixed hardware: Simple constant (e.g., #define FIXED_WIDTH 640)
 * - Runtime variants: Macro with ternary (e.g., #define FIXED_WIDTH (is_560p ? 752 : 640))
 *
 * These derived constants then calculate pitch and size from those base values.
 * All platforms use RGB565 (2 bytes per pixel, 16-bit depth).
 */
#define FIXED_BPP 2 // Bytes per pixel (RGB565)
#define FIXED_DEPTH (FIXED_BPP * 8) // Bit depth (16-bit color)
#define FIXED_PITCH (FIXED_WIDTH * FIXED_BPP) // Row stride in bytes
#define FIXED_SIZE (FIXED_PITCH * FIXED_HEIGHT) // Total framebuffer size

/**
 * HDMI output buffer calculations.
 * If HAS_HDMI is defined, platform must provide HDMI_WIDTH/HDMI_HEIGHT.
 * Otherwise, HDMI uses the same resolution as the built-in screen.
 */
#ifndef HAS_HDMI
#define HDMI_WIDTH FIXED_WIDTH
#define HDMI_HEIGHT FIXED_HEIGHT
#endif
#define HDMI_PITCH (HDMI_WIDTH * FIXED_BPP) // HDMI row stride
#define HDMI_SIZE (HDMI_PITCH * HDMI_HEIGHT) // HDMI framebuffer size

///////////////////////////////
// Audio configuration
///////////////////////////////

/**
 * SDL audio callback chunk size in samples.
 * At 48kHz/60fps, one video frame = 800 samples.
 *
 * Tradeoff:
 * - 512 samples (~10.7ms latency): More stable, fewer callbacks, better for CPU
 * - 256 samples (~5.3ms latency): Lower latency, more responsive input
 *
 * Default 512 balances stability and responsiveness. Platforms can override.
 */
#ifndef SND_CHUNK_SAMPLES
#define SND_CHUNK_SAMPLES 512
#endif

/**
 * Audio ring buffer size in samples (stereo frames).
 * Controls how much audio is buffered ahead (~133ms at 48kHz, ~8 video frames at 60fps).
 * Matches RetroArch's 128ms default for handheld devices.
 * Provides headroom for CPU frequency scaling and timing variance.
 * Platforms can override this in platform.h if needed.
 */
#ifndef SND_BUFFER_SAMPLES
#define SND_BUFFER_SAMPLES 6400
#endif

/**
 * Rate control proportional gain (d parameter from Arntzen paper).
 * Controls maximum pitch deviation for buffer level compensation.
 * Higher values = more aggressive correction, faster response to jitter.
 * Paper recommends 0.2-0.5%, handhelds typically use 0.5-1.0%.
 * Set to 0.8% - gentler than previous 1.2% since larger buffer provides more headroom.
 * Platforms can override this in platform.h if needed.
 */
#ifndef SND_RATE_CONTROL_D
#define SND_RATE_CONTROL_D 0.008f
#endif

///////////////////////////////
// Button ID enums
///////////////////////////////

#ifndef BTN_A // prevent collisions with input.h in keymon

/**
 * Button ID enumeration (used for array indexing).
 *
 * These IDs are used to index into the PAD_Context.repeat_at array
 * and for other button-specific data structures.
 */
// TODO: doesn't this belong in api.h? it's meaningless without PAD_*
enum {
	BTN_ID_NONE = -1, // No button
	BTN_ID_DPAD_UP, // D-pad up
	BTN_ID_DPAD_DOWN, // D-pad down
	BTN_ID_DPAD_LEFT, // D-pad left
	BTN_ID_DPAD_RIGHT, // D-pad right
	BTN_ID_A, // A button (confirm/select)
	BTN_ID_B, // B button (cancel/back)
	BTN_ID_X, // X button
	BTN_ID_Y, // Y button
	BTN_ID_START, // Start button
	BTN_ID_SELECT, // Select button
	BTN_ID_L1, // Left shoulder button
	BTN_ID_R1, // Right shoulder button
	BTN_ID_L2, // Left trigger
	BTN_ID_R2, // Right trigger
	BTN_ID_L3, // Left stick click
	BTN_ID_R3, // Right stick click
	BTN_ID_MENU, // Menu button
	BTN_ID_PLUS, // Plus/volume up button
	BTN_ID_MINUS, // Minus/volume down button
	BTN_ID_POWER, // Power button (sleep/wake)
	BTN_ID_POWEROFF, // Power off button

	BTN_ID_ANALOG_UP, // Analog stick up (virtual button)
	BTN_ID_ANALOG_DOWN, // Analog stick down (virtual button)
	BTN_ID_ANALOG_LEFT, // Analog stick left (virtual button)
	BTN_ID_ANALOG_RIGHT, // Analog stick right (virtual button)

	BTN_ID_COUNT, // Total number of button IDs
};

/**
 * Button bitmask enumeration (used for button state tracking).
 *
 * These bitmasks are used in PAD_Context fields (is_pressed, just_pressed, etc.)
 * to represent multiple buttons in a single integer.
 */
enum {
	BTN_NONE = 0, // No buttons pressed
	BTN_DPAD_UP = 1 << BTN_ID_DPAD_UP, // D-pad up bitmask
	BTN_DPAD_DOWN = 1 << BTN_ID_DPAD_DOWN, // D-pad down bitmask
	BTN_DPAD_LEFT = 1 << BTN_ID_DPAD_LEFT, // D-pad left bitmask
	BTN_DPAD_RIGHT = 1 << BTN_ID_DPAD_RIGHT, // D-pad right bitmask
	BTN_A = 1 << BTN_ID_A, // A button bitmask
	BTN_B = 1 << BTN_ID_B, // B button bitmask
	BTN_X = 1 << BTN_ID_X, // X button bitmask
	BTN_Y = 1 << BTN_ID_Y, // Y button bitmask
	BTN_START = 1 << BTN_ID_START, // Start button bitmask
	BTN_SELECT = 1 << BTN_ID_SELECT, // Select button bitmask
	BTN_L1 = 1 << BTN_ID_L1, // L1 bitmask
	BTN_R1 = 1 << BTN_ID_R1, // R1 bitmask
	BTN_L2 = 1 << BTN_ID_L2, // L2 bitmask
	BTN_R2 = 1 << BTN_ID_R2, // R2 bitmask
	BTN_L3 = 1 << BTN_ID_L3, // L3 bitmask
	BTN_R3 = 1 << BTN_ID_R3, // R3 bitmask
	BTN_MENU = 1 << BTN_ID_MENU, // Menu button bitmask
	BTN_PLUS = 1 << BTN_ID_PLUS, // Plus button bitmask
	BTN_MINUS = 1 << BTN_ID_MINUS, // Minus button bitmask
	BTN_POWER = 1 << BTN_ID_POWER, // Power button bitmask
	BTN_POWEROFF = 1 << BTN_ID_POWEROFF, // Power-off button bitmask

	BTN_ANALOG_UP = 1 << BTN_ID_ANALOG_UP, // Analog up bitmask
	BTN_ANALOG_DOWN = 1 << BTN_ID_ANALOG_DOWN, // Analog down bitmask
	BTN_ANALOG_LEFT = 1 << BTN_ID_ANALOG_LEFT, // Analog left bitmask
	BTN_ANALOG_RIGHT = 1 << BTN_ID_ANALOG_RIGHT, // Analog right bitmask

	/**
	 * Composite directional masks (combines D-pad and analog).
	 *
	 * Use these for navigation to support both D-pad and analog stick.
	 */
	BTN_UP = BTN_DPAD_UP | BTN_ANALOG_UP,
	BTN_DOWN = BTN_DPAD_DOWN | BTN_ANALOG_DOWN,
	BTN_LEFT = BTN_DPAD_LEFT | BTN_ANALOG_LEFT,
	BTN_RIGHT = BTN_DPAD_RIGHT | BTN_ANALOG_RIGHT,
};
#endif

// Sleep/wake button fallbacks (if not defined by platform)
// These must be after the BTN_* enum to use enum values as defaults
#ifndef BTN_SLEEP
#define BTN_SLEEP BTN_POWER
#endif
#ifndef BTN_WAKE
#define BTN_WAKE BTN_MENU
#endif

#endif