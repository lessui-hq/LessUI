/**
 * player_state.h - Save state system utilities
 *
 * Provides functions to read/write emulator save states to/from disk.
 * Save states are complete snapshots of emulator state (RAM, registers, etc.)
 *
 * Designed for testability with injectable core serialization functions.
 * Extracted from player.c.
 */

#ifndef __PLAYER_STATE_H__
#define __PLAYER_STATE_H__

#include <stdbool.h>
#include <stddef.h>

/**
 * Special slot for auto-resume functionality.
 */
#define PLAYER_AUTO_RESUME_SLOT 9

/**
 * Result codes for state operations.
 */
typedef enum {
	PLAYER_STATE_OK = 0, // Success
	PLAYER_STATE_NO_SUPPORT, // Core doesn't support save states
	PLAYER_STATE_FILE_NOT_FOUND, // State file doesn't exist
	PLAYER_STATE_FILE_ERROR, // File I/O error
	PLAYER_STATE_ALLOC_ERROR, // Memory allocation failed
	PLAYER_STATE_SERIALIZE_ERROR, // Core serialize/unserialize failed
	PLAYER_STATE_SIZE_MISMATCH // File size doesn't match expected size
} PlayerStateResult;

/**
 * Callback function type to get serialize size.
 * @return Size of save state in bytes, 0 if not supported
 */
typedef size_t (*PlayerSerializeSizeFn)(void);

/**
 * Callback function type to serialize state.
 * @param data Buffer to write state into
 * @param size Size of buffer
 * @return true on success, false on failure
 */
typedef bool (*PlayerSerializeFn)(void* data, size_t size);

/**
 * Callback function type to unserialize state.
 * @param data Buffer containing state data
 * @param size Size of state data
 * @return true on success, false on failure
 */
typedef bool (*PlayerUnserializeFn)(const void* data, size_t size);

/**
 * Core interface for state operations.
 */
typedef struct {
	PlayerSerializeSizeFn serialize_size;
	PlayerSerializeFn serialize;
	PlayerUnserializeFn unserialize;
} PlayerStateCore;

/**
 * Reads a save state from disk into the core.
 *
 * @param filepath Path to state file
 * @param core Core interface callbacks
 * @return PLAYER_STATE_OK on success, error code otherwise
 *
 * @note Returns PLAYER_STATE_NO_SUPPORT if core reports size of 0
 * @note Returns PLAYER_STATE_FILE_NOT_FOUND if file doesn't exist
 */
PlayerStateResult PlayerState_read(const char* filepath, const PlayerStateCore* core);

/**
 * Writes current state from core to disk.
 *
 * @param filepath Path to state file
 * @param core Core interface callbacks
 * @return PLAYER_STATE_OK on success, error code otherwise
 *
 * @note Creates file if it doesn't exist
 */
PlayerStateResult PlayerState_write(const char* filepath, const PlayerStateCore* core);

/**
 * Performs auto-save to the auto-resume slot.
 *
 * Convenience function that generates the path for slot 9 and writes state.
 *
 * @param states_dir Directory for state files
 * @param game_name Game name (without extension)
 * @param core Core interface callbacks
 * @return PLAYER_STATE_OK on success, error code otherwise
 */
PlayerStateResult PlayerState_autoSave(const char* states_dir, const char* game_name,
                                       const PlayerStateCore* core);

/**
 * Performs auto-resume from a specified slot.
 *
 * @param states_dir Directory for state files
 * @param game_name Game name (without extension)
 * @param slot Slot number to load from (0-9)
 * @param core Core interface callbacks
 * @return PLAYER_STATE_OK on success, error code otherwise
 */
PlayerStateResult PlayerState_resume(const char* states_dir, const char* game_name, int slot,
                                     const PlayerStateCore* core);

/**
 * Returns human-readable description of result code.
 *
 * @param result Result code from state operation
 * @return String description (static, do not free)
 */
const char* PlayerState_resultString(PlayerStateResult result);

#endif // __PLAYER_STATE_H__
