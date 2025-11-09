/**
 * integration_support.c - Support utilities for integration tests implementation
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "integration_support.h"

/**
 * Creates nested directories recursively.
 */
static int mkdir_recursive(const char* path) {
	char tmp[512];
	char* p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;

	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
				return 0;
			*p = '/';
		}
	}
	if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
		return 0;

	return 1;
}

char* create_test_minui_structure(char* template) {
	// Create temp directory
	if (!mkdtemp(template))
		return NULL;

	// Create subdirectories
	char path[512];

	// Roms directory
	snprintf(path, sizeof(path), "%s/Roms", template);
	if (!mkdir_recursive(path))
		return NULL;

	// .userdata/.minui directory
	snprintf(path, sizeof(path), "%s/.userdata/.minui", template);
	if (!mkdir_recursive(path))
		return NULL;

	// Collections directory
	snprintf(path, sizeof(path), "%s/Collections", template);
	if (!mkdir_recursive(path))
		return NULL;

	// Emus directory (for emulator paks)
	snprintf(path, sizeof(path), "%s/Emus", template);
	if (!mkdir_recursive(path))
		return NULL;

	// Paks directory (for emulator detection)
	snprintf(path, sizeof(path), "%s/Paks/Emus", template);
	if (!mkdir_recursive(path))
		return NULL;

	return template;
}

int create_test_rom(const char* path) {
	// Create parent directory if needed
	char dir[512];
	snprintf(dir, sizeof(dir), "%s", path);
	char* last_slash = strrchr(dir, '/');
	if (last_slash) {
		*last_slash = '\0';
		if (!mkdir_recursive(dir))
			return 0;
	}

	// Create empty file
	FILE* f = fopen(path, "w");
	if (!f)
		return 0;

	// Write minimal data so file exists and has size
	fwrite("TEST", 1, 4, f);
	fclose(f);
	return 1;
}

int create_test_m3u(const char* path, const char** disc_names, int disc_count) {
	// Create parent directory
	char dir[512];
	snprintf(dir, sizeof(dir), "%s", path);
	char* last_slash = strrchr(dir, '/');
	if (last_slash) {
		*last_slash = '\0';
		if (!mkdir_recursive(dir))
			return 0;
	}

	// Write M3U file
	FILE* f = fopen(path, "w");
	if (!f)
		return 0;

	for (int i = 0; i < disc_count; i++) {
		fprintf(f, "%s\n", disc_names[i]);
	}

	fclose(f);
	return 1;
}

int create_test_map(const char* path, const char** rom_names, const char** aliases,
                    int count) {
	// Create parent directory
	char dir[512];
	snprintf(dir, sizeof(dir), "%s", path);
	char* last_slash = strrchr(dir, '/');
	if (last_slash) {
		*last_slash = '\0';
		if (!mkdir_recursive(dir))
			return 0;
	}

	// Write map.txt file (tab-delimited)
	FILE* f = fopen(path, "w");
	if (!f)
		return 0;

	for (int i = 0; i < count; i++) {
		fprintf(f, "%s\t%s\n", rom_names[i], aliases[i]);
	}

	fclose(f);
	return 1;
}

int create_test_collection(const char* path, const char** rom_paths, int count) {
	// Create parent directory
	char dir[512];
	snprintf(dir, sizeof(dir), "%s", path);
	char* last_slash = strrchr(dir, '/');
	if (last_slash) {
		*last_slash = '\0';
		if (!mkdir_recursive(dir))
			return 0;
	}

	// Write collection file (one ROM path per line)
	FILE* f = fopen(path, "w");
	if (!f)
		return 0;

	for (int i = 0; i < count; i++) {
		fprintf(f, "%s\n", rom_paths[i]);
	}

	fclose(f);
	return 1;
}

int create_parent_dir(const char* file_path) {
	char dir[512];
	snprintf(dir, sizeof(dir), "%s", file_path);

	// Find last slash
	char* last_slash = strrchr(dir, '/');
	if (!last_slash)
		return 0;

	// Truncate to get directory
	*last_slash = '\0';

	// Create directory recursively
	return mkdir_recursive(dir);
}

int rmdir_recursive(const char* path) {
	DIR* d = opendir(path);
	if (!d)
		return 0;

	struct dirent* entry;
	char file_path[512];
	int success = 1;

	while ((entry = readdir(d)) != NULL) {
		// Skip . and ..
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);

		struct stat st;
		if (stat(file_path, &st) != 0) {
			success = 0;
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			// Recursively remove subdirectory
			if (!rmdir_recursive(file_path))
				success = 0;
		} else {
			// Remove file
			if (unlink(file_path) != 0)
				success = 0;
		}
	}

	closedir(d);

	// Remove the directory itself
	if (rmdir(path) != 0)
		success = 0;

	return success;
}
