/**
 * player_input.h - Input handling utilities
 *
 * Provides functions for processing libretro input state queries
 * and button mapping lookups.
 *
 * Designed for testability with explicit state parameters.
 * Extracted from player.c.
 */

#ifndef __PLAYER_INPUT_H__
#define __PLAYER_INPUT_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * Maximum number of buttons supported in libretro.
 */
#define PLAYER_INPUT_MAX_BUTTONS 16

/**
 * Analog axis structure.
 */
typedef struct {
	int16_t x;
	int16_t y;
} PlayerAnalogAxis;

/**
 * Complete input state for a controller.
 */
typedef struct {
	uint32_t buttons; // Bitmask of pressed buttons
	PlayerAnalogAxis left; // Left analog stick
	PlayerAnalogAxis right; // Right analog stick
} PlayerInputState;

/**
 * Button mapping entry.
 *
 * Maps a libretro button to a device button, with optional modifier support.
 * Field names and types match production ButtonMapping in player.c.
 */
typedef struct PlayerButtonMapping {
	char* name; // Display name for UI (may be dynamically allocated)
	int retro_id; // Libretro button ID (RETRO_DEVICE_ID_JOYPAD_*)
	int local_id; // Device button ID (BTN_ID_*)
	int modifier; // Requires MENU held when true
	int default_id; // Default local_id for reset
	int ignore; // Button not available in core
} PlayerButtonMapping;

/**
 * Input descriptor from libretro core.
 */
typedef struct {
	unsigned port;
	unsigned device;
	unsigned index;
	unsigned id;
	const char* description;
} PlayerInputDescriptor;

/**
 * Queries button state from input state.
 *
 * @param state Current input state
 * @param id Button ID to query (RETRO_DEVICE_ID_JOYPAD_*)
 * @return 1 if button pressed, 0 otherwise
 */
int PlayerInput_getButton(const PlayerInputState* state, unsigned id);

/**
 * Queries full button mask from input state.
 *
 * @param state Current input state
 * @return Bitmask of all pressed buttons
 */
uint32_t PlayerInput_getButtonMask(const PlayerInputState* state);

/**
 * Queries analog axis value from input state.
 *
 * @param state Current input state
 * @param index Analog stick index (0=left, 1=right)
 * @param axis_id Axis ID (0=X, 1=Y)
 * @return Axis value (-32768 to 32767)
 */
int16_t PlayerInput_getAnalog(const PlayerInputState* state, unsigned index, unsigned axis_id);

/**
 * Finds a button mapping by libretro ID.
 *
 * @param mappings Array of button mappings (NULL-terminated by name)
 * @param retro_id Libretro button ID to find
 * @return Pointer to mapping, or NULL if not found
 */
const PlayerButtonMapping* PlayerInput_findMappingByRetroId(const PlayerButtonMapping* mappings,
                                                            int retro_id);

/**
 * Finds a button mapping by name.
 *
 * @param mappings Array of button mappings (NULL-terminated by name)
 * @param name Button name to find (case-sensitive)
 * @return Pointer to mapping, or NULL if not found
 */
const PlayerButtonMapping* PlayerInput_findMappingByName(const PlayerButtonMapping* mappings,
                                                         const char* name);

/**
 * Checks if a button is available based on input descriptors.
 *
 * @param descriptors Array of input descriptors (NULL-terminated by description)
 * @param button_id Libretro button ID to check
 * @return true if button is present in descriptors
 */
bool PlayerInput_isButtonAvailable(const PlayerInputDescriptor* descriptors, unsigned button_id);

/**
 * Counts the number of available buttons in descriptors.
 *
 * @param descriptors Array of input descriptors (NULL-terminated)
 * @param max_button_id Maximum button ID to consider
 * @return Number of unique buttons present
 */
int PlayerInput_countAvailableButtons(const PlayerInputDescriptor* descriptors,
                                      unsigned max_button_id);

/**
 * Gets the description for a button from descriptors.
 *
 * @param descriptors Array of input descriptors (NULL-terminated)
 * @param button_id Libretro button ID
 * @return Description string, or NULL if not found
 */
const char* PlayerInput_getButtonDescription(const PlayerInputDescriptor* descriptors,
                                             unsigned button_id);

/**
 * Marks buttons as ignored if not present in descriptors.
 *
 * @param mappings Array of button mappings (modifiable)
 * @param descriptors Array of input descriptors
 * @return Number of buttons marked as ignored
 */
int PlayerInput_markIgnoredButtons(PlayerButtonMapping* mappings,
                                   const PlayerInputDescriptor* descriptors);

/**
 * Resets all button mappings to their default values.
 *
 * @param mappings Array of button mappings
 */
void PlayerInput_resetToDefaults(PlayerButtonMapping* mappings);

/**
 * Validates a button mapping array.
 *
 * @param mappings Array of button mappings
 * @return true if mappings are valid (no duplicate retro_ids, proper termination)
 */
bool PlayerInput_validateMappings(const PlayerButtonMapping* mappings);

/**
 * D-pad button remapping entry.
 *
 * Used to remap d-pad buttons to arrow keys for standard gamepad type.
 */
typedef struct {
	int from_btn; // Source button (BTN_DPAD_*)
	int to_btn; // Target button (BTN_UP/DOWN/LEFT/RIGHT)
} PlayerDpadRemap;

/**
 * Collects button state into retro button bitmask.
 *
 * Iterates through control mappings and builds a bitmask of pressed
 * libretro buttons based on current device button state.
 *
 * Handles:
 * - D-pad to arrow key remapping for standard gamepad (gamepad_type=0)
 * - Modifier key requirements (MENU must be held)
 *
 * @param controls Array of button mappings (NULL-terminated by name)
 * @param pressed_buttons Bitmask of currently pressed device buttons
 * @param menu_pressed Whether MENU modifier is currently pressed
 * @param gamepad_type 0=standard (d-pad→arrows), 1=analog (d-pad direct)
 * @param dpad_remaps Array of d-pad remapping entries (NULL-terminated, or NULL to disable)
 * @param out_used_modifier Set to 1 if any modifier-requiring button was used
 * @return Bitmask of pressed libretro buttons
 */
uint32_t PlayerInput_collectButtons(const PlayerButtonMapping* controls, uint32_t pressed_buttons,
                                    int menu_pressed, int gamepad_type,
                                    const PlayerDpadRemap* dpad_remaps, int* out_used_modifier);

#endif // __PLAYER_INPUT_H__
