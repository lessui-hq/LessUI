/**
 * test_temp.h - Standardized temporary file and directory management
 *
 * Provides safe, auto-cleaned temp file/directory creation for tests.
 * All temp resources are tracked and automatically cleaned up in tearDown().
 *
 * Benefits:
 * - No fixed paths (prevents collisions with parallel test runs)
 * - Automatic cleanup (even when tests fail)
 * - Cross-platform (works on Linux and macOS)
 *
 * Usage:
 *   void setUp(void) {
 *       // Optional: test_temp is reset automatically
 *   }
 *
 *   void tearDown(void) {
 *       test_temp_cleanup();  // Clean all temp files/dirs
 *   }
 *
 *   void test_example(void) {
 *       // Create temp file - auto cleaned in tearDown()
 *       const char* path = test_temp_file(".txt");
 *       FILE* f = fopen(path, "w");
 *       fputs("test data", f);
 *       fclose(f);
 *
 *       // Create temp directory - auto cleaned in tearDown()
 *       const char* dir = test_temp_dir();
 *       // ... create files inside dir ...
 *   }
 */

#ifndef TEST_TEMP_H
#define TEST_TEMP_H

#include <stddef.h>

///////////////////////////////
// Configuration
///////////////////////////////

// Maximum number of temp files/dirs that can be tracked per test
#define TEST_TEMP_MAX_TRACKED 64

// Maximum path length for temp files
#define TEST_TEMP_MAX_PATH 512

///////////////////////////////
// Temp File Functions
///////////////////////////////

/**
 * Create a unique temporary file.
 *
 * @param suffix Optional suffix for the file (e.g., ".txt", ".sav")
 *               Pass NULL or "" for no suffix
 * @return Path to the temp file. Do NOT free this pointer.
 *         Returns NULL on error.
 *
 * The returned path is valid until test_temp_cleanup() is called.
 * The file is created empty and ready for writing.
 *
 * Example:
 *   const char* path = test_temp_file(".txt");
 *   FILE* f = fopen(path, "w");
 *   fputs("data", f);
 *   fclose(f);
 */
const char* test_temp_file(const char* suffix);

/**
 * Create a unique temporary file with initial content.
 *
 * @param suffix Optional suffix for the file
 * @param content Initial content to write to the file
 * @return Path to the temp file, or NULL on error
 *
 * Example:
 *   const char* path = test_temp_file_with_content(".m3u", "disc1.bin\ndisc2.bin\n");
 */
const char* test_temp_file_with_content(const char* suffix, const char* content);

/**
 * Create a unique temporary file with binary content.
 *
 * @param suffix Optional suffix for the file
 * @param data Binary data to write
 * @param size Size of the data in bytes
 * @return Path to the temp file, or NULL on error
 *
 * Example:
 *   uint8_t sram[8192] = {0};
 *   const char* path = test_temp_file_with_binary(".sav", sram, sizeof(sram));
 */
const char* test_temp_file_with_binary(const char* suffix, const void* data, size_t size);

///////////////////////////////
// Temp Directory Functions
///////////////////////////////

/**
 * Create a unique temporary directory.
 *
 * @return Path to the temp directory. Do NOT free this pointer.
 *         Returns NULL on error.
 *
 * The directory is created empty and ready for use.
 * Will be recursively deleted by test_temp_cleanup().
 *
 * Example:
 *   const char* dir = test_temp_dir();
 *   char path[512];
 *   snprintf(path, sizeof(path), "%s/test.txt", dir);
 *   touch(path);
 */
const char* test_temp_dir(void);

/**
 * Create a nested directory structure under a temp directory.
 *
 * @param base Base temp directory (from test_temp_dir())
 * @param subpath Relative path to create (e.g., "Roms/GB")
 * @return Full path to the created directory, or NULL on error
 *
 * Example:
 *   const char* base = test_temp_dir();
 *   const char* roms_dir = test_temp_subdir(base, "Roms/GB");
 *   // roms_dir is now "/tmp/test_XXXXXX/Roms/GB"
 */
const char* test_temp_subdir(const char* base, const char* subpath);

///////////////////////////////
// Cleanup Functions
///////////////////////////////

/**
 * Clean up all temporary files and directories created in this test.
 *
 * Call this in tearDown() to ensure cleanup even when tests fail.
 * Safe to call multiple times (idempotent).
 *
 * This function:
 * - Removes all temp files created by test_temp_file()
 * - Recursively removes all temp directories created by test_temp_dir()
 * - Resets internal tracking for the next test
 */
void test_temp_cleanup(void);

/**
 * Get the number of currently tracked temp resources.
 *
 * Useful for debugging temp resource leaks.
 *
 * @return Number of tracked temp files/directories
 */
int test_temp_count(void);

///////////////////////////////
// Utility Functions
///////////////////////////////

/**
 * Check if a path is a temp path managed by test_temp.
 *
 * @param path Path to check
 * @return 1 if path is tracked, 0 otherwise
 */
int test_temp_is_tracked(const char* path);

/**
 * Create a file inside a temp directory.
 *
 * Convenience function combining test_temp_dir() paths with file creation.
 *
 * @param dir Base directory (from test_temp_dir())
 * @param filename Filename to create (can include subdirectories)
 * @param content Content to write
 * @return Full path to the created file, or NULL on error
 *
 * Example:
 *   const char* dir = test_temp_dir();
 *   const char* rom = test_temp_create_file(dir, "Roms/GB/game.gb", "ROM DATA");
 */
const char* test_temp_create_file(const char* dir, const char* filename, const char* content);

#endif // TEST_TEMP_H
