/**
 * log.c - LessUI Logging System Implementation
 *
 * Provides elegant, consistent logging across all LessUI components.
 */

#include "log.h"
#include "log_internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

///////////////////////////////
// Constants
///////////////////////////////

#define LOG_BUFFER_SIZE 2048
#define TIMESTAMP_SIZE 16
#define LOG_MAX_SIZE_DEFAULT (1024 * 1024) // 1MB default rotation size
#define LOG_MAX_BACKUPS_DEFAULT 3

// Log level names for output
static const char* LEVEL_NAMES[] = {
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_WARN] = "WARN",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_DEBUG] = "DEBUG",
};

///////////////////////////////
// Global Log State
///////////////////////////////

static LogFile* g_log_file = NULL; // Global log file (NULL = use stdout/stderr)
static int g_log_sync = 0; // Crash-safe mode: fsync after each write
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER; // Protects g_log_file

///////////////////////////////
// Timestamp Formatting
///////////////////////////////

/**
 * Get current time as compact formatted string (HH:MM:SS).
 *
 * Uses local time for readability. Falls back to zeros if time unavailable.
 */
int log_get_timestamp(char* buf, size_t size) {
	time_t now = time(NULL);
	if (now == (time_t)-1) {
		return snprintf(buf, size, "00:00:00");
	}

	struct tm* tm = localtime(&now);
	if (!tm) {
		return snprintf(buf, size, "00:00:00");
	}

	return snprintf(buf, size, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
}

///////////////////////////////
// Prefix Formatting
///////////////////////////////

/**
 * Format a log message prefix: [HH:MM:SS] [LEVEL] [file:line]
 *
 * File and line are optional (pass NULL/0 to omit).
 */
int log_format_prefix(char* buf, size_t size, LogLevel level, const char* file, int line) {
	char timestamp[TIMESTAMP_SIZE];
	log_get_timestamp(timestamp, sizeof(timestamp));

	const char* level_name = LEVEL_NAMES[level];

	// Extract basename from file path (strip directory)
	const char* basename = file;
	if (file) {
		const char* slash = strrchr(file, '/');
		if (slash)
			basename = slash + 1;
	}

	if (file && line > 0) {
		return snprintf(buf, size, "[%s] [%s] %s:%d ", timestamp, level_name, basename, line);
	} else {
		return snprintf(buf, size, "[%s] [%s] ", timestamp, level_name);
	}
}

///////////////////////////////
// Global Log Initialization
///////////////////////////////

/**
 * Initialize the global logging system.
 */
int log_open(const char* path) {
	// Determine path: explicit > env var > NULL (use stdout)
	const char* log_path = path;
	if (!log_path) {
		log_path = getenv("LOG_FILE");
	}

	// Check for sync mode
	const char* sync_env = getenv("LOG_SYNC");
	int sync_mode = (sync_env && strcmp(sync_env, "1") == 0);

	pthread_mutex_lock(&g_log_mutex);

	// Close existing log if open
	if (g_log_file) {
		log_file_close(g_log_file);
		g_log_file = NULL;
	}

	g_log_sync = sync_mode;

	// If no path specified, stay with stdout/stderr (backward compatible)
	if (!log_path || log_path[0] == '\0') {
		pthread_mutex_unlock(&g_log_mutex);
		return 0;
	}

	// Open the log file with rotation support
	g_log_file = log_file_open(log_path, LOG_MAX_SIZE_DEFAULT, LOG_MAX_BACKUPS_DEFAULT);
	if (!g_log_file) {
		pthread_mutex_unlock(&g_log_mutex);
		// Log to stderr since file logging failed
		fprintf(stderr, "[ERROR] log_open: Failed to open log file: %s\n", log_path);
		return -1;
	}

	// Store sync mode in the LogFile for use during writes
	// Note: We'll handle sync in the write path using g_log_sync

	pthread_mutex_unlock(&g_log_mutex);
	return 0;
}

/**
 * Close the global log file.
 */
void log_close(void) {
	pthread_mutex_lock(&g_log_mutex);

	if (g_log_file) {
		// Final sync before closing (acquire file lock for thread safety)
		pthread_mutex_lock(&g_log_file->lock);
		if (g_log_file->fp) {
			fflush(g_log_file->fp);
			fsync(fileno(g_log_file->fp));
		}
		pthread_mutex_unlock(&g_log_file->lock);

		log_file_close(g_log_file);
		g_log_file = NULL;
	}

	g_log_sync = 0;

	pthread_mutex_unlock(&g_log_mutex);
}

/**
 * Manually sync log file to disk.
 */
void log_sync(void) {
	pthread_mutex_lock(&g_log_mutex);

	if (g_log_file) {
		pthread_mutex_lock(&g_log_file->lock);
		if (g_log_file->fp) {
			fflush(g_log_file->fp);
			fsync(fileno(g_log_file->fp));
		}
		pthread_mutex_unlock(&g_log_file->lock);
	}

	pthread_mutex_unlock(&g_log_mutex);
}

/**
 * Check if global logging is initialized to a file.
 */
int log_is_file_open(void) {
	pthread_mutex_lock(&g_log_mutex);
	int result = (g_log_file != NULL);
	pthread_mutex_unlock(&g_log_mutex);
	return result;
}

///////////////////////////////
// Core Logging Functions
///////////////////////////////

/**
 * Internal: Write to global log file with optional sync.
 *
 * Called with g_log_mutex held.
 */
static void log_write_to_file(LogFile* lf, const char* prefix, const char* message, int do_sync) {
	if (!lf || !lf->fp)
		return;

	char full_message[LOG_BUFFER_SIZE + 256];
	snprintf(full_message, sizeof(full_message), "%s%s", prefix, message);
	size_t msg_len = strlen(full_message);

	pthread_mutex_lock(&lf->lock);

	// Check if rotation needed
	int rotate_ok = 1;
	if (lf->max_size > 0 && lf->current_size + msg_len > lf->max_size) {
		rotate_ok = (log_rotate_file(lf) != -1);
	}

	// Write message (with auto-newline)
	if (rotate_ok && lf->fp) {
		fprintf(lf->fp, "%s\n", full_message);
		fflush(lf->fp);
		lf->current_size += msg_len + 1;

		// Sync to disk if crash-safe mode enabled
		if (do_sync) {
			fsync(fileno(lf->fp));
		}
	}

	pthread_mutex_unlock(&lf->lock);
}

/**
 * Write a log message with context (file:line).
 *
 * If global log file is open, writes there (with optional fsync).
 * Otherwise, ERROR/WARN go to stderr, INFO/DEBUG go to stdout.
 */
void log_write(LogLevel level, const char* file, int line, const char* fmt, ...) {
	char prefix[256];
	char message[LOG_BUFFER_SIZE];

	// Format prefix
	log_format_prefix(prefix, sizeof(prefix), level, file, line);

	// Format message
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	// Write to global log file if available
	pthread_mutex_lock(&g_log_mutex);
	if (g_log_file) {
		log_write_to_file(g_log_file, prefix, message, g_log_sync);
		pthread_mutex_unlock(&g_log_mutex);
		return;
	}
	pthread_mutex_unlock(&g_log_mutex);

	// Fallback: write to stdout/stderr
	FILE* stream = (level <= LOG_LEVEL_WARN) ? stderr : stdout;
	fprintf(stream, "%s%s\n", prefix, message);
	fflush(stream);
}

/**
 * Write a simple log message without file:line context.
 *
 * If global log file is open, writes there (with optional fsync).
 * Otherwise uses stdout/stderr.
 */
void log_write_simple(LogLevel level, const char* fmt, ...) {
	char prefix[256];
	char message[LOG_BUFFER_SIZE];

	// Format prefix (no file:line)
	log_format_prefix(prefix, sizeof(prefix), level, NULL, 0);

	// Format message
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	// Write to global log file if available
	pthread_mutex_lock(&g_log_mutex);
	if (g_log_file) {
		log_write_to_file(g_log_file, prefix, message, g_log_sync);
		pthread_mutex_unlock(&g_log_mutex);
		return;
	}
	pthread_mutex_unlock(&g_log_mutex);

	// Fallback: write to stdout/stderr
	FILE* stream = (level <= LOG_LEVEL_WARN) ? stderr : stdout;
	fprintf(stream, "%s%s\n", prefix, message);
	fflush(stream);
}

///////////////////////////////
// File Logging with Rotation
///////////////////////////////

/**
 * Get current size of an open file.
 */
size_t log_get_file_size(FILE* fp) {
	if (!fp)
		return 0;

	long current_pos = ftell(fp);
	if (current_pos < 0)
		return 0;

	if (fseek(fp, 0, SEEK_END) != 0)
		return 0;

	long size = ftell(fp);
	fseek(fp, current_pos, SEEK_SET);

	return (size < 0) ? 0 : (size_t)size;
}

/**
 * Rotate a log file: file.log -> file.log.1, file.log.1 -> file.log.2, etc.
 *
 * Closes current file, renames backups, reopens new file.
 */
int log_rotate_file(LogFile* lf) {
	if (!lf)
		return -1;

	// Close current file
	if (lf->fp) {
		fclose(lf->fp);
		lf->fp = NULL;
	}

	// Rotate existing backups (from oldest to newest)
	// Delete oldest backup (file.log.N)
	// Rename file.log.N-1 -> file.log.N
	// ...
	// Rename file.log.1 -> file.log.2
	if (lf->max_backups > 0) {
		char old_path[1024];
		char new_path[1024];

		// Delete oldest
		int old_path_len = snprintf(old_path, sizeof(old_path), "%s.%d", lf->path, lf->max_backups);
		if (old_path_len >= 0 && old_path_len < (int)sizeof(old_path)) {
			unlink(old_path);
		}

		// Rename backups
		for (int i = lf->max_backups - 1; i >= 1; i--) {
			int old_len = snprintf(old_path, sizeof(old_path), "%s.%d", lf->path, i);
			int new_len = snprintf(new_path, sizeof(new_path), "%s.%d", lf->path, i + 1);
			if (old_len >= 0 && old_len < (int)sizeof(old_path) && new_len >= 0 &&
			    new_len < (int)sizeof(new_path)) {
				rename(old_path, new_path);
			}
		}

		// Rename current file to .1
		int new_path_len = snprintf(new_path, sizeof(new_path), "%s.1", lf->path);
		if (new_path_len >= 0 && new_path_len < (int)sizeof(new_path)) {
			rename(lf->path, new_path);
		}
	}

	// Open new file
	lf->fp = fopen(lf->path, "a");
	if (!lf->fp) {
		return -1;
	}

	lf->current_size = 0;
	return 0;
}

/**
 * Open a log file with rotation support.
 */
LogFile* log_file_open(const char* path, size_t max_size, int max_backups) {
	if (!path)
		return NULL;

	LogFile* lf = (LogFile*)calloc(1, sizeof(LogFile));
	if (!lf)
		return NULL;

	// Validate path length to prevent truncation
	if (strlen(path) >= sizeof(lf->path)) {
		free(lf);
		return NULL;
	}

	// Copy path
	strncpy(lf->path, path, sizeof(lf->path) - 1);
	lf->path[sizeof(lf->path) - 1] = '\0';

	lf->max_size = max_size;
	lf->max_backups = max_backups;

	// Initialize mutex
	if (pthread_mutex_init(&lf->lock, NULL) != 0) {
		free(lf);
		return NULL;
	}

	// Open file (append mode)
	lf->fp = fopen(path, "a");
	if (!lf->fp) {
		pthread_mutex_destroy(&lf->lock);
		free(lf);
		return NULL;
	}

	// Get current size
	lf->current_size = log_get_file_size(lf->fp);

	return lf;
}

/**
 * Write a message to a LogFile with automatic rotation.
 *
 * Thread-safe. Checks for rotation before writing.
 */
void log_file_write(LogFile* lf, LogLevel level, const char* fmt, ...) {
	if (!lf || !lf->fp)
		return;

	char prefix[256];
	char message[LOG_BUFFER_SIZE];
	char full_message[LOG_BUFFER_SIZE + 256];

	// Format prefix
	log_format_prefix(prefix, sizeof(prefix), level, NULL, 0);

	// Format message
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	// Combine prefix + message
	snprintf(full_message, sizeof(full_message), "%s%s", prefix, message);
	size_t msg_len = strlen(full_message);

	// Lock for thread safety
	pthread_mutex_lock(&lf->lock);

	// Check if rotation needed
	int rotate_ok = 1;
	if (lf->max_size > 0 && lf->current_size + msg_len > lf->max_size) {
		rotate_ok = (log_rotate_file(lf) != -1);
	}

	// Write message (with auto-newline)
	if (rotate_ok && lf->fp) {
		fprintf(lf->fp, "%s\n", full_message);
		fflush(lf->fp);
		lf->current_size += msg_len + 1; // +1 for newline
	}

	pthread_mutex_unlock(&lf->lock);
}

/**
 * Close and free a LogFile.
 */
void log_file_close(LogFile* lf) {
	if (!lf)
		return;

	pthread_mutex_lock(&lf->lock);

	if (lf->fp) {
		fclose(lf->fp);
		lf->fp = NULL;
	}

	pthread_mutex_unlock(&lf->lock);
	pthread_mutex_destroy(&lf->lock);

	free(lf);
}
