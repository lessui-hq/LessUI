/**
 * player_paths.c - Path generation utilities for Player save files
 *
 * Provides consistent path generation for save data files.
 * Extracted from player.c for testability.
 */

#include "player_paths.h"
#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "utils.h"

/**
 * Generates path for SRAM (battery save) file.
 *
 * Format: {saves_dir}/{game_name}.sav
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param saves_dir Directory for save files
 * @param game_name Game name (without extension)
 */
void PlayerPaths_getSRAM(char* filename, const char* saves_dir, const char* game_name) {
	(void)snprintf(filename, MAX_PATH, "%s/%s.sav", saves_dir, game_name);
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
void PlayerPaths_getRTC(char* filename, const char* saves_dir, const char* game_name) {
	(void)snprintf(filename, MAX_PATH, "%s/%s.rtc", saves_dir, game_name);
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
void PlayerPaths_getState(char* filename, const char* states_dir, const char* game_name, int slot) {
	(void)snprintf(filename, MAX_PATH, "%s/%s.st%i", states_dir, game_name, slot);
}

/**
 * Generates path for configuration file.
 *
 * Format (game-specific): {config_dir}/{game_name}{device_tag}.cfg
 * Format (global): {config_dir}/player{device_tag}.cfg
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param config_dir Directory for config files
 * @param game_name Game name (NULL for global config)
 * @param device_tag Device-specific tag (NULL or "" if none, e.g., "-rg35xx")
 */
void PlayerConfig_getPath(char* filename, const char* config_dir, const char* game_name,
                          const char* device_tag) {
	char device_suffix[64] = {0};

	// Build device tag suffix if provided
	if (device_tag && device_tag[0] != '\0') {
		(void)snprintf(device_suffix, sizeof(device_suffix), "-%s", device_tag);
	}

	// Generate path based on game-specific or global
	// Treat empty string same as NULL (global config)
	if (game_name && game_name[0] != '\0') {
		(void)snprintf(filename, MAX_PATH, "%s/%s%s.cfg", config_dir, game_name, device_suffix);
	} else {
		(void)snprintf(filename, MAX_PATH, "%s/player%s.cfg", config_dir, device_suffix);
	}
}

/**
 * Builds tag-specific BIOS directory path.
 *
 * Format: {base_bios_path}/{tag}
 *
 * @param base_bios_path Root BIOS directory
 * @param tag Platform tag
 * @param tag_bios_dir Output buffer for tag-specific path
 */
void PlayerPaths_getTagBios(const char* base_bios_path, const char* tag, char* tag_bios_dir) {
	(void)snprintf(tag_bios_dir, MAX_PATH, "%s/%s", base_bios_path, tag);
}

/**
 * Chooses BIOS directory path with smart fallback (pure function).
 *
 * If tag directory has files, use it; otherwise fall back to root.
 *
 * @param base_bios_path Root BIOS directory
 * @param tag Platform tag
 * @param bios_dir Output buffer for selected path
 * @param tag_dir_has_files Whether tag directory contains files
 */
void PlayerPaths_chooseBios(const char* base_bios_path, const char* tag, char* bios_dir,
                            int tag_dir_has_files) {
	if (tag_dir_has_files) {
		PlayerPaths_getTagBios(base_bios_path, tag, bios_dir);
	} else {
		safe_strcpy(bios_dir, base_bios_path, MAX_PATH);
	}
}
