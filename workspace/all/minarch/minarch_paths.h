/**
 * minarch_paths.h - Path generation utilities for MinArch save files
 *
 * Provides functions to generate consistent file paths for save data:
 * - SRAM (battery-backed save RAM, .sav files)
 * - RTC (real-time clock data, .rtc files)
 * - Save states (.st0-.st9 files)
 * - Configuration files (.cfg files)
 *
 * Extracted from minarch.c for testability.
 */

#ifndef __MINARCH_PATHS_H__
#define __MINARCH_PATHS_H__

/**
 * Generates path for SRAM (battery save) file.
 *
 * Format: {saves_dir}/{game_name}.sav
 * Example: "/mnt/SDCARD/.userdata/miyoomini/gpsp/Pokemon.sav"
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param saves_dir Directory for save files
 * @param game_name Game name (without extension)
 */
void MinArch_getSRAMPath(char* filename, const char* saves_dir, const char* game_name);

/**
 * Generates path for RTC (real-time clock) file.
 *
 * Format: {saves_dir}/{game_name}.rtc
 * Example: "/mnt/SDCARD/.userdata/miyoomini/gpsp/Pokemon.rtc"
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param saves_dir Directory for save files
 * @param game_name Game name (without extension)
 */
void MinArch_getRTCPath(char* filename, const char* saves_dir, const char* game_name);

/**
 * Generates path for save state file.
 *
 * Format: {states_dir}/{game_name}.st{slot}
 * Example: "/mnt/SDCARD/.userdata/miyoomini/gpsp/Pokemon.st0"
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param states_dir Directory for state files
 * @param game_name Game name (without extension)
 * @param slot Save state slot number (0-9)
 */
void MinArch_getStatePath(char* filename, const char* states_dir, const char* game_name, int slot);

/**
 * Generates path for configuration file.
 *
 * Format (game-specific): {config_dir}/{game_name}{device_tag}.cfg
 * Format (global): {config_dir}/minarch{device_tag}.cfg
 *
 * Example: "/mnt/SDCARD/.userdata/miyoomini/gpsp/Pokemon-rg35xx.cfg"
 * Example: "/mnt/SDCARD/.userdata/miyoomini/gpsp/minarch-rg35xx.cfg"
 *
 * @param filename Output buffer (min MAX_PATH bytes)
 * @param config_dir Directory for config files
 * @param game_name Game name (NULL for global config)
 * @param device_tag Device-specific tag (NULL or "" if none, e.g., "-rg35xx")
 */
void MinArch_getConfigPath(char* filename, const char* config_dir, const char* game_name,
                           const char* device_tag);

#endif // __MINARCH_PATHS_H__
