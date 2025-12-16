/**
 * player_video_convert.c - Pixel format conversion for Player
 *
 * Provides NEON-optimized and scalar fallback implementations for converting
 * non-native pixel formats to RGB565.
 *
 * Extracted from player.c for maintainability.
 */

#include "player_video_convert.h"
#include "defines.h"
#include "log.h"
#include <stdlib.h>

#if defined(__arm__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

// Internal conversion buffer
static void* convert_buffer = NULL;

void PlayerVideoConvert_freeBuffer(void) {
	if (!convert_buffer)
		return;
	free(convert_buffer);
	convert_buffer = NULL;
}

void PlayerVideoConvert_allocBuffer(int w, int h) {
	PlayerVideoConvert_freeBuffer();
	size_t buffer_size = (size_t)(w * FIXED_BPP) * h;
	convert_buffer = malloc(buffer_size);
	if (!convert_buffer) {
		LOG_error("Failed to allocate conversion buffer: %dx%d (%zu bytes)", w, h, buffer_size);
		return;
	}
	LOG_debug("Allocated conversion buffer: %dx%d (%zu bytes)", w, h, buffer_size);
}

void* PlayerVideoConvert_getBuffer(void) {
	return convert_buffer;
}

int PlayerVideoConvert_needsConversion(PlayerPixelFormat format) {
	return format != PLAYER_PIXEL_FORMAT_RGB565;
}

void PlayerVideoConvert_xrgb8888Scalar(const void* data, uint16_t* output, unsigned width,
                                       unsigned height, size_t pitch) {
	const uint32_t* input = data;
	size_t extra = pitch / sizeof(uint32_t) - width;

	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0; x < width; x++) {
			uint32_t pixel = *input++;
			*output++ = ((pixel & 0xF80000) >> 8) | // Red: bits 23-19 -> 15-11
			            ((pixel & 0x00FC00) >> 5) | // Green: bits 15-10 -> 10-5
			            ((pixel & 0x0000F8) >> 3); // Blue: bits 7-3 -> 4-0
		}
		input += extra;
	}
}

void PlayerVideoConvert_0rgb1555Scalar(const void* data, uint16_t* output, unsigned width,
                                       unsigned height, size_t pitch) {
	const uint16_t* input = data;
	size_t extra = pitch / sizeof(uint16_t) - width;

	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0; x < width; x++) {
			uint16_t px = *input++;
			// Extract 5-bit components from 0RRRRRGGGGGBBBBB
			uint16_t r = (px >> 10) & 0x1F;
			uint16_t g = (px >> 5) & 0x1F;
			uint16_t b = px & 0x1F;
			// Expand green from 5 to 6 bits
			uint16_t g6 = (g << 1) | (g >> 4);
			// Pack to RGB565
			*output++ = (r << 11) | (g6 << 5) | b;
		}
		input += extra;
	}
}

#if defined(__arm__) || defined(__aarch64__)
void PlayerVideoConvert_xrgb8888Neon(const void* data, uint16_t* output, unsigned width,
                                     unsigned height, size_t pitch) {
	const uint32_t* input = data;
	size_t extra = pitch / sizeof(uint32_t) - width;

	// NEON mask constants for extracting RGB565 components from XRGB8888
	const uint32x4_t mask_blue = vdupq_n_u32(0x000000F8); // Blue: bits 7-3
	const uint32x4_t mask_green = vdupq_n_u32(0x0000FC00); // Green: bits 15-10
	const uint32x4_t mask_red = vdupq_n_u32(0x00F80000); // Red: bits 23-19

	for (unsigned y = 0; y < height; y++) {
		unsigned x = 0;
		const uint32_t* line_input = input;
		uint16_t* line_output = output;

		// NEON: process 4 pixels at a time
		unsigned width_vec = width & ~3u;
		for (; x < width_vec; x += 4) {
			uint32x4_t pixels = vld1q_u32(line_input);
			line_input += 4;

			uint32x4_t blue = vshrq_n_u32(vandq_u32(pixels, mask_blue), 3);
			uint32x4_t green = vshrq_n_u32(vandq_u32(pixels, mask_green), 5);
			uint32x4_t red = vshrq_n_u32(vandq_u32(pixels, mask_red), 8);

			uint32x4_t rgb565_32 = vorrq_u32(vorrq_u32(red, green), blue);
			uint16x4_t rgb565 = vmovn_u32(rgb565_32);

			vst1_u16(line_output, rgb565);
			line_output += 4;
		}

		// Scalar tail
		for (; x < width; x++) {
			uint32_t pixel = *line_input++;
			*line_output++ =
			    ((pixel & 0xF80000) >> 8) | ((pixel & 0x00FC00) >> 5) | ((pixel & 0x0000F8) >> 3);
		}

		input += width + extra;
		output += width;
	}
}

void PlayerVideoConvert_0rgb1555Neon(const void* data, uint16_t* output, unsigned width,
                                     unsigned height, size_t pitch) {
	const uint16_t* input = data;
	size_t extra = pitch / sizeof(uint16_t) - width;

	for (unsigned y = 0; y < height; y++) {
		unsigned x = 0;
		const uint16_t* line_input = input;
		uint16_t* line_output = output;

		// NEON: process 8 pixels at a time
		unsigned width_vec = width & ~7u;
		for (; x < width_vec; x += 8) {
			uint16x8_t src = vld1q_u16(line_input);
			line_input += 8;

			// Extract 5-bit components from 0RRRRRGGGGGBBBBB
			uint16x8_t r = vandq_u16(vshrq_n_u16(src, 10), vdupq_n_u16(0x1F));
			uint16x8_t g = vandq_u16(vshrq_n_u16(src, 5), vdupq_n_u16(0x1F));
			uint16x8_t b = vandq_u16(src, vdupq_n_u16(0x1F));

			// Expand green from 5 to 6 bits: g6 = (g << 1) | (g >> 4)
			uint16x8_t g6 = vorrq_u16(vshlq_n_u16(g, 1), vshrq_n_u16(g, 4));

			// Pack to RGB565: RRRRRGGGGGGBBBBB
			uint16x8_t rgb565 = vorrq_u16(vorrq_u16(vshlq_n_u16(r, 11), vshlq_n_u16(g6, 5)), b);

			vst1q_u16(line_output, rgb565);
			line_output += 8;
		}

		// Scalar tail
		for (; x < width; x++) {
			uint16_t px = *line_input++;
			uint16_t r = (px >> 10) & 0x1F;
			uint16_t g = (px >> 5) & 0x1F;
			uint16_t b = px & 0x1F;
			uint16_t g6 = (g << 1) | (g >> 4);
			*line_output++ = (r << 11) | (g6 << 5) | b;
		}

		input += width + extra;
		output += width;
	}
}
#endif // __arm__ || __aarch64__

void PlayerVideoConvert_convert(const void* data, unsigned width, unsigned height, size_t pitch,
                                PlayerPixelFormat format) {
	if (!convert_buffer) {
		LOG_error("Conversion buffer not allocated - skipping frame");
		return;
	}

	// Validate pitch based on pixel format
	size_t bytes_per_pixel = (format == PLAYER_PIXEL_FORMAT_XRGB8888) ? 4 : 2;
	size_t min_pitch = width * bytes_per_pixel;

	if (pitch < min_pitch) {
		LOG_error("Invalid pitch %zu for width %u (format %d requires >= %zu)", pitch, width,
		          format, min_pitch);
		return;
	}

	uint16_t* output = convert_buffer;

	switch (format) {
	case PLAYER_PIXEL_FORMAT_XRGB8888:
#if defined(__arm__) || defined(__aarch64__)
		PlayerVideoConvert_xrgb8888Neon(data, output, width, height, pitch);
#else
		PlayerVideoConvert_xrgb8888Scalar(data, output, width, height, pitch);
#endif
		break;

	case PLAYER_PIXEL_FORMAT_0RGB1555:
#if defined(__arm__) || defined(__aarch64__)
		PlayerVideoConvert_0rgb1555Neon(data, output, width, height, pitch);
#else
		PlayerVideoConvert_0rgb1555Scalar(data, output, width, height, pitch);
#endif
		break;

	case PLAYER_PIXEL_FORMAT_RGB565:
		LOG_warn("PlayerVideoConvert_convert called for RGB565 (no conversion needed)");
		break;

	default:
		LOG_error("Unknown pixel format %d", format);
		break;
	}
}
