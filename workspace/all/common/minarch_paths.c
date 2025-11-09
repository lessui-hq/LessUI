/**
 * minarch_paths.c - Path generation utilities for MinArch save files
 *
 * Provides consistent path generation for save data files.
 * Extracted from minarch.c for testability.
 */

#include "minarch_paths.h"
#include <stdio.h>
#include <string.h>

/**
 * Generates path for SRAM (battery save) file.
 *
 * Format: {saves_dir}/{game_name}.sav
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param saves_dir Directory for save files
 * @param game_name Game name (without extension)
 */
void MinArch_getSRAMPath(char* filename, const char* saves_dir, const char* game_name) {
	sprintf(filename, "%s/%s.sav", saves_dir, game_name);
}

/**
 * Generates path for RTC (real-time clock) file.
 *
 * Format: {saves_dir}/{game_name}.rtc
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param saves_dir Directory for save files
 * @param game_name Game name (without extension)
 */
void MinArch_getRTCPath(char* filename, const char* saves_dir, const char* game_name) {
	sprintf(filename, "%s/%s.rtc", saves_dir, game_name);
}

/**
 * Generates path for save state file.
 *
 * Format: {states_dir}/{game_name}.st{slot}
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param states_dir Directory for state files
 * @param game_name Game name (without extension)
 * @param slot Save state slot number (0-9)
 */
void MinArch_getStatePath(char* filename, const char* states_dir, const char* game_name, int slot) {
	sprintf(filename, "%s/%s.st%i", states_dir, game_name, slot);
}

/**
 * Generates path for configuration file.
 *
 * Format (game-specific): {config_dir}/{game_name}{device_tag}.cfg
 * Format (global): {config_dir}/minarch{device_tag}.cfg
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param config_dir Directory for config files
 * @param game_name Game name (NULL for global config)
 * @param device_tag Device-specific tag (NULL or "" if none, e.g., "-rg35xx")
 */
void MinArch_getConfigPath(char* filename, const char* config_dir, const char* game_name,
                           const char* device_tag) {
	char device_suffix[64] = {0};

	// Build device tag suffix if provided
	if (device_tag && device_tag[0] != '\0') {
		sprintf(device_suffix, "-%s", device_tag);
	}

	// Generate path based on game-specific or global
	if (game_name) {
		sprintf(filename, "%s/%s%s.cfg", config_dir, game_name, device_suffix);
	} else {
		sprintf(filename, "%s/minarch%s.cfg", config_dir, device_suffix);
	}
}
