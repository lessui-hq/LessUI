/**
 * minarch_state.c - Save state system utilities
 *
 * Provides functions to read/write emulator save states to/from disk.
 *
 * Extracted from minarch.c for testability.
 */

#include "minarch_state.h"
#include "minarch_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

MinArchStateResult MinArch_readState(const char* filepath, const MinArchStateCore* core) {
	MinArchStateResult result = MINARCH_STATE_OK;
	FILE* state_file = NULL;
	void* state_buffer = NULL;

	// Check if core supports save states
	size_t state_size = core->serialize_size();
	if (!state_size) {
		return MINARCH_STATE_NO_SUPPORT;
	}

	// Allocate buffer for state data
	state_buffer = calloc(1, state_size);
	if (!state_buffer) {
		return MINARCH_STATE_ALLOC_ERROR;
	}

	// Open state file
	state_file = fopen(filepath, "rb");
	if (!state_file) {
		result = MINARCH_STATE_FILE_NOT_FOUND;
		goto cleanup;
	}

	// Read state data from file
	// Allow reading less than expected (some cores report wrong size initially)
	if (state_size < fread(state_buffer, 1, state_size, state_file)) {
		result = MINARCH_STATE_FILE_ERROR;
		goto cleanup;
	}

	// Restore state into core
	if (!core->unserialize(state_buffer, state_size)) {
		result = MINARCH_STATE_SERIALIZE_ERROR;
		goto cleanup;
	}

cleanup:
	if (state_buffer)
		free(state_buffer);
	if (state_file)
		fclose(state_file);

	return result;
}

MinArchStateResult MinArch_writeState(const char* filepath, const MinArchStateCore* core) {
	MinArchStateResult result = MINARCH_STATE_OK;
	FILE* state_file = NULL;
	void* state_buffer = NULL;

	// Check if core supports save states
	size_t state_size = core->serialize_size();
	if (!state_size) {
		return MINARCH_STATE_NO_SUPPORT;
	}

	// Allocate buffer for state data
	state_buffer = calloc(1, state_size);
	if (!state_buffer) {
		return MINARCH_STATE_ALLOC_ERROR;
	}

	// Serialize state from core
	if (!core->serialize(state_buffer, state_size)) {
		result = MINARCH_STATE_SERIALIZE_ERROR;
		goto cleanup;
	}

	// Open state file for writing
	state_file = fopen(filepath, "wb");
	if (!state_file) {
		result = MINARCH_STATE_FILE_ERROR;
		goto cleanup;
	}

	// Write state data to file
	if (state_size != fwrite(state_buffer, 1, state_size, state_file)) {
		result = MINARCH_STATE_FILE_ERROR;
		goto cleanup;
	}

cleanup:
	if (state_buffer)
		free(state_buffer);
	if (state_file)
		fclose(state_file);

	return result;
}

MinArchStateResult MinArch_autoSave(const char* states_dir, const char* game_name,
                                    const MinArchStateCore* core) {
	char filepath[MAX_PATH];
	MinArch_getStatePath(filepath, states_dir, game_name, MINARCH_AUTO_RESUME_SLOT);
	return MinArch_writeState(filepath, core);
}

MinArchStateResult MinArch_resumeState(const char* states_dir, const char* game_name, int slot,
                                       const MinArchStateCore* core) {
	char filepath[MAX_PATH];
	MinArch_getStatePath(filepath, states_dir, game_name, slot);
	return MinArch_readState(filepath, core);
}

const char* MinArch_stateResultString(MinArchStateResult result) {
	switch (result) {
	case MINARCH_STATE_OK:
		return "Success";
	case MINARCH_STATE_NO_SUPPORT:
		return "Core does not support save states";
	case MINARCH_STATE_FILE_NOT_FOUND:
		return "State file not found";
	case MINARCH_STATE_FILE_ERROR:
		return "File I/O error";
	case MINARCH_STATE_ALLOC_ERROR:
		return "Memory allocation failed";
	case MINARCH_STATE_SERIALIZE_ERROR:
		return "Core serialization failed";
	case MINARCH_STATE_SIZE_MISMATCH:
		return "State size mismatch";
	default:
		return "Unknown error";
	}
}
