/**
 * render_common.h - Shared rendering utilities across all platforms
 *
 * This module provides common rendering calculations used by all backends:
 * - Destination rectangle calculation (aspect ratio, centering, scaling)
 * - Hard scale factor calculation for crisp upscaling
 * - Color conversion utilities
 *
 * These functions are backend-agnostic and work with any rendering system.
 */

#ifndef __RENDER_COMMON_H__
#define __RENDER_COMMON_H__

#include <stdint.h>

#include "api.h"

/**
 * Destination rectangle for rendering.
 *
 * Represents the target area on screen where content should be rendered,
 * calculated based on aspect ratio settings and screen dimensions.
 */
typedef struct RenderDestRect {
	int x; // X offset from left edge of screen
	int y; // Y offset from top edge of screen
	int w; // Width of destination area
	int h; // Height of destination area
} RenderDestRect;

/**
 * Calculates destination rectangle for rendering.
 *
 * Given source dimensions, screen dimensions, and aspect ratio settings,
 * computes the optimal destination rectangle that:
 * - Preserves aspect ratio (if aspect > 0)
 * - Centers content on screen
 * - Handles native scaling (aspect == 0)
 * - Handles fullscreen stretch (aspect < 0)
 *
 * Aspect ratio modes:
 * - aspect == 0:  Native/integer scaling with centering
 * - aspect < 0:   Fullscreen stretch (fills screen)
 * - aspect > 0:   Preserve aspect ratio (letterbox/pillarbox as needed)
 *
 * @param renderer     Renderer with source dimensions and aspect setting
 * @param device_w     Device screen width in pixels
 * @param device_h     Device screen height in pixels
 * @return Calculated destination rectangle
 */
RenderDestRect RENDER_calcDestRect(const GFX_Renderer* renderer, int device_w, int device_h);

/**
 * Calculates hard scale factor for crisp upscaling.
 *
 * The "crisp" sharpness mode uses a two-pass scaling approach:
 * 1. First upscale with nearest-neighbor to hard_scale multiplier
 * 2. Then downscale with bilinear to final size
 *
 * This produces sharper pixels than pure bilinear while avoiding
 * the aliasing artifacts of pure nearest-neighbor at non-integer scales.
 *
 * The hard_scale factor is chosen based on source resolution:
 * - Native or larger than device: 1 (no intermediate upscale)
 * - Smaller sources: 4 (crisp nearest-neighbor for retro content)
 *
 * @param src_w    Source width in pixels
 * @param src_h    Source height in pixels
 * @param device_w Device screen width
 * @param device_h Device screen height
 * @return Hard scale factor (1 or 4)
 */
int RENDER_calcHardScale(int src_w, int src_h, int device_w, int device_h);

/**
 * Converts RGB565 color to RGB888 components.
 *
 * Extracts 8-bit R, G, B values from a 16-bit RGB565 packed color.
 * The 5/6/5 bit components are scaled to full 8-bit range.
 *
 * @param rgb565 Packed RGB565 color value
 * @param r      Output: Red component (0-255)
 * @param g      Output: Green component (0-255)
 * @param b      Output: Blue component (0-255)
 */
void RENDER_rgb565ToRgb888(uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * Converts RGB888 components to RGB565 color.
 *
 * Packs 8-bit R, G, B values into a 16-bit RGB565 color.
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return Packed RGB565 color value
 */
uint16_t RENDER_rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b);

#endif /* __RENDER_COMMON_H__ */
