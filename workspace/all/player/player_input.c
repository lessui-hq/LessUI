/**
 * player_input.c - Input handling utilities
 *
 * Implements input state queries and button mapping lookups.
 * Extracted from player.c for testability.
 */

#include "player_input.h"

#include <string.h>

///////////////////////////////
// Input State Queries
///////////////////////////////

int PlayerInput_getButton(const PlayerInputState* state, unsigned id) {
	if (!state || id >= 32)
		return 0;
	return (state->buttons >> id) & 1;
}

uint32_t PlayerInput_getButtonMask(const PlayerInputState* state) {
	if (!state)
		return 0;
	return state->buttons;
}

int16_t PlayerInput_getAnalog(const PlayerInputState* state, unsigned index, unsigned axis_id) {
	if (!state)
		return 0;

	const PlayerAnalogAxis* axis = NULL;
	if (index == 0) {
		axis = &state->left;
	} else if (index == 1) {
		axis = &state->right;
	} else {
		return 0;
	}

	if (axis_id == 0) {
		return axis->x;
	} else if (axis_id == 1) {
		return axis->y;
	}
	return 0;
}

///////////////////////////////
// Button Mapping Lookups
///////////////////////////////

const PlayerButtonMapping* PlayerInput_findMappingByRetroId(const PlayerButtonMapping* mappings,
                                                            int retro_id) {
	if (!mappings)
		return NULL;

	for (int i = 0; mappings[i].name != NULL; i++) {
		if (mappings[i].retro_id == retro_id) {
			return &mappings[i];
		}
	}
	return NULL;
}

const PlayerButtonMapping* PlayerInput_findMappingByName(const PlayerButtonMapping* mappings,
                                                         const char* name) {
	if (!mappings || !name)
		return NULL;

	for (int i = 0; mappings[i].name != NULL; i++) {
		if (strcmp(mappings[i].name, name) == 0) {
			return &mappings[i];
		}
	}
	return NULL;
}

///////////////////////////////
// Input Descriptor Processing
///////////////////////////////

bool PlayerInput_isButtonAvailable(const PlayerInputDescriptor* descriptors, unsigned button_id) {
	if (!descriptors)
		return false;

	for (int i = 0; descriptors[i].description != NULL; i++) {
		const PlayerInputDescriptor* desc = &descriptors[i];
		// Only check standard joypad buttons on port 0
		if (desc->port == 0 && desc->device == 1 && desc->index == 0) {
			if (desc->id == button_id) {
				return true;
			}
		}
	}
	return false;
}

int PlayerInput_countAvailableButtons(const PlayerInputDescriptor* descriptors,
                                      unsigned max_button_id) {
	if (!descriptors)
		return 0;

	// Track which buttons we've seen
	bool seen[32] = {false};
	int count = 0;

	for (int i = 0; descriptors[i].description != NULL; i++) {
		const PlayerInputDescriptor* desc = &descriptors[i];
		if (desc->port == 0 && desc->device == 1 && desc->index == 0) {
			if (desc->id <= max_button_id && !seen[desc->id]) {
				seen[desc->id] = true;
				count++;
			}
		}
	}
	return count;
}

const char* PlayerInput_getButtonDescription(const PlayerInputDescriptor* descriptors,
                                             unsigned button_id) {
	if (!descriptors)
		return NULL;

	for (int i = 0; descriptors[i].description != NULL; i++) {
		const PlayerInputDescriptor* desc = &descriptors[i];
		if (desc->port == 0 && desc->device == 1 && desc->index == 0) {
			if (desc->id == button_id) {
				return desc->description;
			}
		}
	}
	return NULL;
}

int PlayerInput_markIgnoredButtons(PlayerButtonMapping* mappings,
                                   const PlayerInputDescriptor* descriptors) {
	if (!mappings)
		return 0;

	int ignored_count = 0;

	for (int i = 0; mappings[i].name != NULL; i++) {
		PlayerButtonMapping* mapping = &mappings[i];
		if (mapping->retro_id < 0) {
			// Special mappings with negative IDs are never ignored
			continue;
		}

		if (descriptors && !PlayerInput_isButtonAvailable(descriptors, mapping->retro_id)) {
			mapping->ignore = 1;
			ignored_count++;
		} else {
			mapping->ignore = 0;
		}
	}
	return ignored_count;
}

void PlayerInput_resetToDefaults(PlayerButtonMapping* mappings) {
	if (!mappings)
		return;

	for (int i = 0; mappings[i].name != NULL; i++) {
		mappings[i].local_id = mappings[i].default_id;
		mappings[i].ignore = 0;
	}
}

bool PlayerInput_validateMappings(const PlayerButtonMapping* mappings) {
	if (!mappings)
		return false;

	// Track seen retro_ids to detect duplicates
	bool seen[PLAYER_INPUT_MAX_BUTTONS] = {false};

	for (int i = 0; mappings[i].name != NULL; i++) {
		int retro_id = mappings[i].retro_id;

		// Skip special mappings with negative IDs
		if (retro_id < 0)
			continue;

		// Check for valid range
		if (retro_id >= PLAYER_INPUT_MAX_BUTTONS)
			return false;

		// Check for duplicates
		if (seen[retro_id])
			return false;

		seen[retro_id] = true;
	}

	return true;
}

///////////////////////////////
// Button State Collection
///////////////////////////////

uint32_t PlayerInput_collectButtons(const PlayerButtonMapping* controls, uint32_t pressed_buttons,
                                    int menu_pressed, int gamepad_type,
                                    const PlayerDpadRemap* dpad_remaps, int* out_used_modifier) {
	if (!controls)
		return 0;

	uint32_t result = 0;
	int used_modifier = 0;

	for (int i = 0; controls[i].name != NULL; i++) {
		const PlayerButtonMapping* mapping = &controls[i];
		int btn = 1 << mapping->local_id;

		if (btn == 1) // BTN_NONE = 0, 1 << 0 = 1
			continue; // not bound

		// Apply d-pad remapping for standard gamepad type
		if (gamepad_type == 0 && dpad_remaps) {
			for (int j = 0; dpad_remaps[j].from_btn != 0; j++) {
				if (btn == dpad_remaps[j].from_btn) {
					btn = dpad_remaps[j].to_btn;
					break;
				}
			}
		}

		// Check if button is pressed and modifier requirement is met
		if ((pressed_buttons & btn) && (!mapping->modifier || menu_pressed)) {
			result |= 1 << mapping->retro_id;
			if (mapping->modifier)
				used_modifier = 1;
		}
	}

	if (out_used_modifier)
		*out_used_modifier = used_modifier;

	return result;
}
