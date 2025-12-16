/**
 * launcher_state.c - Launcher state persistence utilities
 *
 * Implements navigation state saving/restoration and resume path generation.
 * Extracted from launcher.c for testability.
 */

#include "launcher_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////
// Path Stack Implementation
///////////////////////////////

LauncherPathStack* LauncherPathStack_new(int capacity) {
	LauncherPathStack* stack = malloc(sizeof(LauncherPathStack));
	if (!stack)
		return NULL;

	stack->items = malloc(sizeof(LauncherPathComponent) * capacity);
	if (!stack->items) {
		free(stack);
		return NULL;
	}

	stack->count = 0;
	stack->capacity = capacity;
	return stack;
}

void LauncherPathStack_free(LauncherPathStack* stack) {
	if (!stack)
		return;
	free(stack->items);
	free(stack);
}

bool LauncherPathStack_push(LauncherPathStack* stack, const char* path) {
	if (!stack || !path)
		return false;

	// Grow if needed
	if (stack->count >= stack->capacity) {
		int new_capacity = stack->capacity * 2;
		LauncherPathComponent* new_items =
		    realloc(stack->items, sizeof(LauncherPathComponent) * new_capacity);
		if (!new_items)
			return false;
		stack->items = new_items;
		stack->capacity = new_capacity;
	}

	strncpy(stack->items[stack->count].path, path, LAUNCHER_STATE_MAX_PATH - 1);
	stack->items[stack->count].path[LAUNCHER_STATE_MAX_PATH - 1] = '\0';
	stack->count++;
	return true;
}

bool LauncherPathStack_pop(LauncherPathStack* stack, char* out_path) {
	if (!stack || stack->count == 0)
		return false;

	stack->count--;
	if (out_path) {
		(void)snprintf(out_path, LAUNCHER_STATE_MAX_PATH, "%s", stack->items[stack->count].path);
	}
	return true;
}

///////////////////////////////
// Path Decomposition
///////////////////////////////

LauncherPathStack* LauncherState_decomposePath(const char* full_path, const char* root_path) {
	if (!full_path || !root_path)
		return NULL;

	LauncherPathStack* stack = LauncherPathStack_new(16);
	if (!stack)
		return NULL;

	char path[LAUNCHER_STATE_MAX_PATH];
	strncpy(path, full_path, LAUNCHER_STATE_MAX_PATH - 1);
	path[LAUNCHER_STATE_MAX_PATH - 1] = '\0';

	// Walk up the path tree, pushing each level
	while (strcmp(path, root_path) != 0 && strlen(path) > strlen(root_path)) {
		LauncherPathStack_push(stack, path);

		// Find last slash and truncate
		char* slash = strrchr(path, '/');
		if (!slash || slash == path)
			break;
		*slash = '\0';
	}

	return stack;
}

void LauncherState_extractFilename(const char* full_path, char* out_filename) {
	if (!full_path || !out_filename) {
		if (out_filename)
			out_filename[0] = '\0';
		return;
	}

	const char* slash = strrchr(full_path, '/');
	if (slash) {
		(void)snprintf(out_filename, LAUNCHER_STATE_MAX_PATH, "%s", slash + 1);
	} else {
		(void)snprintf(out_filename, LAUNCHER_STATE_MAX_PATH, "%s", full_path);
	}
}

///////////////////////////////
// Collation Detection
///////////////////////////////

bool LauncherState_isCollatedPath(const char* path) {
	if (!path)
		return false;

	size_t len = strlen(path);
	if (len < 2)
		return false;

	// Check if ends with ")"
	if (path[len - 1] != ')')
		return false;

	// Look for opening parenthesis
	const char* open_paren = strrchr(path, '(');
	if (!open_paren)
		return false;

	// Must have content between parens
	if (open_paren + 1 >= path + len - 1)
		return false;

	return true;
}

bool LauncherState_getCollationPrefix(const char* path, char* out_prefix) {
	if (!out_prefix) {
		return false;
	}
	out_prefix[0] = '\0';

	if (!LauncherState_isCollatedPath(path))
		return false;

	const char* open_paren = strrchr(path, '(');
	if (!open_paren)
		return false;

	// Copy up to and including the opening paren
	size_t prefix_len = (open_paren - path) + 1;
	if (prefix_len >= LAUNCHER_STATE_MAX_PATH)
		prefix_len = LAUNCHER_STATE_MAX_PATH - 1;

	strncpy(out_prefix, path, prefix_len);
	out_prefix[prefix_len] = '\0';
	return true;
}

///////////////////////////////
// Resume Path Generation
///////////////////////////////

void LauncherState_getResumeSlotPath(const char* rom_path, const char* userdata_path,
                                     const char* emu_name, char* out_path) {
	if (!out_path)
		return;
	out_path[0] = '\0';

	if (!rom_path || !userdata_path || !emu_name)
		return;

	// Extract ROM filename
	char rom_file[LAUNCHER_STATE_MAX_PATH];
	LauncherState_extractFilename(rom_path, rom_file);

	(void)snprintf(out_path, LAUNCHER_STATE_MAX_PATH, "%s/.launcher/%s/%s.txt", userdata_path,
	               emu_name, rom_file);
}

void LauncherState_buildResumeCommand(const char* emu_path, const char* rom_path, char* out_cmd) {
	if (!out_cmd)
		return;
	out_cmd[0] = '\0';

	if (!emu_path || !rom_path)
		return;

	// Escape quotes in paths
	char escaped_emu[LAUNCHER_STATE_MAX_PATH * 2];
	char escaped_rom[LAUNCHER_STATE_MAX_PATH * 2];
	LauncherState_escapeQuotes(emu_path, escaped_emu, sizeof(escaped_emu));
	LauncherState_escapeQuotes(rom_path, escaped_rom, sizeof(escaped_rom));

	(void)snprintf(out_cmd, (size_t)LAUNCHER_STATE_MAX_PATH * 2, "'%s' '%s'", escaped_emu,
	               escaped_rom);
}

///////////////////////////////
// Path Validation
///////////////////////////////

bool LauncherState_isRecentsPath(const char* path, const char* recents_path) {
	if (!path || !recents_path)
		return false;
	return strcmp(path, recents_path) == 0;
}

bool LauncherState_validatePath(const char* path, const char* sd_path) {
	if (!path || !sd_path)
		return false;

	// Must start with SD card path
	size_t sd_len = strlen(sd_path);
	if (strncmp(path, sd_path, sd_len) != 0)
		return false;

	// Path must be longer than SD path (has actual content)
	if (strlen(path) <= sd_len)
		return false;

	return true;
}

void LauncherState_makeAbsolutePath(const char* relative_path, const char* sd_path,
                                    char* out_path) {
	if (!out_path)
		return;
	out_path[0] = '\0';

	if (!relative_path || !sd_path)
		return;

	(void)snprintf(out_path, LAUNCHER_STATE_MAX_PATH, "%s%s", sd_path, relative_path);
}

///////////////////////////////
// Quote Escaping
///////////////////////////////

void LauncherState_escapeQuotes(const char* input, char* out_escaped, int out_size) {
	if (!out_escaped || out_size <= 0)
		return;
	out_escaped[0] = '\0';

	if (!input)
		return;

	int out_idx = 0;
	for (int i = 0; input[i] != '\0' && out_idx < out_size - 4; i++) {
		if (input[i] == '\'') {
			// Replace ' with '\''
			out_escaped[out_idx++] = '\'';
			out_escaped[out_idx++] = '\\';
			out_escaped[out_idx++] = '\'';
			out_escaped[out_idx++] = '\'';
		} else {
			out_escaped[out_idx++] = input[i];
		}
	}
	out_escaped[out_idx] = '\0';
}
