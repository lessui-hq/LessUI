/**
 * player_memory.h - SRAM and RTC persistence utilities
 *
 * Provides functions to read/write battery-backed save RAM and
 * real-time clock data to/from disk.
 *
 * Designed for testability with injectable core memory functions.
 * Extracted from player.c.
 */

#ifndef __PLAYER_MEMORY_H__
#define __PLAYER_MEMORY_H__

#include <stddef.h>

/**
 * Memory type constants (matches libretro RETRO_MEMORY_* values).
 */
#define PLAYER_MEMORY_SAVE_RAM 0
#define PLAYER_MEMORY_RTC 1

/**
 * Result codes for memory operations.
 */
typedef enum {
	PLAYER_MEM_OK = 0, // Success
	PLAYER_MEM_NO_SUPPORT, // Core doesn't support this memory type
	PLAYER_MEM_FILE_NOT_FOUND, // File doesn't exist (read only)
	PLAYER_MEM_FILE_ERROR, // File I/O error
	PLAYER_MEM_NULL_POINTER, // Core returned NULL memory pointer
	PLAYER_MEM_SIZE_MISMATCH // File size doesn't match expected size
} PlayerMemoryResult;

/**
 * Callback function type to get memory size.
 * @param type Memory type (PLAYER_MEMORY_SAVE_RAM or PLAYER_MEMORY_RTC)
 * @return Size of memory region in bytes, 0 if not supported
 */
typedef size_t (*PlayerGetMemorySizeFn)(unsigned type);

/**
 * Callback function type to get memory pointer.
 * @param type Memory type (PLAYER_MEMORY_SAVE_RAM or PLAYER_MEMORY_RTC)
 * @return Pointer to memory region, NULL if not supported
 */
typedef void* (*PlayerGetMemoryDataFn)(unsigned type);

/**
 * Reads battery-backed save RAM from disk into core memory.
 *
 * @param filepath Path to .sav file
 * @param get_size Function to get SRAM size from core
 * @param get_data Function to get SRAM pointer from core
 * @return PLAYER_MEM_OK on success, error code otherwise
 *
 * @note Returns PLAYER_MEM_NO_SUPPORT if core reports size of 0
 * @note Returns PLAYER_MEM_FILE_NOT_FOUND if file doesn't exist (not an error)
 */
PlayerMemoryResult PlayerMemory_readSRAM(const char* filepath, PlayerGetMemorySizeFn get_size,
                                         PlayerGetMemoryDataFn get_data);

/**
 * Writes battery-backed save RAM from core memory to disk.
 *
 * @param filepath Path to .sav file
 * @param get_size Function to get SRAM size from core
 * @param get_data Function to get SRAM pointer from core
 * @return PLAYER_MEM_OK on success, error code otherwise
 *
 * @note Returns PLAYER_MEM_NO_SUPPORT if core reports size of 0
 * @note Creates file if it doesn't exist
 */
PlayerMemoryResult PlayerMemory_writeSRAM(const char* filepath, PlayerGetMemorySizeFn get_size,
                                          PlayerGetMemoryDataFn get_data);

/**
 * Reads real-time clock data from disk into core memory.
 *
 * @param filepath Path to .rtc file
 * @param get_size Function to get RTC size from core
 * @param get_data Function to get RTC pointer from core
 * @return PLAYER_MEM_OK on success, error code otherwise
 */
PlayerMemoryResult PlayerMemory_readRTC(const char* filepath, PlayerGetMemorySizeFn get_size,
                                        PlayerGetMemoryDataFn get_data);

/**
 * Writes real-time clock data from core memory to disk.
 *
 * @param filepath Path to .rtc file
 * @param get_size Function to get RTC size from core
 * @param get_data Function to get RTC pointer from core
 * @return PLAYER_MEM_OK on success, error code otherwise
 */
PlayerMemoryResult PlayerMemory_writeRTC(const char* filepath, PlayerGetMemorySizeFn get_size,
                                         PlayerGetMemoryDataFn get_data);

/**
 * Generic memory read function.
 * Reads any memory type from disk into core memory.
 *
 * @param filepath Path to memory file
 * @param memory_type Memory type constant
 * @param get_size Function to get memory size from core
 * @param get_data Function to get memory pointer from core
 * @return PLAYER_MEM_OK on success, error code otherwise
 */
PlayerMemoryResult PlayerMemory_read(const char* filepath, unsigned memory_type,
                                     PlayerGetMemorySizeFn get_size,
                                     PlayerGetMemoryDataFn get_data);

/**
 * Generic memory write function.
 * Writes any memory type from core memory to disk.
 *
 * @param filepath Path to memory file
 * @param memory_type Memory type constant
 * @param get_size Function to get memory size from core
 * @param get_data Function to get memory pointer from core
 * @return PLAYER_MEM_OK on success, error code otherwise
 */
PlayerMemoryResult PlayerMemory_write(const char* filepath, unsigned memory_type,
                                      PlayerGetMemorySizeFn get_size,
                                      PlayerGetMemoryDataFn get_data);

/**
 * Returns human-readable description of result code.
 *
 * @param result Result code from memory operation
 * @return String description (static, do not free)
 */
const char* PlayerMemory_resultString(PlayerMemoryResult result);

#endif // __PLAYER_MEMORY_H__
