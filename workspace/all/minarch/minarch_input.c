/**
 * minarch_input.c - Input handling utilities
 *
 * Implements input state queries and button mapping lookups.
 * Extracted from minarch.c for testability.
 */

#include "minarch_input.h"

#include <string.h>

///////////////////////////////
// Input State Queries
///////////////////////////////

int MinArchInput_getButton(const MinArchInputState* state, unsigned id) {
	if (!state || id >= 32)
		return 0;
	return (state->buttons >> id) & 1;
}

uint32_t MinArchInput_getButtonMask(const MinArchInputState* state) {
	if (!state)
		return 0;
	return state->buttons;
}

int16_t MinArchInput_getAnalog(const MinArchInputState* state, unsigned index, unsigned axis_id) {
	if (!state)
		return 0;

	const MinArchAnalogAxis* axis = NULL;
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

const MinArchButtonMapping* MinArchInput_findMappingByRetroId(const MinArchButtonMapping* mappings,
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

const MinArchButtonMapping* MinArchInput_findMappingByName(const MinArchButtonMapping* mappings,
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

bool MinArchInput_isButtonAvailable(const MinArchInputDescriptor* descriptors, unsigned button_id) {
	if (!descriptors)
		return false;

	for (int i = 0; descriptors[i].description != NULL; i++) {
		const MinArchInputDescriptor* desc = &descriptors[i];
		// Only check standard joypad buttons on port 0
		if (desc->port == 0 && desc->device == 1 && desc->index == 0) {
			if (desc->id == button_id) {
				return true;
			}
		}
	}
	return false;
}

int MinArchInput_countAvailableButtons(const MinArchInputDescriptor* descriptors,
                                       unsigned max_button_id) {
	if (!descriptors)
		return 0;

	// Track which buttons we've seen
	bool seen[32] = {false};
	int count = 0;

	for (int i = 0; descriptors[i].description != NULL; i++) {
		const MinArchInputDescriptor* desc = &descriptors[i];
		if (desc->port == 0 && desc->device == 1 && desc->index == 0) {
			if (desc->id <= max_button_id && !seen[desc->id]) {
				seen[desc->id] = true;
				count++;
			}
		}
	}
	return count;
}

const char* MinArchInput_getButtonDescription(const MinArchInputDescriptor* descriptors,
                                              unsigned button_id) {
	if (!descriptors)
		return NULL;

	for (int i = 0; descriptors[i].description != NULL; i++) {
		const MinArchInputDescriptor* desc = &descriptors[i];
		if (desc->port == 0 && desc->device == 1 && desc->index == 0) {
			if (desc->id == button_id) {
				return desc->description;
			}
		}
	}
	return NULL;
}

int MinArchInput_markIgnoredButtons(MinArchButtonMapping* mappings,
                                    const MinArchInputDescriptor* descriptors) {
	if (!mappings)
		return 0;

	int ignored_count = 0;

	for (int i = 0; mappings[i].name != NULL; i++) {
		MinArchButtonMapping* mapping = &mappings[i];
		if (mapping->retro_id < 0) {
			// Special mappings with negative IDs are never ignored
			continue;
		}

		if (descriptors && !MinArchInput_isButtonAvailable(descriptors, mapping->retro_id)) {
			mapping->ignore = 1;
			ignored_count++;
		} else {
			mapping->ignore = 0;
		}
	}
	return ignored_count;
}

void MinArchInput_resetToDefaults(MinArchButtonMapping* mappings) {
	if (!mappings)
		return;

	for (int i = 0; mappings[i].name != NULL; i++) {
		mappings[i].local_id = mappings[i].default_id;
		mappings[i].ignore = 0;
	}
}

bool MinArchInput_validateMappings(const MinArchButtonMapping* mappings) {
	if (!mappings)
		return false;

	// Track seen retro_ids to detect duplicates
	bool seen[MINARCH_INPUT_MAX_BUTTONS] = {false};

	for (int i = 0; mappings[i].name != NULL; i++) {
		int retro_id = mappings[i].retro_id;

		// Skip special mappings with negative IDs
		if (retro_id < 0)
			continue;

		// Check for valid range
		if (retro_id >= MINARCH_INPUT_MAX_BUTTONS)
			return false;

		// Check for duplicates
		if (seen[retro_id])
			return false;

		seen[retro_id] = true;
	}

	return true;
}
