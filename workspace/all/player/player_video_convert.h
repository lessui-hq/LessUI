/**
 * player_video_convert.h - Pixel format conversion for Player
 *
 * Provides functions to convert non-native pixel formats (0RGB1555, XRGB8888)
 * to the device's native RGB565 format. Includes NEON-optimized implementations
 * for ARM devices.
 *
 * Extracted from player.c for maintainability.
 */

#ifndef PLAYER_VIDEO_CONVERT_H
#define PLAYER_VIDEO_CONVERT_H

#include <stddef.h>
#include <stdint.h>

/**
 * Pixel format enumeration (matches libretro.h values).
 */
typedef enum {
	PLAYER_PIXEL_FORMAT_0RGB1555 = 0, // 15-bit color, 1 unused bit (legacy)
	PLAYER_PIXEL_FORMAT_XRGB8888 = 1, // 32-bit with unused alpha
	PLAYER_PIXEL_FORMAT_RGB565 = 2, // 16-bit native format
} PlayerPixelFormat;

/**
 * Allocates the pixel format conversion buffer.
 *
 * Must be called before pixel_convert() if the core uses a non-RGB565 format.
 * Buffer is sized for RGB565 output (2 bytes per pixel).
 *
 * @param width Buffer width in pixels
 * @param height Buffer height in pixels
 */
void PlayerVideoConvert_allocBuffer(int width, int height);

/**
 * Frees the pixel format conversion buffer.
 *
 * Safe to call even if buffer was never allocated.
 */
void PlayerVideoConvert_freeBuffer(void);

/**
 * Gets the current conversion buffer pointer.
 *
 * @return Pointer to conversion buffer, or NULL if not allocated
 */
void* PlayerVideoConvert_getBuffer(void);

/**
 * Checks if the current pixel format requires conversion.
 *
 * @param format Current pixel format
 * @return 1 if conversion is needed, 0 if format is native RGB565
 */
int PlayerVideoConvert_needsConversion(PlayerPixelFormat format);

/**
 * Converts pixel data to RGB565 format.
 *
 * Dispatches to the appropriate conversion function (NEON-optimized or scalar)
 * based on the source format. RGB565 input is a no-op.
 *
 * @param data Source pixel data
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @param pitch Source pitch in bytes
 * @param format Source pixel format
 *
 * @note Writes converted data to internal buffer (get via PlayerVideoConvert_getBuffer)
 * @note PlayerVideoConvert_allocBuffer() must be called first
 */
void PlayerVideoConvert_convert(const void* data, unsigned width, unsigned height, size_t pitch,
                                PlayerPixelFormat format);

/**
 * Converts XRGB8888 to RGB565 (scalar implementation).
 *
 * @param data Source XRGB8888 data
 * @param output Destination RGB565 buffer
 * @param width Frame width
 * @param height Frame height
 * @param pitch Source pitch in bytes
 */
void PlayerVideoConvert_xrgb8888Scalar(const void* data, uint16_t* output, unsigned width,
                                       unsigned height, size_t pitch);

/**
 * Converts 0RGB1555 to RGB565 (scalar implementation).
 *
 * @param data Source 0RGB1555 data
 * @param output Destination RGB565 buffer
 * @param width Frame width
 * @param height Frame height
 * @param pitch Source pitch in bytes
 */
void PlayerVideoConvert_0rgb1555Scalar(const void* data, uint16_t* output, unsigned width,
                                       unsigned height, size_t pitch);

#if defined(__arm__) || defined(__aarch64__)
/**
 * Converts XRGB8888 to RGB565 using ARM NEON SIMD.
 *
 * Processes 4 pixels per iteration for ~3-4x speedup on ARM devices.
 *
 * @param data Source XRGB8888 data
 * @param output Destination RGB565 buffer
 * @param width Frame width
 * @param height Frame height
 * @param pitch Source pitch in bytes
 */
void PlayerVideoConvert_xrgb8888Neon(const void* data, uint16_t* output, unsigned width,
                                     unsigned height, size_t pitch);

/**
 * Converts 0RGB1555 to RGB565 using ARM NEON SIMD.
 *
 * Processes 8 pixels per iteration for ~3-4x speedup on ARM devices.
 *
 * @param data Source 0RGB1555 data
 * @param output Destination RGB565 buffer
 * @param width Frame width
 * @param height Frame height
 * @param pitch Source pitch in bytes
 */
void PlayerVideoConvert_0rgb1555Neon(const void* data, uint16_t* output, unsigned width,
                                     unsigned height, size_t pitch);
#endif

#endif /* PLAYER_VIDEO_CONVERT_H */
