/**
 * platform_variant.c - Platform variant detection implementation
 */

#include "platform_variant.h"
#include "platform.h"
#include <stdio.h>

// Weak symbol - platform can override with its own implementation
#ifndef FALLBACK_IMPLEMENTATION
#define FALLBACK_IMPLEMENTATION __attribute__((weak))
#endif

// Global platform variant instance
PlatformVariant platform_variant = {.platform = NULL,
                                    .variant = VARIANT_NONE,
                                    .device = NULL,
                                    .screen_width = 0,
                                    .screen_height = 0,
                                    .screen_diagonal = 0.0f,
                                    .has_hdmi = 0,
                                    .hdmi_active = 0,
                                    .hw_features = 0,
                                    .platform_data = NULL};

/**
 * Default implementation for single-device platforms.
 *
 * Platforms with multiple device variants should override this with their own
 * implementation that includes device detection and registry lookup.
 *
 * This fallback uses compile-time constants from platform.h.
 */
FALLBACK_IMPLEMENTATION
void PLAT_detectVariant(PlatformVariant* v) {
	v->platform = PLATFORM;
	v->variant = VARIANT_STANDARD;
	v->device = NULL; // Single-device platforms don't need registry
	v->screen_width = FIXED_WIDTH;
	v->screen_height = FIXED_HEIGHT;
	v->screen_diagonal = SCREEN_DIAGONAL;
#ifdef HAS_HDMI
	v->has_hdmi = HAS_HDMI;
#else
	v->has_hdmi = 0;
#endif
	v->hdmi_active = 0;
	v->hw_features = 0;
#if defined(__arm__) || defined(__aarch64__)
	v->hw_features |= HW_FEATURE_NEON;
#endif
	v->platform_data = NULL;
}

const char* PLAT_getDeviceName(void) {
	static char device_name[256];

	if (platform_variant.device && platform_variant.device->manufacturer) {
		(void)snprintf(device_name, sizeof(device_name), "%s %s",
		               platform_variant.device->manufacturer,
		               platform_variant.device->display_name);
		return device_name;
	} else if (platform_variant.device) {
		return platform_variant.device->display_name;
	}

	return "Unknown Device";
}
