/**
 * minarch_scaler.c - Video scaling calculation utilities
 *
 * Pure functions for calculating video scaling parameters.
 * Extracted from minarch.c selectScaler() for testability.
 */

#include "minarch_scaler.h"

#include <stdio.h>
#include <string.h>

// Ceiling division: (a + b - 1) / b
#define CEIL_DIV(a, b) (((a) + (b) - 1) / (b))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

void MinArchScaler_applyRotation(MinArchRotation rotation, int* w, int* h) {
	if (rotation == SCALER_ROTATION_90 || rotation == SCALER_ROTATION_270) {
		int temp = *w;
		*w = *h;
		*h = temp;
	}
}

void MinArchScaler_calculateAspectDimensions(int src_w, int src_h, double aspect_ratio, int* out_w,
                                             int* out_h) {
	*out_w = src_w;
	*out_h = CEIL_DIV(src_w, aspect_ratio);

	// If calculated height is less than source, recalculate from height
	if (*out_h < src_h) {
		*out_h = src_h;
		*out_w = (int)(src_h * aspect_ratio);
		*out_w += *out_w % 2; // Ensure even width
	}
}

bool MinArchScaler_clampToBuffer(MinArchScalerResult* result, int buffer_w, int buffer_h, int bpp) {
	if (result->dst_w <= buffer_w && result->dst_h <= buffer_h) {
		return false;
	}

	// Calculate scale factor to fit within buffer
	float cap_w = (float)buffer_w / result->dst_w;
	float cap_h = (float)buffer_h / result->dst_h;
	float cap = (cap_w < cap_h) ? cap_w : cap_h;

	result->dst_w = (int)(result->dst_w * cap);
	result->dst_h = (int)(result->dst_h * cap);
	result->dst_p = result->dst_w * bpp;

	// Adjust offsets proportionally
	result->dst_x = (int)(result->dst_x * cap);
	result->dst_y = (int)(result->dst_y * cap);

	return true;
}

/**
 * Calculate scaling for native/cropped modes (integer scaling).
 */
static void calculate_native_cropped(const MinArchScalerInput* input, MinArchScalerResult* result,
                                     int src_w, int src_h, MinArchScalerMode mode) {
	int scale = MIN(input->device_w / src_w, input->device_h / src_h);

	if (scale == 0) {
		// Source is larger than device - forced crop
		(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "forced crop");
		result->dst_w = input->device_w;
		result->dst_h = input->device_h;
		result->dst_p = input->device_p;

		int ox = (input->device_w - src_w) / 2;
		int oy = (input->device_h - src_h) / 2;

		if (ox < 0)
			result->src_x = -ox;
		else
			result->dst_x = ox;

		if (oy < 0)
			result->src_y = -oy;
		else
			result->dst_y = oy;

		result->scale = 1;
	} else if (mode == SCALER_MODE_CROPPED) {
		// Cropped mode - scale up and crop edges
		int scale_x = CEIL_DIV(input->device_w, src_w);
		int scale_y = CEIL_DIV(input->device_h, src_h);
		scale = MIN(scale_x, scale_y);

		(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "cropped");
		result->dst_w = input->device_w;
		result->dst_h = input->device_h;
		result->dst_p = input->device_p;

		int scaled_w = src_w * scale;
		int scaled_h = src_h * scale;

		int ox = (input->device_w - scaled_w) / 2;
		int oy = (input->device_h - scaled_h) / 2;

		if (ox < 0) {
			result->src_x = -ox / scale;
			result->src_w = src_w - result->src_x * 2;
		} else {
			result->dst_x = ox;
		}

		if (oy < 0) {
			result->src_y = -oy / scale;
			result->src_h = src_h - result->src_y * 2;
		} else {
			result->dst_y = oy;
		}

		result->scale = scale;
	} else {
		// Native integer scaling
		(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "integer");
		int scaled_w = src_w * scale;
		int scaled_h = src_h * scale;
		result->dst_w = input->device_w;
		result->dst_h = input->device_h;
		result->dst_p = input->device_p;
		result->dst_x = (input->device_w - scaled_w) / 2;
		result->dst_y = (input->device_h - scaled_h) / 2;
		result->scale = scale;
	}
}

/**
 * Calculate scaling for fit mode devices (software scaling).
 */
static void calculate_fit_mode(const MinArchScalerInput* input, MinArchScalerResult* result,
                               int src_w, int src_h, int aspect_w, int aspect_h) {
	if (input->mode == SCALER_MODE_FULLSCREEN) {
		(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "full fit");
		result->dst_w = input->device_w;
		result->dst_h = input->device_h;
		result->dst_p = input->device_p;
		result->scale = -1; // Nearest neighbor
	} else {
		// Aspect-preserving scaling
		double scale_f =
		    MIN(((double)input->device_w) / aspect_w, ((double)input->device_h) / aspect_h);

		(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "aspect fit");
		result->dst_w = (int)(aspect_w * scale_f);
		result->dst_h = (int)(aspect_h * scale_f);
		result->dst_p = input->device_p;
		result->dst_x = (input->device_w - result->dst_w) / 2;
		result->dst_y = (input->device_h - result->dst_h) / 2;

		// Use integer scale if perfect 1:1 match
		if (scale_f == 1.0 && result->dst_w == src_w && result->dst_h == src_h) {
			result->scale = 1;
		} else {
			result->scale = -1;
		}
	}
}

/**
 * Calculate scaling for oversized devices (hardware scaling with overscan).
 */
static void calculate_oversized(const MinArchScalerInput* input, MinArchScalerResult* result,
                                int src_w, int src_h, double aspect_ratio) {
	int scale_x = CEIL_DIV(input->device_w, src_w);
	int scale_y = CEIL_DIV(input->device_h, src_h);

	// Odd resolutions need snapping to eights
	int r = (input->device_h - src_h) % 8;
	if (r)
		scale_y -= 1;

	int scale = MAX(scale_x, scale_y);

	int scaled_w = src_w * scale;
	int scaled_h = src_h * scale;

	if (input->mode == SCALER_MODE_FULLSCREEN) {
		(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "full%d", scale);
		result->dst_w = scaled_w;
		result->dst_h = scaled_h;
		result->dst_p = result->dst_w * input->bpp;
		result->scale = scale;
	} else {
		// Aspect ratio handling for oversized devices
		double fixed_aspect_ratio = ((double)input->device_w) / input->device_h;
		int core_aspect = (int)(aspect_ratio * 1000);
		int fixed_aspect = (int)(fixed_aspect_ratio * 1000);

		if (core_aspect > fixed_aspect) {
			// Letterbox (black bars top/bottom)
			(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "aspect%dL", scale);
			int letterbox_h = (int)(input->device_w / aspect_ratio);
			double aspect_hr = ((double)letterbox_h) / input->device_h;
			result->dst_w = scaled_w;
			result->dst_h = (int)(scaled_h / aspect_hr);
			result->dst_y = (result->dst_h - scaled_h) / 2;
		} else if (core_aspect < fixed_aspect) {
			// Pillarbox (black bars left/right)
			(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "aspect%dP", scale);
			int pillar_w = (int)(input->device_h * aspect_ratio);
			double aspect_wr = ((double)pillar_w) / input->device_w;
			result->dst_w = (int)(scaled_w / aspect_wr);
			result->dst_h = scaled_h;
			result->dst_w = (result->dst_w / 8) * 8; // Snap to 8-pixel boundary
			result->dst_x = (result->dst_w - scaled_w) / 2;
		} else {
			// Perfect aspect match
			(void)snprintf(result->scaler_name, sizeof(result->scaler_name), "aspect%dM", scale);
			result->dst_w = scaled_w;
			result->dst_h = scaled_h;
		}

		result->dst_p = result->dst_w * input->bpp;
		result->scale = scale;
	}
}

void MinArchScaler_calculate(const MinArchScalerInput* input, MinArchScalerResult* result) {
	// Initialize result to defaults
	memset(result, 0, sizeof(*result));
	result->src_p = input->src_p;

	// Apply rotation to source dimensions
	int src_w = input->src_w;
	int src_h = input->src_h;
	MinArchScaler_applyRotation(input->rotation, &src_w, &src_h);

	// Store true (rotated) dimensions
	result->true_w = src_w;
	result->true_h = src_h;

	// Set default source dimensions (may be modified by cropping)
	result->src_w = src_w;
	result->src_h = src_h;

	// Calculate aspect-corrected dimensions
	double aspect_ratio = input->aspect_ratio;
	if (aspect_ratio <= 0) {
		aspect_ratio = (double)src_w / src_h;
	}

	int aspect_w, aspect_h;
	MinArchScaler_calculateAspectDimensions(src_w, src_h, aspect_ratio, &aspect_w, &aspect_h);

	// Determine effective scaling mode
	MinArchScalerMode mode = input->mode;

	// HDMI detection: force native mode for HDMI on cropped
	if (mode == SCALER_MODE_CROPPED && input->device_w == input->hdmi_width) {
		mode = SCALER_MODE_NATIVE;
	}

	// Calculate based on mode and device type
	if (mode == SCALER_MODE_NATIVE || mode == SCALER_MODE_CROPPED) {
		calculate_native_cropped(input, result, src_w, src_h, mode);
	} else if (input->fit) {
		calculate_fit_mode(input, result, src_w, src_h, aspect_w, aspect_h);
	} else {
		calculate_oversized(input, result, src_w, src_h, aspect_ratio);
	}

	// Clamp to buffer bounds
	if (input->buffer_w > 0 && input->buffer_h > 0) {
		MinArchScaler_clampToBuffer(result, input->buffer_w, input->buffer_h, input->bpp);
	}

	// Set aspect value for renderer
	if (mode == SCALER_MODE_NATIVE || mode == SCALER_MODE_CROPPED) {
		result->aspect = 0; // Integer scale
	} else if (mode == SCALER_MODE_FULLSCREEN) {
		result->aspect = -1; // Fullscreen
	} else {
		result->aspect = aspect_ratio; // Aspect ratio for SDL2 accelerated scaling
	}
}
