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
void MinArchPaths_getSRAM(char* filename, const char* saves_dir, const char* game_name);

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
void MinArchPaths_getRTC(char* filename, const char* saves_dir, const char* game_name);

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
void MinArchPaths_getState(char* filename, const char* states_dir, const char* game_name, int slot);

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
void MinArchConfig_getPath(char* filename, const char* config_dir, const char* game_name,
                           const char* device_tag);

/**
 * Chooses BIOS directory path with smart fallback (pure function).
 *
 * Implements intelligent BIOS path selection:
 * - If tag-specific directory (e.g., Bios/GB/) has files, use it
 * - Otherwise fall back to root BIOS directory (Bios/)
 *
 * This allows organized users (separate folders per system) and messy
 * users (all BIOS files in root) to work seamlessly.
 *
 * @param base_bios_path Root BIOS directory (e.g., "/mnt/SDCARD/Bios")
 * @param tag Platform tag (e.g., "GB", "PS", "N64")
 * @param bios_dir Output buffer for selected BIOS directory path
 * @param tag_dir_has_files Whether the tag-specific directory contains files
 */
void MinArchPaths_chooseBios(const char* base_bios_path, const char* tag, char* bios_dir,
                             int tag_dir_has_files);

/**
 * Builds tag-specific BIOS directory path.
 *
 * Format: {base_bios_path}/{tag}
 * Example: "/mnt/SDCARD/Bios/GB"
 *
 * @param base_bios_path Root BIOS directory
 * @param tag Platform tag
 * @param tag_bios_dir Output buffer for tag-specific path
 */
void MinArchPaths_getTagBios(const char* base_bios_path, const char* tag, char* tag_bios_dir);

#endif // __MINARCH_PATHS_H__
