/**
 * minarch_config.h - Configuration path and option utilities for MinArch
 *
 * Provides pure utility functions for config file path generation and
 * option value manipulation. These functions have minimal dependencies
 * and can be tested in isolation.
 */

#ifndef MINARCH_CONFIG_H
#define MINARCH_CONFIG_H

/**
 * Generates configuration file path with optional device tag and game override.
 *
 * Produces paths like:
 * - /userdata/platform/core/minarch.cfg (default config)
 * - /userdata/platform/core/minarch-device.cfg (with device tag)
 * - /userdata/platform/core/gamename.cfg (game-specific)
 * - /userdata/platform/core/gamename-device.cfg (game-specific with device)
 *
 * @param output Output buffer for path (should be MAX_PATH size)
 * @param config_dir Base configuration directory path
 * @param game_name Game name (or NULL for default config)
 * @param device_tag Device tag (or NULL for no tag)
 *
 * @example
 *   MinArch_getConfigPath(buf, "/userdata/GB", "Tetris", "rg35xx")
 *   // Result: "/userdata/GB/Tetris-rg35xx.cfg"
 */
void MinArch_getConfigPath(char* output, const char* config_dir, const char* game_name,
                           const char* device_tag);

/**
 * Maps option keys to custom display names.
 *
 * Some cores use cryptic option keys that need friendlier names for display.
 * Returns the mapped name if found, otherwise returns the original name.
 *
 * Current mappings:
 * - "pcsx_rearmed_analog_combo" -> "DualShock Toggle Combo"
 *
 * @param key Option key to look up
 * @param default_name Fallback name if no mapping exists
 * @return Display name (either mapped or default_name)
 *
 * @example
 *   MinArch_getOptionDisplayName("pcsx_rearmed_analog_combo", "Unknown")
 *   // Returns: "DualShock Toggle Combo"
 */
const char* MinArch_getOptionDisplayName(const char* key, const char* default_name);

#endif /* MINARCH_CONFIG_H */
