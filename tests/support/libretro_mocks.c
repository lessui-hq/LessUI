/**
 * libretro_mocks.c - Mock libretro core API implementation
 *
 * Provides configurable mock implementations of libretro core functions
 * for testing save state and memory persistence code.
 */

#include "libretro_mocks.h"
#include <string.h>

///////////////////////////////
// Global Mock State
///////////////////////////////

MockLibretroCore mock_core;

///////////////////////////////
// State Control Functions
///////////////////////////////

void mock_libretro_reset(void) {
	memset(&mock_core, 0, sizeof(mock_core));
}

void mock_libretro_set_memory(unsigned type, void* data, size_t size) {
	if (type >= MAX_MOCK_MEMORY_REGIONS)
		return;
	mock_core.memory[type].data = data;
	mock_core.memory[type].size = size;
}

void mock_libretro_set_state(size_t size, uint8_t* serialize_buffer, uint8_t* unserialize_buffer) {
	mock_core.serialize_size = size;
	mock_core.serialize_buffer = serialize_buffer;
	mock_core.unserialize_buffer = unserialize_buffer;
}

void mock_libretro_set_serialize_fail(bool should_fail) {
	mock_core.serialize_should_fail = should_fail;
}

void mock_libretro_set_unserialize_fail(bool should_fail) {
	mock_core.unserialize_should_fail = should_fail;
}

///////////////////////////////
// Mock Core Functions
///////////////////////////////

size_t mock_get_memory_size(unsigned type) {
	if (type >= MAX_MOCK_MEMORY_REGIONS)
		return 0;
	return mock_core.memory[type].size;
}

void* mock_get_memory_data(unsigned type) {
	if (type >= MAX_MOCK_MEMORY_REGIONS)
		return NULL;
	return mock_core.memory[type].data;
}

size_t mock_serialize_size(void) {
	return mock_core.serialize_size;
}

bool mock_serialize(void* data, size_t size) {
	if (mock_core.serialize_should_fail)
		return false;
	if (!mock_core.serialize_buffer || size > mock_core.serialize_size)
		return false;

	// Copy from mock buffer to provided buffer
	memcpy(data, mock_core.serialize_buffer, size);
	return true;
}

bool mock_unserialize(const void* data, size_t size) {
	if (mock_core.unserialize_should_fail)
		return false;
	if (!mock_core.unserialize_buffer || size > mock_core.serialize_size)
		return false;

	// Copy from provided buffer to mock buffer (simulating restore)
	memcpy(mock_core.unserialize_buffer, data, size);
	return true;
}
