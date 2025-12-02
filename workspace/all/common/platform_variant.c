/**
 * platform_variant.c - Platform variant detection implementation
 */

#include "platform_variant.h"
#include <stdio.h>

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

const char* PLAT_getDeviceName(void) {
	static char device_name[256];

	if (platform_variant.device && platform_variant.device->manufacturer) {
		snprintf(device_name, sizeof(device_name), "%s %s", platform_variant.device->manufacturer,
		         platform_variant.device->display_name);
		return device_name;
	} else if (platform_variant.device) {
		return platform_variant.device->display_name;
	}

	return "Unknown Device";
}
