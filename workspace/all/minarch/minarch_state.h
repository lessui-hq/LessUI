/**
 * minarch_state.h - Save state system utilities
 *
 * Provides functions to read/write emulator save states to/from disk.
 * Save states are complete snapshots of emulator state (RAM, registers, etc.)
 *
 * Designed for testability with injectable core serialization functions.
 * Extracted from minarch.c.
 */

#ifndef __MINARCH_STATE_H__
#define __MINARCH_STATE_H__

#include <stdbool.h>
#include <stddef.h>

/**
 * Special slot for auto-resume functionality.
 */
#define MINARCH_AUTO_RESUME_SLOT 9

/**
 * Result codes for state operations.
 */
typedef enum {
	MINARCH_STATE_OK = 0, // Success
	MINARCH_STATE_NO_SUPPORT, // Core doesn't support save states
	MINARCH_STATE_FILE_NOT_FOUND, // State file doesn't exist
	MINARCH_STATE_FILE_ERROR, // File I/O error
	MINARCH_STATE_ALLOC_ERROR, // Memory allocation failed
	MINARCH_STATE_SERIALIZE_ERROR, // Core serialize/unserialize failed
	MINARCH_STATE_SIZE_MISMATCH // File size doesn't match expected size
} MinArchStateResult;

/**
 * Callback function type to get serialize size.
 * @return Size of save state in bytes, 0 if not supported
 */
typedef size_t (*MinArchSerializeSizeFn)(void);

/**
 * Callback function type to serialize state.
 * @param data Buffer to write state into
 * @param size Size of buffer
 * @return true on success, false on failure
 */
typedef bool (*MinArchSerializeFn)(void* data, size_t size);

/**
 * Callback function type to unserialize state.
 * @param data Buffer containing state data
 * @param size Size of state data
 * @return true on success, false on failure
 */
typedef bool (*MinArchUnserializeFn)(const void* data, size_t size);

/**
 * Core interface for state operations.
 */
typedef struct {
	MinArchSerializeSizeFn serialize_size;
	MinArchSerializeFn serialize;
	MinArchUnserializeFn unserialize;
} MinArchStateCore;

/**
 * Reads a save state from disk into the core.
 *
 * @param filepath Path to state file
 * @param core Core interface callbacks
 * @return MINARCH_STATE_OK on success, error code otherwise
 *
 * @note Returns MINARCH_STATE_NO_SUPPORT if core reports size of 0
 * @note Returns MINARCH_STATE_FILE_NOT_FOUND if file doesn't exist
 */
MinArchStateResult MinArchState_read(const char* filepath, const MinArchStateCore* core);

/**
 * Writes current state from core to disk.
 *
 * @param filepath Path to state file
 * @param core Core interface callbacks
 * @return MINARCH_STATE_OK on success, error code otherwise
 *
 * @note Creates file if it doesn't exist
 */
MinArchStateResult MinArchState_write(const char* filepath, const MinArchStateCore* core);

/**
 * Performs auto-save to the auto-resume slot.
 *
 * Convenience function that generates the path for slot 9 and writes state.
 *
 * @param states_dir Directory for state files
 * @param game_name Game name (without extension)
 * @param core Core interface callbacks
 * @return MINARCH_STATE_OK on success, error code otherwise
 */
MinArchStateResult MinArchState_autoSave(const char* states_dir, const char* game_name,
                                         const MinArchStateCore* core);

/**
 * Performs auto-resume from a specified slot.
 *
 * @param states_dir Directory for state files
 * @param game_name Game name (without extension)
 * @param slot Slot number to load from (0-9)
 * @param core Core interface callbacks
 * @return MINARCH_STATE_OK on success, error code otherwise
 */
MinArchStateResult MinArchState_resume(const char* states_dir, const char* game_name, int slot,
                                       const MinArchStateCore* core);

/**
 * Returns human-readable description of result code.
 *
 * @param result Result code from state operation
 * @return String description (static, do not free)
 */
const char* MinArchState_resultString(MinArchStateResult result);

#endif // __MINARCH_STATE_H__
