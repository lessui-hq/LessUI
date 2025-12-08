/**
 * minarch_config.c - Configuration path and option utilities implementation
 */

#include "minarch_config.h"

#include <stdio.h>
#include <string.h>

void MinArch_getConfigPath(char* output, const char* config_dir, const char* game_name,
                           const char* device_tag) {
	char device_suffix[64] = {0};

	// Build device tag suffix if provided
	if (device_tag && device_tag[0] != '\0') {
		snprintf(device_suffix, sizeof(device_suffix), "-%s", device_tag);
	}

	// Generate path based on whether this is game-specific or default config
	if (game_name && game_name[0] != '\0') {
		// Game-specific config: /userdata/GB/Tetris-rg35xx.cfg
		sprintf(output, "%s/%s%s.cfg", config_dir, game_name, device_suffix);
	} else {
		// Default config: /userdata/GB/minarch-rg35xx.cfg
		sprintf(output, "%s/minarch%s.cfg", config_dir, device_suffix);
	}
}

// Option key to display name mapping table
// Format: {key1, name1, key2, name2, ..., NULL}
static const char* option_key_name_map[] = {"pcsx_rearmed_analog_combo", "DualShock Toggle Combo",
                                            NULL};

const char* MinArch_getOptionDisplayName(const char* key, const char* default_name) {
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
