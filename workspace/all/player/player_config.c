/**
 * player_config.c - Configuration option utilities implementation
 *
 * For config path generation (PlayerConfig_getPath), see player_paths.c.
 */

#include "player_config.h"

#include <stdio.h>
#include <string.h>

// Option key to display name mapping table
// Format: {key1, name1, key2, name2, ..., NULL}
static const char* option_key_name_map[] = {"pcsx_rearmed_analog_combo", "DualShock Toggle Combo",
                                            NULL};

const char* PlayerConfig_getOptionDisplayName(const char* key, const char* default_name) {
	if (!key) {
		return default_name;
	}

	// Search the mapping table
	for (int i = 0; option_key_name_map[i]; i += 2) {
		if (strcmp(key, option_key_name_map[i]) == 0) {
			return option_key_name_map[i + 1];
		}
	}

	// No mapping found, return default
	return default_name;
}

int PlayerConfig_getValue(const char* cfg, const char* key, char* out_value, int* lock) {
	if (!cfg || !key || !out_value) {
		return 0;
	}

	const char* tmp = cfg;
	while ((tmp = strstr(tmp, key))) {
		// Check for lock prefix (-key = value)
		if (lock != NULL && tmp > cfg && *(tmp - 1) == '-') {
			*lock = 1;
		}
		tmp += strlen(key);
		// Must match " = " pattern exactly
		if (strncmp(tmp, " = ", 3) == 0) {
			break; // Found valid match
		}
	}

	if (!tmp) {
		return 0;
	}

	// Skip past " = "
	tmp += 3;

	// Copy value up to newline (max 255 chars + null)
	strncpy(out_value, tmp, 256);
	out_value[255] = '\0';

	// Trim at newline or carriage return (handle both \r\n and \n)
	char* nl = strchr(out_value, '\n');
	char* cr = strchr(out_value, '\r');

	// Terminate at whichever comes first
	if (nl && cr) {
		*(nl < cr ? nl : cr) = '\0';
	} else if (nl) {
		*nl = '\0';
	} else if (cr) {
		*cr = '\0';
	}

	return 1;
}

const char* PlayerConfig_getStateDesc(PlayerConfigState state) {
	switch (state) {
	case PLAYER_CONFIG_NONE:
		return "Using defaults.";
	case PLAYER_CONFIG_CONSOLE:
		return "Using console config.";
	case PLAYER_CONFIG_GAME:
		return "Using game config.";
	default:
		return NULL;
	}
}
