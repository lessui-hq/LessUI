/**
 * minarch_memory.c - SRAM and RTC persistence utilities
 *
 * Provides functions to read/write battery-backed save RAM and
 * real-time clock data to/from disk.
 *
 * Extracted from minarch.c for testability.
 */

#include "minarch_memory.h"
#include <errno.h>
#include <stdio.h>

MinArchMemoryResult MinArch_readMemory(const char* filepath, unsigned memory_type,
                                       MinArchGetMemorySizeFn get_size,
                                       MinArchGetMemoryDataFn get_data) {
	// Check if core supports this memory type
	size_t mem_size = get_size(memory_type);
	if (!mem_size) {
		return MINARCH_MEM_NO_SUPPORT;
	}

	// Try to open the file
	FILE* file = fopen(filepath, "rb");
	if (!file) {
		// File not found is a normal case (no save exists yet)
		return MINARCH_MEM_FILE_NOT_FOUND;
	}

	// Get memory pointer from core
	void* mem_data = get_data(memory_type);
	if (!mem_data) {
		fclose(file);
		return MINARCH_MEM_NULL_POINTER;
	}

	// Read data into memory
	size_t bytes_read = fread(mem_data, 1, mem_size, file);
	fclose(file);

	// Allow partial reads (some games save different sizes)
	if (bytes_read == 0) {
		return MINARCH_MEM_FILE_ERROR;
	}

	return MINARCH_MEM_OK;
}

MinArchMemoryResult MinArch_writeMemory(const char* filepath, unsigned memory_type,
                                        MinArchGetMemorySizeFn get_size,
                                        MinArchGetMemoryDataFn get_data) {
	// Check if core supports this memory type
	size_t mem_size = get_size(memory_type);
	if (!mem_size) {
		return MINARCH_MEM_NO_SUPPORT;
	}

	// Get memory pointer from core
	void* mem_data = get_data(memory_type);
	if (!mem_data) {
		return MINARCH_MEM_NULL_POINTER;
	}

	// Open file for writing
	FILE* file = fopen(filepath, "wb");
	if (!file) {
		return MINARCH_MEM_FILE_ERROR;
	}

	// Write data to file
	size_t bytes_written = fwrite(mem_data, 1, mem_size, file);
	fclose(file);

	if (bytes_written != mem_size) {
		return MINARCH_MEM_FILE_ERROR;
	}

	return MINARCH_MEM_OK;
}

MinArchMemoryResult MinArch_readSRAM(const char* filepath, MinArchGetMemorySizeFn get_size,
                                     MinArchGetMemoryDataFn get_data) {
	return MinArch_readMemory(filepath, MINARCH_MEMORY_SAVE_RAM, get_size, get_data);
}

MinArchMemoryResult MinArch_writeSRAM(const char* filepath, MinArchGetMemorySizeFn get_size,
                                      MinArchGetMemoryDataFn get_data) {
	return MinArch_writeMemory(filepath, MINARCH_MEMORY_SAVE_RAM, get_size, get_data);
}

MinArchMemoryResult MinArch_readRTC(const char* filepath, MinArchGetMemorySizeFn get_size,
                                    MinArchGetMemoryDataFn get_data) {
	return MinArch_readMemory(filepath, MINARCH_MEMORY_RTC, get_size, get_data);
}

MinArchMemoryResult MinArch_writeRTC(const char* filepath, MinArchGetMemorySizeFn get_size,
                                     MinArchGetMemoryDataFn get_data) {
	return MinArch_writeMemory(filepath, MINARCH_MEMORY_RTC, get_size, get_data);
}

const char* MinArch_memoryResultString(MinArchMemoryResult result) {
	switch (result) {
	case MINARCH_MEM_OK:
		return "Success";
	case MINARCH_MEM_NO_SUPPORT:
		return "Core does not support this memory type";
	case MINARCH_MEM_FILE_NOT_FOUND:
		return "File not found";
	case MINARCH_MEM_FILE_ERROR:
		return "File I/O error";
	case MINARCH_MEM_NULL_POINTER:
		return "Core returned null memory pointer";
	case MINARCH_MEM_SIZE_MISMATCH:
		return "File size does not match expected size";
	default:
		return "Unknown error";
	}
}
