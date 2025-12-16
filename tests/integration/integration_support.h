/**
 * integration_support.h - Support utilities for integration tests
 *
 * Provides helper functions for creating realistic test directory structures
 * and test data files for Launcher integration testing.
 */

#ifndef __INTEGRATION_SUPPORT_H__
#define __INTEGRATION_SUPPORT_H__

/**
 * Creates a temporary Launcher directory structure for testing.
 *
 * Creates a directory tree with:
 * - /Roms/              (ROM directories)
 * - /.userdata/.launcher/  (recent games, state)
 * - /Collections/       (custom ROM lists)
 *
 * @param template Template for mkdtemp (e.g., "/tmp/launcher_test_XXXXXX")
 * @return Pointer to template (now contains actual path), or NULL on failure
 *
 * @note Caller must remove directory with rmdir_recursive() when done
 */
char* create_test_launcher_structure(char* template);

/**
 * Creates a test ROM file (empty placeholder).
 *
 * @param path Full path to ROM file
 * @return 1 on success, 0 on failure
 */
int create_test_rom(const char* path);

/**
 * Creates a test M3U file with disc entries.
 *
 * @param path Full path to .m3u file
 * @param disc_names Array of disc filenames (relative to M3U directory)
 * @param disc_count Number of discs
 * @return 1 on success, 0 on failure
 */
int create_test_m3u(const char* path, const char** disc_names, int disc_count);

/**
 * Creates a test map.txt file with ROM aliases.
 *
 * @param path Full path to map.txt
 * @param rom_names Array of ROM filenames
 * @param aliases Array of display names
 * @param count Number of entries
 * @return 1 on success, 0 on failure
 */
int create_test_map(const char* path, const char** rom_names,
                    const char** aliases, int count);

/**
 * Creates a test collection .txt file.
 *
 * @param path Full path to collection file
 * @param rom_paths Array of full ROM paths
 * @param count Number of ROMs
 * @return 1 on success, 0 on failure
 */
int create_test_collection(const char* path, const char** rom_paths, int count);

/**
 * Creates parent directories for a file path.
 *
 * Extracts the directory from a file path and creates it recursively.
 * Example: "/tmp/foo/bar/file.txt" creates "/tmp/foo/bar/"
 *
 * @param file_path Full path to file
 * @return 1 on success, 0 on failure
 */
int create_parent_dir(const char* file_path);

/**
 * Recursively removes a directory and all its contents.
 *
 * @param path Directory to remove
 * @return 1 on success, 0 on failure
 *
 * @warning Destructive operation - use only with test directories!
 */
int rmdir_recursive(const char* path);

#endif // __INTEGRATION_SUPPORT_H__
