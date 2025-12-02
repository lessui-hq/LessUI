/**
 * render_common.c - Shared rendering utilities implementation
 *
 * These calculations were previously duplicated across multiple platform files.
 * Now unified in a single location for consistency and maintainability.
 */

#include "render_common.h"

RenderDestRect RENDER_calcDestRect(const GFX_Renderer* renderer, int device_w, int device_h) {
	RenderDestRect dst = {0, 0, device_w, device_h};

	if (renderer->aspect == 0) {
		// Native or cropped scaling - integer scale with centering
		int dst_w = renderer->src_w * renderer->scale;
		int dst_h = renderer->src_h * renderer->scale;
		int dst_x = (device_w - dst_w) / 2;
		int dst_y = (device_h - dst_h) / 2;

		dst.x = dst_x;
		dst.y = dst_y;
		dst.w = dst_w;
		dst.h = dst_h;
	} else if (renderer->aspect < 0) {
		// Fullscreen stretch - fill entire screen
		dst.x = 0;
		dst.y = 0;
		dst.w = device_w;
		dst.h = device_h;
	} else {
		// Aspect ratio preserving - letterbox or pillarbox as needed
		double aspect = renderer->aspect;

		// Start by fitting height
		int aspect_h = device_h;
		int aspect_w = (int)(aspect_h * aspect);

		// If too wide, fit width instead
		if (aspect_w > device_w) {
			double ratio = 1.0 / aspect;
			aspect_w = device_w;
			aspect_h = (int)(aspect_w * ratio);
		}

		// Center on screen
		int aspect_x = (device_w - aspect_w) / 2;
		int aspect_y = (device_h - aspect_h) / 2;

		dst.x = aspect_x;
		dst.y = aspect_y;
		dst.w = aspect_w;
		dst.h = aspect_h;
	}

	return dst;
}

int RENDER_calcHardScale(int src_w, int src_h, int device_w, int device_h) {
	// Native or larger than device - no intermediate upscale needed
	if (src_w >= device_w && src_h >= device_h) {
		return 1;
	}

	// All smaller sources use 4x for crisp nearest-neighbor scaling
	return 4;
}

void RENDER_rgb565ToRgb888(uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b) {
	// Extract 5-bit red (bits 15-11)
	uint8_t red = (rgb565 >> 11) & 0x1F;
	// Extract 6-bit green (bits 10-5)
	uint8_t green = (rgb565 >> 5) & 0x3F;
	// Extract 5-bit blue (bits 4-0)
	uint8_t blue = rgb565 & 0x1F;

	// Scale to 8-bit range with proper rounding
	// For 5-bit: multiply by 255/31 ≈ 8.226, approximated as (val << 3) | (val >> 2)
	// For 6-bit: multiply by 255/63 ≈ 4.048, approximated as (val << 2) | (val >> 4)
	*r = (red << 3) | (red >> 2);
	*g = (green << 2) | (green >> 4);
	*b = (blue << 3) | (blue >> 2);
}

uint16_t RENDER_rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b) {
	// Pack into RGB565: RRRRR GGGGGG BBBBB
	return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
