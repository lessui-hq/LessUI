/**
 * player_mappings.h - Static button/label data for Player
 *
 * Contains all static mapping arrays and label strings used throughout Player:
 * - Button mappings (default, label lookup, device names)
 * - Option label arrays (scaling, effects, sharpness, etc.)
 * - Gamepad type definitions
 *
 * Extracted from player.c for maintainability.
 */

#ifndef PLAYER_MAPPINGS_H
#define PLAYER_MAPPINGS_H

#include "player_input.h"

///////////////////////////////////////
// Constants
///////////////////////////////////////

#define LOCAL_BUTTON_COUNT 16 // Number of physical device buttons
#define RETRO_BUTTON_COUNT 16 // Allow L3/R3 remapping (VirtualBoy uses extras)

///////////////////////////////////////
// Video Scaling Modes
///////////////////////////////////////

typedef enum {
	PLAYER_SCALE_NATIVE, // No scaling, 1:1 pixel mapping (may be cropped)
	PLAYER_SCALE_ASPECT, // Scale maintaining aspect ratio (letterboxed)
	PLAYER_SCALE_FULLSCREEN, // Scale to fill entire screen (may distort)
	PLAYER_SCALE_CROPPED, // Crop to fill screen maintaining aspect ratio
	PLAYER_SCALE_COUNT, // Number of scaling modes
} PlayerScaleMode;

///////////////////////////////////////
// Frontend Option Indices
///////////////////////////////////////

typedef enum {
	FE_OPT_SCALING,
	FE_OPT_EFFECT,
	FE_OPT_SHARPNESS,
	FE_OPT_OVERCLOCK,
	FE_OPT_DEBUG,
	FE_OPT_MAXFF,
	FE_OPT_COUNT,
} PlayerFrontendOption;

///////////////////////////////////////
// Shortcut Indices
///////////////////////////////////////

typedef enum {
	SHORTCUT_SAVE_STATE,
	SHORTCUT_LOAD_STATE,
	SHORTCUT_RESET_GAME,
	SHORTCUT_SAVE_QUIT,
	SHORTCUT_CYCLE_SCALE,
	SHORTCUT_CYCLE_EFFECT,
	SHORTCUT_TOGGLE_FF,
	SHORTCUT_HOLD_FF,
	SHORTCUT_COUNT,
} PlayerShortcut;

///////////////////////////////////////
// Label Arrays (NULL-terminated)
///////////////////////////////////////

extern char* player_onoff_labels[];
extern char* player_scaling_labels[];
extern char* player_effect_labels[];
extern char* player_sharpness_labels[];
extern char* player_max_ff_labels[];
extern char* player_overclock_labels[];
extern char* player_button_labels[];
extern char* player_gamepad_labels[];
extern char* player_gamepad_values[];

///////////////////////////////////////
// Button Mappings
///////////////////////////////////////

/**
 * Default button mapping used when pak.cfg doesn't exist or lacks bindings.
 * Maps standard libretro buttons to device buttons 1:1.
 */
extern PlayerButtonMapping player_default_button_mapping[];

/**
 * Lookup table for parsing button names from config files.
 * Maps button names (e.g., "UP", "A", "L1") to retro_id and local_id.
 */
extern PlayerButtonMapping player_button_label_mapping[];

/**
 * Device button names indexed by BTN_ID_*.
 * Used for UI display and config serialization.
 */
extern const char* player_device_button_names[LOCAL_BUTTON_COUNT];

#endif /* PLAYER_MAPPINGS_H */
