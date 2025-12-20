/**
 * player_state.c - Save state system utilities
 *
 * Provides functions to read/write emulator save states to/from disk.
 *
 * Extracted from player.c for testability.
 */

#include "player_state.h"
#include "player_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

PlayerStateResult PlayerState_read(const char* filepath, const PlayerStateCore* core) {
	PlayerStateResult result = PLAYER_STATE_OK;
	FILE* state_file = NULL;
	void* state_buffer = NULL;

	// Check if core supports save states
	size_t state_size = core->serialize_size();
	if (!state_size) {
		return PLAYER_STATE_NO_SUPPORT;
	}

	// Allocate buffer for state data
	state_buffer = calloc(1, state_size);
	if (!state_buffer) {
		return PLAYER_STATE_ALLOC_ERROR;
	}

	// Open state file
	state_file = fopen(filepath, "rb");
	if (!state_file) {
		result = PLAYER_STATE_FILE_NOT_FOUND;
		goto cleanup;
	}

	// Read state data from file
	// Allow reading less than expected (some cores report wrong size initially)
	size_t bytes_read = fread(state_buffer, 1, state_size, state_file);
	if (bytes_read == 0 && ferror(state_file)) {
		result = PLAYER_STATE_FILE_ERROR;
		goto cleanup;
	}

	// Restore state into core
	if (!core->unserialize(state_buffer, state_size)) {
		result = PLAYER_STATE_SERIALIZE_ERROR;
		goto cleanup;
	}

cleanup:
	if (state_buffer)
		free(state_buffer);
	if (state_file)
		(void)fclose(state_file); // State file opened for reading

	return result;
}

PlayerStateResult PlayerState_write(const char* filepath, const PlayerStateCore* core) {
	PlayerStateResult result = PLAYER_STATE_OK;
	FILE* state_file = NULL;
	void* state_buffer = NULL;

	// Check if core supports save states
	size_t state_size = core->serialize_size();
	if (!state_size) {
		return PLAYER_STATE_NO_SUPPORT;
	}

	// Allocate buffer for state data
	state_buffer = calloc(1, state_size);
	if (!state_buffer) {
		return PLAYER_STATE_ALLOC_ERROR;
	}

	// Serialize state from core
	if (!core->serialize(state_buffer, state_size)) {
		result = PLAYER_STATE_SERIALIZE_ERROR;
		goto cleanup;
	}

	// Open state file for writing
	state_file = fopen(filepath, "wb");
	if (!state_file) {
		result = PLAYER_STATE_FILE_ERROR;
		goto cleanup;
	}

	// Write state data to file
	if (state_size != fwrite(state_buffer, 1, state_size, state_file)) {
		result = PLAYER_STATE_FILE_ERROR;
		goto cleanup;
	}

cleanup:
	if (state_buffer)
		free(state_buffer);
	if (state_file)
		(void)fclose(state_file); // State file opened for writing

	return result;
}

PlayerStateResult PlayerState_autoSave(const char* states_dir, const char* game_name,
                                       const PlayerStateCore* core) {
	char filepath[MAX_PATH];
	PlayerPaths_getState(filepath, states_dir, game_name, PLAYER_AUTO_RESUME_SLOT);
	return PlayerState_write(filepath, core);
}

PlayerStateResult PlayerState_resume(const char* states_dir, const char* game_name, int slot,
                                     const PlayerStateCore* core) {
	char filepath[MAX_PATH];
	PlayerPaths_getState(filepath, states_dir, game_name, slot);
	return PlayerState_read(filepath, core);
}

const char* PlayerState_resultString(PlayerStateResult result) {
	switch (result) {
	case PLAYER_STATE_OK:
		return "Success";
	case PLAYER_STATE_NO_SUPPORT:
		return "Core does not support save states";
	case PLAYER_STATE_FILE_NOT_FOUND:
		return "State file not found";
	case PLAYER_STATE_FILE_ERROR:
		return "File I/O error";
	case PLAYER_STATE_ALLOC_ERROR:
		return "Memory allocation failed";
	case PLAYER_STATE_SERIALIZE_ERROR:
		return "Core serialization failed";
	case PLAYER_STATE_SIZE_MISMATCH:
		return "State size mismatch";
	default:
		return "Unknown error";
	}
}
