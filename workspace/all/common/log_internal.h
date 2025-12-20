/**
 * log_internal.h - Internal structures and utilities for logging system
 *
 * Private header for log.c implementation. Not for external use.
 */

#ifndef __LOG_INTERNAL_H__
#define __LOG_INTERNAL_H__

#include "log.h"
#include <pthread.h>
#include <stdio.h>

///////////////////////////////
// Internal Structures
///////////////////////////////

/**
 * LogFile structure for direct file logging with rotation.
 *
 * Thread-safe file handle with automatic size-based rotation.
 */
struct LogFile {
	FILE* fp; // Current log file handle
	char path[512]; // Base log file path
	size_t max_size; // Maximum size before rotation (bytes)
	size_t current_size; // Current file size (bytes)
	int max_backups; // Number of backup files to keep
	pthread_mutex_t lock; // Thread safety mutex
};

///////////////////////////////
// Internal Functions
///////////////////////////////

/**
 * Get current time as formatted string.
 *
 * @param buf Buffer to write formatted time (min 16 bytes)
 * @param size Size of buffer
 * @return Number of bytes written
 */
int log_get_timestamp(char* buf, size_t size);

/**
 * Format a log message prefix (timestamp + level + context).
 *
 * @param buf Output buffer
 * @param size Buffer size
 * @param level Log level
 * @param file Source file name (NULL to omit)
 * @param line Source line number (0 to omit)
 * @return Number of bytes written
 */
int log_format_prefix(char* buf, size_t size, LogLevel level, const char* file, int line);

/**
 * Rotate a log file if it exceeds maximum size.
 *
 * Closes current file, renames existing backups (file.log.N -> file.log.N+1),
 * renames current file to file.log.1, and opens new file.
 *
 * @param lf LogFile to rotate
 * @return 0 on success, -1 on failure
 */
int log_rotate_file(LogFile* lf);

/**
 * Get current file size.
 *
 * @param fp File pointer
 * @return File size in bytes, or 0 on error
 */
size_t log_get_file_size(FILE* fp);

#endif // __LOG_INTERNAL_H__
