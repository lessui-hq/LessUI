/**
 * minarch_rotation.c - Software rotation buffer management for MinArch
 *
 * Provides buffer management and rotation functions for software-based
 * framebuffer rotation.
 *
 * Extracted from minarch.c for maintainability.
 */

#include "minarch_rotation.h"
#include "log.h"
#include "scaler.h"
#include <stdlib.h>

// Internal rotation buffer state
static MinArchRotationBuffer rotation_buffer = {NULL, 0, 0, 0, 0};

MinArchRotationBuffer* MinArchRotation_getBuffer(void) {
	return &rotation_buffer;
}

void MinArchRotation_allocBuffer(uint32_t width, uint32_t height, uint32_t pitch) {
	size_t required_size = (size_t)pitch * height;

	LOG_debug("MinArchRotation_allocBuffer: %ux%u pitch=%u (need %zu bytes, have %zu)", width,
	          height, pitch, required_size, rotation_buffer.size);

	// Reallocate only if needed (buffer grows but never shrinks)
	if (required_size > rotation_buffer.size) {
		void* new_buffer = realloc(rotation_buffer.buffer, required_size);
		if (!new_buffer) {
			LOG_error("Failed to allocate rotation buffer: %zu bytes", required_size);
			return;
		}
		rotation_buffer.buffer = new_buffer;
		rotation_buffer.size = required_size;
		LOG_debug("Reallocated rotation buffer to %zu bytes", required_size);
	}

	// Always update dimensions (buffer may be reused at different sizes)
	rotation_buffer.width = width;
	rotation_buffer.height = height;
	rotation_buffer.pitch = pitch;
}

void MinArchRotation_freeBuffer(void) {
	if (rotation_buffer.buffer) {
		free(rotation_buffer.buffer);
		rotation_buffer.buffer = NULL;
		rotation_buffer.size = 0;
		rotation_buffer.width = 0;
		rotation_buffer.height = 0;
		rotation_buffer.pitch = 0;
	}
}

void* MinArchRotation_apply(unsigned rotation, void* src, uint32_t src_w, uint32_t src_h,
                            uint32_t src_p) {
	// Fast path: no rotation
	if (rotation == ROTATION_0)
		return src;

	// Calculate rotated dimensions
	uint32_t dst_w, dst_h, dst_p;
	if (rotation == ROTATION_90 || rotation == ROTATION_270) {
		dst_w = src_h; // Swap dimensions
		dst_h = src_w;
	} else {
		dst_w = src_w;
		dst_h = src_h;
	}
	dst_p = dst_w * sizeof(uint16_t);

	LOG_debug("MinArchRotation_apply: rot=%u, src=%ux%u (pitch=%u) -> dst=%ux%u (pitch=%u)",
	          rotation, src_w, src_h, src_p, dst_w, dst_h, dst_p);

	// Allocate rotation buffer if needed
	MinArchRotation_allocBuffer(dst_w, dst_h, dst_p);

	if (!rotation_buffer.buffer) {
		LOG_error("Rotation buffer allocation failed, skipping rotation");
		return src;
	}

	// Perform rotation (use NEON-optimized version when available)
#ifdef HAS_NEON
	rotate_n16(rotation, src, rotation_buffer.buffer, src_w, src_h, src_p, dst_p);
#else
	rotate_c16(rotation, src, rotation_buffer.buffer, src_w, src_h, src_p, dst_p);
#endif

	return rotation_buffer.buffer;
}
