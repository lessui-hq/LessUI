/**
 * libretro_mocks.h - Mock libretro core API for testing
 *
 * Provides mock implementations of libretro core functions needed for
 * testing save state and memory persistence code.
 *
 * Uses fff (Fake Function Framework) to create configurable mocks.
 *
 * Usage in tests:
 *   #include "libretro_mocks.h"
 *
 *   void setUp(void) {
 *       RESET_LIBRETRO_MOCKS();
 *       mock_libretro_set_memory(RETRO_MEMORY_SAVE_RAM, buffer, 8192);
 *   }
 *
 *   void test_sram_write(void) {
 *       // Test code that calls core.get_memory_size/data
 *   }
 */

#ifndef LIBRETRO_MOCKS_H
#define LIBRETRO_MOCKS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

///////////////////////////////
// Libretro Constants
///////////////////////////////

// Memory types (from libretro.h)
#define RETRO_MEMORY_SAVE_RAM    0
#define RETRO_MEMORY_RTC         1
#define RETRO_MEMORY_SYSTEM_RAM  2
#define RETRO_MEMORY_VIDEO_RAM   3

///////////////////////////////
// Mock State Control
///////////////////////////////

/**
 * Maximum number of memory regions that can be mocked.
 */
#define MAX_MOCK_MEMORY_REGIONS 4

/**
 * Maximum save state size for mocking.
 */
#define MAX_MOCK_STATE_SIZE (1024 * 1024)  // 1MB

/**
 * Structure holding mock memory region data.
 */
typedef struct {
	void* data;
	size_t size;
} MockMemoryRegion;

/**
 * Structure holding mock core state.
 */
typedef struct {
	// Memory regions (indexed by RETRO_MEMORY_* constants)
	MockMemoryRegion memory[MAX_MOCK_MEMORY_REGIONS];

	// Save state
	size_t serialize_size;
	uint8_t* serialize_buffer;   // What serialize() writes to
	uint8_t* unserialize_buffer; // What unserialize() reads from
	bool serialize_should_fail;
	bool unserialize_should_fail;
} MockLibretroCore;

/**
 * Global mock core state.
 */
extern MockLibretroCore mock_core;

/**
 * Resets all mock state to defaults.
 * Call in setUp() before each test.
 */
void mock_libretro_reset(void);

/**
 * Configures mock memory for a specific region type.
 *
 * @param type Memory type (RETRO_MEMORY_SAVE_RAM, RETRO_MEMORY_RTC, etc.)
 * @param data Pointer to memory buffer (owned by test, not copied)
 * @param size Size of memory region in bytes
 */
void mock_libretro_set_memory(unsigned type, void* data, size_t size);

/**
 * Configures mock save state system.
 *
 * @param size Size of save state in bytes
 * @param serialize_buffer Buffer that serialize() will copy TO
 * @param unserialize_buffer Buffer that unserialize() will copy FROM
 */
void mock_libretro_set_state(size_t size, uint8_t* serialize_buffer, uint8_t* unserialize_buffer);

/**
 * Configures serialize() to fail.
 */
void mock_libretro_set_serialize_fail(bool should_fail);

/**
 * Configures unserialize() to fail.
 */
void mock_libretro_set_unserialize_fail(bool should_fail);

///////////////////////////////
// Mock Core Functions
///////////////////////////////

/**
 * Mock implementation of core.get_memory_size().
 */
size_t mock_get_memory_size(unsigned type);

/**
 * Mock implementation of core.get_memory_data().
 */
void* mock_get_memory_data(unsigned type);

/**
 * Mock implementation of core.serialize_size().
 */
size_t mock_serialize_size(void);

/**
 * Mock implementation of core.serialize().
 * Copies mock state TO the provided buffer.
 */
bool mock_serialize(void* data, size_t size);

/**
 * Mock implementation of core.unserialize().
 * Copies FROM mock buffer TO the core state.
 */
bool mock_unserialize(const void* data, size_t size);

///////////////////////////////
// Reset Macro
///////////////////////////////

#define RESET_LIBRETRO_MOCKS() mock_libretro_reset()

#endif // LIBRETRO_MOCKS_H
