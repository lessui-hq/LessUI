/**
 * fs_mocks.h - File system mocking for unit tests
 *
 * Provides mock implementations of file I/O using GCC's --wrap linker flag.
 * This allows testing file-heavy code without actual file system operations.
 *
 * Uses GCC --wrap to intercept:
 * - exists() - MinUI's file existence check
 * - fopen/fclose - File handle operations (read mode only)
 * - fgets - Line-by-line reading
 *
 * Usage in tests:
 *   mock_fs_reset();
 *   mock_fs_add_file("/path/to/file.txt", "line 1\nline 2\n");
 *
 *   // Now exists("/path/to/file.txt") returns 1
 *   // fopen("/path/to/file.txt", "r") returns mock FILE*
 *   // fgets() returns lines from the mock content
 *
 * Compilation:
 *   gcc ... -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets
 *
 * Note on write/directory/binary operations:
 *   For testing file writes (fputs, fprintf, fwrite), directory operations
 *   (opendir, readdir), or binary I/O, use real temp files instead of mocking.
 *   This is more reliable and works across all platforms.
 *
 * Note: Requires GCC/GNU ld (works in Docker, not on macOS with clang)
 */

#ifndef FS_MOCKS_H
#define FS_MOCKS_H

#include <stdio.h>

///////////////////////////////
// Mock File System API
///////////////////////////////

/**
 * Resets the mock file system to empty state.
 * Call this in setUp() before each test.
 */
void mock_fs_reset(void);

/**
 * Adds a file to the mock file system with given content.
 *
 * @param path File path (e.g., "/mnt/SDCARD/test.txt")
 * @param content File content as string (can include newlines)
 *
 * Example:
 *   mock_fs_add_file("/data/map.txt", "mario.gb\tSuper Mario\nzelda.gb\tZelda\n");
 */
void mock_fs_add_file(const char* path, const char* content);

/**
 * Checks if a file exists in the mock file system.
 *
 * @param path File path to check
 * @return 1 if file exists, 0 otherwise
 */
int mock_fs_exists(const char* path);

///////////////////////////////
// Wrapped Functions
// These are provided by fs_mocks.c and intercept real libc calls via --wrap
///////////////////////////////

// When compiling with -Wl,--wrap=exists, calls to exists() go to __wrap_exists()
// __wrap_exists can call __real_exists() to get the real implementation
int __wrap_exists(char* path);

// When compiling with -Wl,--wrap=fopen
FILE* __wrap_fopen(const char* pathname, const char* mode);

// When compiling with -Wl,--wrap=fclose
int __wrap_fclose(FILE* stream);

// When compiling with -Wl,--wrap=fgets
char* __wrap_fgets(char* s, int size, FILE* stream);

#endif // FS_MOCKS_H
