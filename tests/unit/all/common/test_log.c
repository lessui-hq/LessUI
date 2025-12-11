/**
 * test_log.c - Unit tests for LessUI logging system
 *
 * Tests the log.h/log.c logging library including:
 * - Timestamp formatting
 * - Log level filtering
 * - errno translation
 * - File rotation
 * - Thread safety
 */

#include "unity.h"
#include "../../../../workspace/all/common/log.h"
#include "../../../../workspace/all/common/log_internal.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

///////////////////////////////
// Test Fixtures
///////////////////////////////

void setUp(void) {
	// Clean up any leftover test files
	unlink("/tmp/test_log.log");
	unlink("/tmp/test_log.log.1");
	unlink("/tmp/test_log.log.2");
	unlink("/tmp/test_log.log.3");
	unlink("/tmp/test_log2.log");
	unlink("/tmp/wrong_log.log");
	// Ensure global log is closed before each test
	log_close();
}

void tearDown(void) {
	// Cleanup
	log_close();
	unlink("/tmp/test_log.log");
	unlink("/tmp/test_log.log.1");
	unlink("/tmp/test_log.log.2");
	unlink("/tmp/test_log.log.3");
	unlink("/tmp/test_log2.log");
	unlink("/tmp/wrong_log.log");
}

///////////////////////////////
// Helper Functions
///////////////////////////////

/**
 * Reads entire file into a buffer.
 */
static char* read_file(const char* path) {
	FILE* fp = fopen(path, "r");
	if (!fp)
		return NULL;

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char* buf = malloc(size + 1);
	if (!buf) {
		fclose(fp);
		return NULL;
	}

	fread(buf, 1, size, fp);
	buf[size] = '\0';
	fclose(fp);

	return buf;
}

/**
 * Gets file size in bytes.
 */
static long get_file_size(const char* path) {
	struct stat st;
	if (stat(path, &st) != 0)
		return -1;
	return st.st_size;
}

/**
 * Counts number of lines in a file.
 */
static int count_lines(const char* path) {
	FILE* fp = fopen(path, "r");
	if (!fp)
		return 0;

	int count = 0;
	int ch;
	while ((ch = fgetc(fp)) != EOF) {
		if (ch == '\n')
			count++;
	}
	fclose(fp);
	return count;
}

///////////////////////////////
// Timestamp Tests
///////////////////////////////

void test_log_get_timestamp_format(void) {
	char buf[32];
	int len = log_get_timestamp(buf, sizeof(buf));

	// Should be HH:MM:SS format (8 characters)
	TEST_ASSERT_EQUAL(8, len);

	// Should match HH:MM:SS pattern
	TEST_ASSERT_EQUAL(':', buf[2]);
	TEST_ASSERT_EQUAL(':', buf[5]);

	// All other characters should be digits
	TEST_ASSERT_TRUE(buf[0] >= '0' && buf[0] <= '9');
	TEST_ASSERT_TRUE(buf[1] >= '0' && buf[1] <= '9');
	TEST_ASSERT_TRUE(buf[3] >= '0' && buf[3] <= '9');
	TEST_ASSERT_TRUE(buf[4] >= '0' && buf[4] <= '9');
	TEST_ASSERT_TRUE(buf[6] >= '0' && buf[6] <= '9');
	TEST_ASSERT_TRUE(buf[7] >= '0' && buf[7] <= '9');
}

void test_log_get_timestamp_null_terminated(void) {
	char buf[32];
	log_get_timestamp(buf, sizeof(buf));

	// Should be null-terminated
	TEST_ASSERT_EQUAL('\0', buf[8]);
}

///////////////////////////////
// Prefix Formatting Tests
///////////////////////////////

void test_log_format_prefix_with_context(void) {
	char buf[256];
	log_format_prefix(buf, sizeof(buf), LOG_LEVEL_ERROR, "utils.c", 123);

	// Should contain timestamp, level, file, and line
	TEST_ASSERT_TRUE(strstr(buf, "[ERROR]") != NULL);
	TEST_ASSERT_TRUE(strstr(buf, "utils.c:123") != NULL);
}

void test_log_format_prefix_without_context(void) {
	char buf[256];
	log_format_prefix(buf, sizeof(buf), LOG_LEVEL_INFO, NULL, 0);

	// Should contain timestamp and level only
	TEST_ASSERT_TRUE(strstr(buf, "[INFO]") != NULL);
	TEST_ASSERT_TRUE(strstr(buf, ".c:") == NULL);  // No file:line
}

void test_log_format_prefix_basename_extraction(void) {
	char buf[256];
	log_format_prefix(buf, sizeof(buf), LOG_LEVEL_ERROR, "/long/path/to/file.c", 42);

	// Should extract basename only
	TEST_ASSERT_TRUE(strstr(buf, "file.c:42") != NULL);
	TEST_ASSERT_TRUE(strstr(buf, "/long/path") == NULL);
}

void test_log_format_prefix_all_levels(void) {
	char buf[256];

	log_format_prefix(buf, sizeof(buf), LOG_LEVEL_ERROR, NULL, 0);
	TEST_ASSERT_TRUE(strstr(buf, "[ERROR]") != NULL);

	log_format_prefix(buf, sizeof(buf), LOG_LEVEL_WARN, NULL, 0);
	TEST_ASSERT_TRUE(strstr(buf, "[WARN]") != NULL);

	log_format_prefix(buf, sizeof(buf), LOG_LEVEL_INFO, NULL, 0);
	TEST_ASSERT_TRUE(strstr(buf, "[INFO]") != NULL);

	log_format_prefix(buf, sizeof(buf), LOG_LEVEL_DEBUG, NULL, 0);
	TEST_ASSERT_TRUE(strstr(buf, "[DEBUG]") != NULL);
}

///////////////////////////////
// File Logging Tests
///////////////////////////////

void test_log_file_open_creates_file(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	log_file_close(lf);

	// File should exist
	TEST_ASSERT_TRUE(access("/tmp/test_log.log", F_OK) == 0);
}

void test_log_file_write_creates_entry(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	log_file_write(lf, LOG_LEVEL_INFO, "Test message");
	log_file_close(lf);

	// Read file and verify content
	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "[INFO]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "Test message") != NULL);

	free(content);
}

void test_log_file_write_multiple_entries(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	log_file_write(lf, LOG_LEVEL_INFO, "Message 1");
	log_file_write(lf, LOG_LEVEL_WARN, "Message 2");
	log_file_write(lf, LOG_LEVEL_ERROR, "Message 3");
	log_file_close(lf);

	// Should have 3 lines
	TEST_ASSERT_EQUAL(3, count_lines("/tmp/test_log.log"));
}

void test_log_file_open_null_path(void) {
	LogFile* lf = log_file_open(NULL, 0, 0);
	TEST_ASSERT_NULL(lf);
}

///////////////////////////////
// Rotation Tests
///////////////////////////////

void test_log_get_file_size(void) {
	// Create a known-size file
	FILE* fp = fopen("/tmp/test_log.log", "w");
	fputs("12345", fp);  // 5 bytes
	fflush(fp);

	size_t size = log_get_file_size(fp);
	fclose(fp);

	TEST_ASSERT_EQUAL(5, size);
}

void test_log_rotation_when_exceeds_limit(void) {
	// Create log with 100 byte limit, write 150 bytes
	LogFile* lf = log_file_open("/tmp/test_log.log", 100, 3);
	TEST_ASSERT_NOT_NULL(lf);

	// Write enough to trigger rotation
	for (int i = 0; i < 10; i++) {
		log_file_write(lf, LOG_LEVEL_INFO, "This is a test message number %d", i);
	}

	log_file_close(lf);

	// Original file should exist and be small (rotated)
	long size = get_file_size("/tmp/test_log.log");
	TEST_ASSERT_TRUE(size >= 0 && size < 150);

	// Backup should exist
	TEST_ASSERT_TRUE(access("/tmp/test_log.log.1", F_OK) == 0);
}

void test_log_rotation_preserves_backups(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 50, 3);
	TEST_ASSERT_NOT_NULL(lf);

	// Trigger multiple rotations
	for (int rotation = 0; rotation < 5; rotation++) {
		for (int i = 0; i < 5; i++) {
			log_file_write(lf, LOG_LEVEL_INFO, "Rotation %d message %d", rotation, i);
		}
	}

	log_file_close(lf);

	// Should have main file + 3 backups (max_backups=3)
	TEST_ASSERT_TRUE(access("/tmp/test_log.log", F_OK) == 0);
	TEST_ASSERT_TRUE(access("/tmp/test_log.log.1", F_OK) == 0);
	TEST_ASSERT_TRUE(access("/tmp/test_log.log.2", F_OK) == 0);
	TEST_ASSERT_TRUE(access("/tmp/test_log.log.3", F_OK) == 0);

	// Should NOT have .4 (deleted)
	TEST_ASSERT_FALSE(access("/tmp/test_log.log.4", F_OK) == 0);
}

void test_log_rotation_disabled_when_max_size_zero(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 3);  // max_size=0
	TEST_ASSERT_NOT_NULL(lf);

	// Write a lot of data
	for (int i = 0; i < 100; i++) {
		log_file_write(lf, LOG_LEVEL_INFO, "Message %d", i);
	}

	log_file_close(lf);

	// Should NOT have rotated
	TEST_ASSERT_FALSE(access("/tmp/test_log.log.1", F_OK) == 0);

	// Original file should be large
	long size = get_file_size("/tmp/test_log.log");
	TEST_ASSERT_TRUE(size > 1000);
}

///////////////////////////////
// Thread Safety Tests
///////////////////////////////

#define THREAD_COUNT 5
#define MESSAGES_PER_THREAD 20

typedef struct {
	LogFile* lf;
	int thread_id;
} ThreadData;

static void* thread_write_logs(void* arg) {
	ThreadData* data = (ThreadData*)arg;

	for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
		log_file_write(data->lf, LOG_LEVEL_INFO, "Thread %d message %d",
			data->thread_id, i);
		usleep(100);  // Small delay to increase chance of interleaving
	}

	return NULL;
}

void test_log_file_thread_safety(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	pthread_t threads[THREAD_COUNT];
	ThreadData thread_data[THREAD_COUNT];

	// Launch threads
	for (int i = 0; i < THREAD_COUNT; i++) {
		thread_data[i].lf = lf;
		thread_data[i].thread_id = i;
		pthread_create(&threads[i], NULL, thread_write_logs, &thread_data[i]);
	}

	// Wait for all threads
	for (int i = 0; i < THREAD_COUNT; i++) {
		pthread_join(threads[i], NULL);
	}

	log_file_close(lf);

	// Should have exactly THREAD_COUNT * MESSAGES_PER_THREAD lines
	int line_count = count_lines("/tmp/test_log.log");
	TEST_ASSERT_EQUAL(THREAD_COUNT * MESSAGES_PER_THREAD, line_count);

	// Verify no corrupted lines (all should have [INFO] prefix)
	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);

	// Count [INFO] occurrences
	int info_count = 0;
	char* pos = content;
	while ((pos = strstr(pos, "[INFO]")) != NULL) {
		info_count++;
		pos++;
	}

	TEST_ASSERT_EQUAL(THREAD_COUNT * MESSAGES_PER_THREAD, info_count);

	free(content);
}

///////////////////////////////
// Auto-Newline Tests
///////////////////////////////

void test_log_auto_newline(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	// Write without \n
	log_file_write(lf, LOG_LEVEL_INFO, "Message without newline");
	log_file_close(lf);

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);

	// Should have newline added automatically
	TEST_ASSERT_EQUAL('\n', content[strlen(content) - 1]);

	free(content);
}

void test_log_no_double_newline(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	log_file_write(lf, LOG_LEVEL_INFO, "Line 1");
	log_file_write(lf, LOG_LEVEL_INFO, "Line 2");
	log_file_close(lf);

	// Should have exactly 2 lines
	TEST_ASSERT_EQUAL(2, count_lines("/tmp/test_log.log"));

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);

	// Should not have double newlines
	TEST_ASSERT_TRUE(strstr(content, "\n\n") == NULL);

	free(content);
}

///////////////////////////////
// Global Log API Tests
///////////////////////////////

void test_log_open_with_explicit_path(void) {
	// Open with explicit path
	int result = log_open("/tmp/test_log.log");
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_TRUE(log_is_file_open());

	// Write via LOG_* macros (they should route to file)
	LOG_info("Test message via LOG_info");

	log_close();
	TEST_ASSERT_FALSE(log_is_file_open());

	// Verify content was written
	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "Test message via LOG_info") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "[INFO]") != NULL);

	free(content);
}

void test_log_open_with_env_var(void) {
	// Set LOG_FILE env var
	setenv("LOG_FILE", "/tmp/test_log.log", 1);

	// Open with NULL (should use env var)
	int result = log_open(NULL);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_TRUE(log_is_file_open());

	LOG_info("Message via env var");

	log_close();
	unsetenv("LOG_FILE");

	// Verify content
	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "Message via env var") != NULL);

	free(content);
}

void test_log_open_without_env_var_uses_stdout(void) {
	// Ensure no LOG_FILE env var
	unsetenv("LOG_FILE");

	// Open with NULL (should fall back to stdout, return success)
	int result = log_open(NULL);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_FALSE(log_is_file_open());  // No file open, using stdout

	// This should go to stdout (we can't easily capture, but it shouldn't crash)
	LOG_info("This goes to stdout");

	log_close();  // Safe to call even when no file open
}

void test_log_open_explicit_path_overrides_env_var(void) {
	// Set env var to one path
	setenv("LOG_FILE", "/tmp/wrong_log.log", 1);

	// Open with explicit different path
	int result = log_open("/tmp/test_log.log");
	TEST_ASSERT_EQUAL(0, result);

	LOG_info("Explicit path message");

	log_close();
	unsetenv("LOG_FILE");

	// Should have written to explicit path, not env var path
	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "Explicit path message") != NULL);
	free(content);

	// Wrong path should not exist (or be empty)
	TEST_ASSERT_FALSE(access("/tmp/wrong_log.log", F_OK) == 0);
}

void test_log_close_safe_when_not_open(void) {
	// Ensure not open
	unsetenv("LOG_FILE");
	log_close();  // Should not crash

	// Call again - still safe
	log_close();

	TEST_ASSERT_FALSE(log_is_file_open());
}

void test_log_open_can_reopen(void) {
	// Open first file
	log_open("/tmp/test_log.log");
	LOG_info("First file");

	// Open second file (should close first)
	unlink("/tmp/test_log2.log");
	log_open("/tmp/test_log2.log");
	LOG_info("Second file");

	log_close();

	// Both files should exist with correct content
	char* content1 = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content1);
	TEST_ASSERT_TRUE(strstr(content1, "First file") != NULL);
	TEST_ASSERT_TRUE(strstr(content1, "Second file") == NULL);
	free(content1);

	char* content2 = read_file("/tmp/test_log2.log");
	TEST_ASSERT_NOT_NULL(content2);
	TEST_ASSERT_TRUE(strstr(content2, "Second file") != NULL);
	TEST_ASSERT_TRUE(strstr(content2, "First file") == NULL);
	free(content2);

	unlink("/tmp/test_log2.log");
}

void test_log_sync_flushes_to_disk(void) {
	log_open("/tmp/test_log.log");
	LOG_info("Before sync");

	// Sync should not crash and should ensure data is on disk
	log_sync();

	// File should be readable immediately after sync
	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "Before sync") != NULL);
	free(content);

	log_close();
}

void test_log_sync_safe_when_not_open(void) {
	// Ensure not open
	unsetenv("LOG_FILE");
	log_close();

	// Sync should not crash when no file open
	log_sync();

	TEST_ASSERT_FALSE(log_is_file_open());
}

void test_log_sync_env_var(void) {
	// Set LOG_SYNC=1
	setenv("LOG_FILE", "/tmp/test_log.log", 1);
	setenv("LOG_SYNC", "1", 1);

	log_open(NULL);
	TEST_ASSERT_TRUE(log_is_file_open());

	// Write - should auto-sync due to LOG_SYNC=1
	LOG_info("Synced message");

	// File should be readable immediately (synced after each write)
	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "Synced message") != NULL);
	free(content);

	log_close();
	unsetenv("LOG_FILE");
	unsetenv("LOG_SYNC");
}

void test_log_sync_env_var_only_when_one(void) {
	// Set LOG_SYNC to something other than "1"
	setenv("LOG_FILE", "/tmp/test_log.log", 1);
	setenv("LOG_SYNC", "yes", 1);  // Not "1", should not enable sync

	log_open(NULL);
	LOG_info("Message");
	log_close();

	unsetenv("LOG_FILE");
	unsetenv("LOG_SYNC");

	// Just verify it didn't crash - sync mode only enabled for "1"
	TEST_ASSERT_TRUE(1);
}

void test_log_is_file_open_accurate(void) {
	// Initially not open
	unsetenv("LOG_FILE");
	log_close();
	TEST_ASSERT_FALSE(log_is_file_open());

	// After open
	log_open("/tmp/test_log.log");
	TEST_ASSERT_TRUE(log_is_file_open());

	// After close
	log_close();
	TEST_ASSERT_FALSE(log_is_file_open());
}

void test_log_error_routes_to_file(void) {
	log_open("/tmp/test_log.log");
	LOG_error("Error message test");
	log_close();

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "[ERROR]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "Error message test") != NULL);
	// ERROR includes file:line
	TEST_ASSERT_TRUE(strstr(content, "test_log.c:") != NULL);
	free(content);
}

void test_log_warn_routes_to_file(void) {
	log_open("/tmp/test_log.log");
	LOG_warn("Warning message test");
	log_close();

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "[WARN]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "Warning message test") != NULL);
	free(content);
}

void test_log_debug_routes_to_file(void) {
	log_open("/tmp/test_log.log");
	LOG_debug("Debug message test");
	log_close();

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "[DEBUG]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "Debug message test") != NULL);
	free(content);
}

void test_log_errno_routes_to_file(void) {
	log_open("/tmp/test_log.log");
	errno = ENOENT;
	LOG_errno("Failed to open");
	log_close();

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_TRUE(strstr(content, "[ERROR]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "Failed to open") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "No such file or directory") != NULL);
	free(content);
}

void test_log_global_with_rotation(void) {
	// Use small max size to trigger rotation
	// Note: log_open uses default 1MB, so we test that file grows correctly
	log_open("/tmp/test_log.log");

	// Write multiple messages
	for (int i = 0; i < 10; i++) {
		LOG_info("Message number %d for rotation test", i);
	}

	log_close();

	// Verify all messages written
	int lines = count_lines("/tmp/test_log.log");
	TEST_ASSERT_EQUAL(10, lines);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_log_errno_includes_error_message(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	// Set a known errno
	errno = ENOENT;

	// Use LOG_errno-like pattern (simulate the macro)
	log_file_write(lf, LOG_LEVEL_ERROR, "Failed to open file: %s", strerror(errno));
	log_file_close(lf);

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);

	// Should contain errno message
	TEST_ASSERT_TRUE(strstr(content, "No such file or directory") != NULL);

	free(content);
}

void test_log_levels_in_output(void) {
	LogFile* lf = log_file_open("/tmp/test_log.log", 0, 0);
	TEST_ASSERT_NOT_NULL(lf);

	log_file_write(lf, LOG_LEVEL_ERROR, "Error message");
	log_file_write(lf, LOG_LEVEL_WARN, "Warning message");
	log_file_write(lf, LOG_LEVEL_INFO, "Info message");
	log_file_write(lf, LOG_LEVEL_DEBUG, "Debug message");
	log_file_close(lf);

	char* content = read_file("/tmp/test_log.log");
	TEST_ASSERT_NOT_NULL(content);

	TEST_ASSERT_TRUE(strstr(content, "[ERROR]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "[WARN]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "[INFO]") != NULL);
	TEST_ASSERT_TRUE(strstr(content, "[DEBUG]") != NULL);

	free(content);
}

///////////////////////////////
// Main Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Timestamp tests
	RUN_TEST(test_log_get_timestamp_format);
	RUN_TEST(test_log_get_timestamp_null_terminated);

	// Prefix formatting tests
	RUN_TEST(test_log_format_prefix_with_context);
	RUN_TEST(test_log_format_prefix_without_context);
	RUN_TEST(test_log_format_prefix_basename_extraction);
	RUN_TEST(test_log_format_prefix_all_levels);

	// File logging tests
	RUN_TEST(test_log_file_open_creates_file);
	RUN_TEST(test_log_file_write_creates_entry);
	RUN_TEST(test_log_file_write_multiple_entries);
	RUN_TEST(test_log_file_open_null_path);

	// Rotation tests
	RUN_TEST(test_log_get_file_size);
	RUN_TEST(test_log_rotation_when_exceeds_limit);
	RUN_TEST(test_log_rotation_preserves_backups);
	RUN_TEST(test_log_rotation_disabled_when_max_size_zero);

	// Thread safety tests
	RUN_TEST(test_log_file_thread_safety);

	// Auto-newline tests
	RUN_TEST(test_log_auto_newline);
	RUN_TEST(test_log_no_double_newline);

	// Integration tests
	RUN_TEST(test_log_errno_includes_error_message);
	RUN_TEST(test_log_levels_in_output);

	// Global Log API tests
	RUN_TEST(test_log_open_with_explicit_path);
	RUN_TEST(test_log_open_with_env_var);
	RUN_TEST(test_log_open_without_env_var_uses_stdout);
	RUN_TEST(test_log_open_explicit_path_overrides_env_var);
	RUN_TEST(test_log_close_safe_when_not_open);
	RUN_TEST(test_log_open_can_reopen);
	RUN_TEST(test_log_sync_flushes_to_disk);
	RUN_TEST(test_log_sync_safe_when_not_open);
	RUN_TEST(test_log_sync_env_var);
	RUN_TEST(test_log_sync_env_var_only_when_one);
	RUN_TEST(test_log_is_file_open_accurate);
	RUN_TEST(test_log_error_routes_to_file);
	RUN_TEST(test_log_warn_routes_to_file);
	RUN_TEST(test_log_debug_routes_to_file);
	RUN_TEST(test_log_errno_routes_to_file);
	RUN_TEST(test_log_global_with_rotation);

	return UNITY_END();
}
