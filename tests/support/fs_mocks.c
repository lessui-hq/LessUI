/**
 * fs_mocks.c - File system mock implementation using GCC --wrap
 *
 * Implements an in-memory file system for unit testing.
 * Intercepts exists(), fopen(), fclose(), fgets() via --wrap linker flag.
 *
 * Note: Requires GCC/GNU ld. Works in Docker, not on macOS with clang/ld64.
 */

#include "fs_mocks.h"
#include <stdlib.h>
#include <string.h>

///////////////////////////////
// Mock File System State
///////////////////////////////

#define MAX_MOCK_FILES 100
#define MAX_MOCK_FILE_SIZE 8192

typedef struct MockFile {
	char path[512];
	char content[MAX_MOCK_FILE_SIZE];
	int exists;
} MockFile;

typedef struct MockFileHandle {
	MockFile* file;
	char* read_pos; // Current position for fgets()
	int is_open;
} MockFileHandle;

static struct {
	MockFile files[MAX_MOCK_FILES];
	int file_count;
	MockFileHandle handles[10]; // Up to 10 open files
} mock_fs = {0};

///////////////////////////////
// Mock FS Control Functions
///////////////////////////////

void mock_fs_reset(void) {
	memset(&mock_fs, 0, sizeof(mock_fs));
}

void mock_fs_add_file(const char* path, const char* content) {
	if (mock_fs.file_count >= MAX_MOCK_FILES)
		return;

	MockFile* file = &mock_fs.files[mock_fs.file_count];
	strncpy(file->path, path, sizeof(file->path) - 1);
	strncpy(file->content, content, sizeof(file->content) - 1);
	file->exists = 1;

	mock_fs.file_count++;
}

int mock_fs_exists(const char* path) {
	for (int i = 0; i < mock_fs.file_count; i++) {
		if (strcmp(mock_fs.files[i].path, path) == 0) {
			return mock_fs.files[i].exists;
		}
	}
	return 0;
}

///////////////////////////////
// Helper Functions
///////////////////////////////

static MockFile* find_file(const char* path) {
	for (int i = 0; i < mock_fs.file_count; i++) {
		if (strcmp(mock_fs.files[i].path, path) == 0) {
			return &mock_fs.files[i];
		}
	}
	return NULL;
}

static MockFileHandle* allocate_handle(void) {
	for (int i = 0; i < 10; i++) {
		if (!mock_fs.handles[i].is_open) {
			return &mock_fs.handles[i];
		}
	}
	return NULL;
}

static MockFileHandle* find_handle(FILE* stream) {
	for (int i = 0; i < 10; i++) {
		if (mock_fs.handles[i].is_open && (FILE*)&mock_fs.handles[i] == stream) {
			return &mock_fs.handles[i];
		}
	}
	return NULL;
}

///////////////////////////////
// Wrapped Function Implementations
///////////////////////////////

/**
 * Wrapped exists() - checks mock file system
 */
int __wrap_exists(char* path) {
	return mock_fs_exists(path);
}

/**
 * Wrapped fopen() - opens files from mock file system
 */
FILE* __wrap_fopen(const char* pathname, const char* mode) {
	// Only support read mode
	if (mode[0] != 'r') {
		return NULL; // Write mode not supported (use real files in separate tests)
	}

	MockFile* file = find_file(pathname);
	if (!file) {
		return NULL; // File not found
	}

	MockFileHandle* handle = allocate_handle();
	if (!handle) {
		return NULL; // Too many open files
	}

	handle->file = file;
	handle->read_pos = file->content;
	handle->is_open = 1;

	// Return the handle address as FILE*
	// This is a hack but works for testing
	return (FILE*)handle;
}

/**
 * Wrapped fclose() - closes mock file handle
 */
int __wrap_fclose(FILE* stream) {
	MockFileHandle* handle = find_handle(stream);
	if (!handle) {
		return EOF;
	}

	handle->is_open = 0;
	handle->file = NULL;
	handle->read_pos = NULL;

	return 0;
}

/**
 * Wrapped fgets() - reads lines from mock file
 */
char* __wrap_fgets(char* s, int size, FILE* stream) {
	MockFileHandle* handle = find_handle(stream);
	if (!handle || !handle->read_pos) {
		return NULL;
	}

	// End of file?
	if (*handle->read_pos == '\0') {
		return NULL;
	}

	// Read until newline or size limit
	int i = 0;
	while (i < size - 1 && *handle->read_pos != '\0') {
		s[i] = *handle->read_pos;
		handle->read_pos++;
		i++;

		if (s[i - 1] == '\n') {
			break;
		}
	}

	s[i] = '\0';
	return s;
}
