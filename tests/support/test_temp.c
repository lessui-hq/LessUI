/**
 * test_temp.c - Temporary file and directory management implementation
 *
 * Tracks all temp resources and provides automatic cleanup.
 */

// Feature test macros must come before any includes
#define _GNU_SOURCE       // For mkstemps on Linux
#define _DEFAULT_SOURCE   // For mkdtemp, nftw
#define _XOPEN_SOURCE 700 // For nftw

#include "test_temp.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

///////////////////////////////
// Internal State
///////////////////////////////

typedef enum { TEMP_TYPE_FILE, TEMP_TYPE_DIR } TempType;

typedef struct {
	char path[TEST_TEMP_MAX_PATH];
	TempType type;
	int in_use;
} TempEntry;

static TempEntry tracked_temps[TEST_TEMP_MAX_TRACKED];
static int temp_count = 0;

// Static buffers for returning paths (avoids malloc)
static char path_buffer[TEST_TEMP_MAX_PATH];
static char subdir_buffer[TEST_TEMP_MAX_PATH];
static char file_buffer[TEST_TEMP_MAX_PATH];

///////////////////////////////
// Internal Helpers
///////////////////////////////

/**
 * Track a new temp resource.
 */
static int track_temp(const char* path, TempType type) {
	if (temp_count >= TEST_TEMP_MAX_TRACKED) {
		fprintf(stderr, "test_temp: max tracked resources exceeded (%d)\n", TEST_TEMP_MAX_TRACKED);
		return 0;
	}

	TempEntry* entry = &tracked_temps[temp_count++];
	strncpy(entry->path, path, TEST_TEMP_MAX_PATH - 1);
	entry->path[TEST_TEMP_MAX_PATH - 1] = '\0';
	entry->type = type;
	entry->in_use = 1;

	return 1;
}

/**
 * Callback for nftw to remove files/dirs.
 */
static int remove_callback(const char* path, const struct stat* sb, int typeflag,
                           struct FTW* ftwbuf) {
	(void)sb;
	(void)typeflag;
	(void)ftwbuf;
	return remove(path);
}

/**
 * Recursively remove a directory.
 */
static int rmdir_recursive(const char* path) {
	// nftw with FTW_DEPTH visits children before parents (needed for deletion)
	return nftw(path, remove_callback, 64, FTW_DEPTH | FTW_PHYS);
}

/**
 * Create parent directories for a path.
 */
static int mkdir_parents(const char* path) {
	char tmp[TEST_TEMP_MAX_PATH];
	char* p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);

	// Remove trailing slash
	if (len > 0 && tmp[len - 1] == '/') {
		tmp[len - 1] = '\0';
	}

	// Create each directory component
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
				return 0;
			}
			*p = '/';
		}
	}

	// Create final directory
	if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
		return 0;
	}

	return 1;
}

///////////////////////////////
// Temp File Functions
///////////////////////////////

const char* test_temp_file(const char* suffix) {
	// Create base template
	char base_template[] = "/tmp/test_XXXXXX";
	int fd = mkstemp(base_template);

	if (fd < 0) {
		perror("test_temp_file: mkstemp failed");
		return NULL;
	}

	// If suffix needed, rename the file
	if (suffix && suffix[0]) {
		snprintf(path_buffer, sizeof(path_buffer), "%s%s", base_template, suffix);
		close(fd);
		if (rename(base_template, path_buffer) != 0) {
			unlink(base_template);
			perror("test_temp_file: rename failed");
			return NULL;
		}
	} else {
		strncpy(path_buffer, base_template, sizeof(path_buffer) - 1);
		path_buffer[sizeof(path_buffer) - 1] = '\0';
		close(fd);
	}

	if (!track_temp(path_buffer, TEMP_TYPE_FILE)) {
		unlink(path_buffer);
		return NULL;
	}

	// Return pointer to tracked copy, not the static buffer
	// This ensures the path remains valid even after subsequent calls
	return tracked_temps[temp_count - 1].path;
}

const char* test_temp_file_with_content(const char* suffix, const char* content) {
	const char* path = test_temp_file(suffix);
	if (!path) {
		return NULL;
	}

	FILE* f = fopen(path, "w");
	if (!f) {
		return NULL;
	}

	if (content) {
		if (fputs(content, f) == EOF) {
			fclose(f);
			return NULL;
		}
	}
	fclose(f);

	return path;
}

const char* test_temp_file_with_binary(const char* suffix, const void* data, size_t size) {
	const char* path = test_temp_file(suffix);
	if (!path) {
		return NULL;
	}

	FILE* f = fopen(path, "wb");
	if (!f) {
		return NULL;
	}

	if (data && size > 0) {
		if (fwrite(data, 1, size, f) != size) {
			fclose(f);
			return NULL;
		}
	}
	fclose(f);

	return path;
}

///////////////////////////////
// Temp Directory Functions
///////////////////////////////

const char* test_temp_dir(void) {
	// Create template
	snprintf(path_buffer, sizeof(path_buffer), "/tmp/test_dir_XXXXXX");

	char* result = mkdtemp(path_buffer);
	if (!result) {
		perror("test_temp_dir: mkdtemp failed");
		return NULL;
	}

	if (!track_temp(path_buffer, TEMP_TYPE_DIR)) {
		rmdir(path_buffer);
		return NULL;
	}

	// Return pointer to tracked copy, not the static buffer
	return tracked_temps[temp_count - 1].path;
}

const char* test_temp_subdir(const char* base, const char* subpath) {
	if (!base || !subpath) {
		return NULL;
	}

	snprintf(subdir_buffer, sizeof(subdir_buffer), "%s/%s", base, subpath);

	if (!mkdir_parents(subdir_buffer)) {
		return NULL;
	}

	return subdir_buffer;
}

///////////////////////////////
// Cleanup Functions
///////////////////////////////

void test_temp_cleanup(void) {
	// Clean up in reverse order (dirs after their contents)
	for (int i = temp_count - 1; i >= 0; i--) {
		TempEntry* entry = &tracked_temps[i];
		if (!entry->in_use) {
			continue;
		}

		if (entry->type == TEMP_TYPE_FILE) {
			unlink(entry->path);
		} else if (entry->type == TEMP_TYPE_DIR) {
			rmdir_recursive(entry->path);
		}

		entry->in_use = 0;
	}

	temp_count = 0;
}

int test_temp_count(void) {
	return temp_count;
}

///////////////////////////////
// Utility Functions
///////////////////////////////

int test_temp_is_tracked(const char* path) {
	if (!path) {
		return 0;
	}

	for (int i = 0; i < temp_count; i++) {
		if (tracked_temps[i].in_use && strcmp(tracked_temps[i].path, path) == 0) {
			return 1;
		}
	}

	return 0;
}

const char* test_temp_create_file(const char* dir, const char* filename, const char* content) {
	if (!dir || !filename) {
		return NULL;
	}

	snprintf(file_buffer, sizeof(file_buffer), "%s/%s", dir, filename);

	// Extract directory portion and create it
	char parent[TEST_TEMP_MAX_PATH];
	strncpy(parent, file_buffer, sizeof(parent) - 1);
	parent[sizeof(parent) - 1] = '\0';

	char* last_slash = strrchr(parent, '/');
	if (last_slash && last_slash != parent) {
		*last_slash = '\0';
		if (!mkdir_parents(parent)) {
			return NULL;
		}
	}

	// Create the file
	FILE* f = fopen(file_buffer, "w");
	if (!f) {
		return NULL;
	}

	if (content) {
		fputs(content, f);
	}
	fclose(f);

	return file_buffer;
}
