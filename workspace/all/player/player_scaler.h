/**
 * player_scaler.h - Video scaling calculation utilities
 *
 * Provides pure functions for calculating video scaling parameters.
 * These handle the math for determining how to scale emulator video
 * output to fit various screen sizes and aspect ratios.
 *
 * Extracted from player.c for testability.
 */

#ifndef __PLAYER_SCALER_H__
#define __PLAYER_SCALER_H__

#include <stdbool.h>

/**
 * Scaling modes for video output.
 */
typedef enum {
	SCALER_MODE_NATIVE, /**< Integer scale, 1:1 pixels (may have borders) */
	SCALER_MODE_ASPECT, /**< Scale maintaining aspect ratio (letterboxed) */
	SCALER_MODE_FULLSCREEN, /**< Scale to fill screen (may distort) */
	SCALER_MODE_CROPPED /**< Overscan crop to fill screen */
} PlayerScalerMode;

/**
 * Rotation values for video output.
 */
typedef enum {
	SCALER_ROTATION_NONE = 0,
	SCALER_ROTATION_90 = 1,
	SCALER_ROTATION_180 = 2,
	SCALER_ROTATION_270 = 3
} PlayerRotation;

/**
 * Input parameters for scaler calculation.
 */
typedef struct {
	int src_w; /**< Source width from core */
	int src_h; /**< Source height from core */
	int src_p; /**< Source pitch (bytes per scanline) */
	double aspect_ratio; /**< Core's aspect ratio (0 = use src dimensions) */
	PlayerRotation rotation; /**< Video rotation */
	PlayerScalerMode mode; /**< Scaling mode */
	int device_w; /**< Device screen width */
	int device_h; /**< Device screen height */
	int device_p; /**< Device screen pitch */
	int bpp; /**< Bytes per pixel (typically 2 for RGB565) */
	bool fit; /**< True if device uses software scaling (fit mode) */
	int buffer_w; /**< Maximum output buffer width */
	int buffer_h; /**< Maximum output buffer height */
	int hdmi_width; /**< HDMI width for HDMI detection */
} PlayerScalerInput;

/**
 * Output result from scaler calculation.
 */
typedef struct {
	// Source rectangle (what portion of source to use)
	int src_x; /**< Source X offset (for cropping) */
	int src_y; /**< Source Y offset (for cropping) */
	int src_w; /**< Source width to use */
	int src_h; /**< Source height to use */
	int src_p; /**< Source pitch */

	// Destination rectangle (where and how big on screen)
	int dst_x; /**< Destination X offset (for centering) */
	int dst_y; /**< Destination Y offset (for centering) */
	int dst_w; /**< Destination width */
	int dst_h; /**< Destination height */
	int dst_p; /**< Destination pitch */

	// Scaling parameters
	int scale; /**< Integer scale factor (-1 for nearest neighbor) */
	int visual_scale; /**< Visual scale for effects (accounts for GPU downscaling) */
	double aspect; /**< 0=integer, -1=fullscreen, >0=aspect ratio */

	// Original dimensions (before cropping)
	int true_w; /**< True source width (after rotation) */
	int true_h; /**< True source height (after rotation) */

	// Debug info
	char scaler_name[24]; /**< Description of scaler mode used */
} PlayerScalerResult;

/**
 * Calculates video scaling parameters.
 *
 * This is a pure function that computes how to scale video output from
 * an emulator core to fit a target screen. It handles:
 * - Integer scaling (1x, 2x, 3x, etc.)
 * - Aspect ratio preservation with letterboxing/pillarboxing
 * - Fullscreen stretching
 * - Overscan cropping
 * - 90°/270° rotation (dimension swapping)
 * - Buffer size bounds checking
 *
 * @param input Input parameters describing source and target
 * @param result Output structure to fill with calculated values
 *
 * @example
 *   PlayerScalerInput input = {
 *       .src_w = 256, .src_h = 224, .src_p = 512,
 *       .aspect_ratio = 4.0/3.0,
 *       .rotation = SCALER_ROTATION_NONE,
 *       .mode = SCALER_MODE_ASPECT,
 *       .device_w = 640, .device_h = 480, .device_p = 1280,
 *       .bpp = 2, .fit = true,
 *       .buffer_w = 960, .buffer_h = 720
 *   };
 *   PlayerScalerResult result;
 *   PlayerScaler_calculate(&input, &result);
 *   // result.dst_w = 640, result.dst_h = 480, result.scale = 2, etc.
 */
void PlayerScaler_calculate(const PlayerScalerInput* input, PlayerScalerResult* result);

/**
 * Applies rotation to source dimensions.
 *
 * For 90° and 270° rotations, swaps width and height.
 * For 0° and 180°, dimensions are unchanged.
 *
 * @param rotation Rotation value
 * @param w Width (in/out)
 * @param h Height (in/out)
 */
void PlayerScaler_applyRotation(PlayerRotation rotation, int* w, int* h);

/**
 * Calculates aspect-corrected dimensions.
 *
 * Given source dimensions and target aspect ratio, calculates the
 * dimensions needed to display at that aspect ratio.
 *
 * @param src_w Source width
 * @param src_h Source height
 * @param aspect_ratio Target aspect ratio (width/height)
 * @param out_w Output width
 * @param out_h Output height
 */
void PlayerScaler_calculateAspectDimensions(int src_w, int src_h, double aspect_ratio, int* out_w,
                                            int* out_h);

/**
 * Clamps scaler output to buffer bounds.
 *
 * If the calculated destination dimensions exceed the buffer size,
 * scales them down proportionally and adjusts offsets.
 *
 * @param result Scaler result to potentially modify
 * @param buffer_w Maximum buffer width
 * @param buffer_h Maximum buffer height
 * @param bpp Bytes per pixel
 * @return true if clamping was applied, false otherwise
 */
bool PlayerScaler_clampToBuffer(PlayerScalerResult* result, int buffer_w, int buffer_h, int bpp);

#endif // __PLAYER_SCALER_H__
