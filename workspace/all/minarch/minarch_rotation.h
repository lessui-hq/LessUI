/**
 * minarch_rotation.h - Software rotation buffer management for MinArch
 *
 * Provides buffer management and rotation functions for software-based
 * framebuffer rotation when the core requests non-zero rotation.
 *
 * Extracted from minarch.c for maintainability.
 */

#ifndef MINARCH_ROTATION_H
#define MINARCH_ROTATION_H

#include <stddef.h>
#include <stdint.h>

/**
 * Rotation buffer state.
 *
 * Holds the allocated buffer and its current configuration.
 * The buffer is reused across frames and only reallocated when
 * dimensions increase.
 */
typedef struct {
	void* buffer; // Rotation output buffer (RGB565)
	size_t size; // Current buffer size in bytes
	uint32_t width; // Buffer width in pixels
	uint32_t height; // Buffer height in pixels
	uint32_t pitch; // Buffer pitch in bytes
} MinArchRotationBuffer;

/**
 * Gets the current rotation buffer.
 *
 * @return Pointer to rotation buffer state
 */
MinArchRotationBuffer* MinArchRotation_getBuffer(void);

/**
 * Allocates rotation buffer for specified dimensions.
 *
 * Only reallocates if the required size exceeds current allocation.
 * Updates buffer dimensions regardless.
 *
 * @param width Destination width in pixels
 * @param height Destination height in pixels
 * @param pitch Destination pitch in bytes
 */
void MinArchRotation_allocBuffer(uint32_t width, uint32_t height, uint32_t pitch);

/**
 * Frees the rotation buffer.
 *
 * Safe to call even if buffer was never allocated.
 */
void MinArchRotation_freeBuffer(void);

/**
 * Applies software rotation to a framebuffer.
 *
 * For 90/270 degree rotations, output dimensions are swapped (width becomes height).
 * Uses NEON-optimized code on ARM platforms, scalar fallback otherwise.
 *
 * @param rotation Rotation angle (0=none, 1=90CCW, 2=180, 3=270CCW)
 * @param src Source pixel buffer (RGB565 format)
 * @param src_w Source width in pixels
 * @param src_h Source height in pixels
 * @param src_p Source pitch in bytes
 *
 * @return Pointer to rotated buffer if rotation applied, or src if rotation=0
 *
 * @note Allocates/reuses internal buffer as needed
 * @note For rotation=0, returns src unchanged (fast path)
 */
void* MinArchRotation_apply(unsigned rotation, void* src, uint32_t src_w, uint32_t src_h,
                            uint32_t src_p);

#endif /* MINARCH_ROTATION_H */
