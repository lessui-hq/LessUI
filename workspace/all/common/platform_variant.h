/**
 * platform_variant.h - Platform variant detection and device registry
 *
 * Provides a unified system for runtime hardware variant detection across all platforms.
 * This allows a single platform build to support multiple physical devices that share
 * the same hardware architecture but differ in screen size, resolution, or features.
 *
 * Key Concepts:
 * - Platform: Build target (e.g., "miyoomini", "rg35xxplus") - one per workspace directory
 * - Device: Physical hardware model (e.g., "Miyoo Mini Plus", "RG35XX H")
 * - Variant: Runtime-detected hardware configuration (resolution, features, etc.)
 *
 * Multiple devices can share a single variant configuration (same resolution/features).
 */

#ifndef PLATFORM_VARIANT_H
#define PLATFORM_VARIANT_H

#include <stdint.h>

///////////////////////////////
// Variant Type Enumeration
///////////////////////////////

typedef enum {
	VARIANT_NONE = 0, // Not applicable / unknown
	VARIANT_STANDARD = 1, // Base/standard variant
	VARIANT_ALTERNATE = 2, // Single alternate variant
	VARIANT_PLATFORM_BASE = 100 // Platform-specific variants start here
} VariantType;

///////////////////////////////
// Device Information
///////////////////////////////

/**
 * Device identifier - describes a specific physical device.
 * Multiple devices can map to the same variant if they share hardware characteristics.
 */
typedef struct {
	const char* device_id; // Short identifier (e.g., "rg35xxplus", "miyoomini")
	const char* display_name; // Human-readable name (e.g., "RG35XX Plus")
	const char* manufacturer; // Manufacturer name (e.g., "Anbernic", "Miyoo")
} DeviceInfo;

///////////////////////////////
// Hardware Feature Flags
///////////////////////////////

/**
 * Bitmask flags for hardware capabilities.
 * Platforms set these during variant detection to indicate available features.
 */
#define HW_FEATURE_NEON (1 << 0) // ARM NEON SIMD support
#define HW_FEATURE_LID (1 << 1) // Lid/hall sensor
#define HW_FEATURE_RUMBLE (1 << 2) // Vibration motor
#define HW_FEATURE_PMIC (1 << 3) // Power management IC (vs GPIO battery)
#define HW_FEATURE_ANALOG (1 << 4) // Analog sticks
#define HW_FEATURE_VOLUME_HW (1 << 5) // Hardware volume buttons (vs combo)

///////////////////////////////
// Platform Variant Structure
///////////////////////////////

/**
 * Global platform variant information.
 * Populated once at startup by PLAT_detectVariant().
 */
typedef struct {
	const char* platform; // Platform identifier (matches PLATFORM define)
	VariantType variant; // Detected variant type
	const char*
	    variant_name; // Short string for LESSUI_VARIANT export and logging (e.g., "vga", "4x3")
	const DeviceInfo* device; // Detected device info (from platform's device registry)

	// Display capabilities (runtime-determined)
	int screen_width; // Native screen width in pixels
	int screen_height; // Native screen height in pixels
	float screen_diagonal; // Physical screen size in inches
	int has_hdmi; // Platform supports HDMI output
	int hdmi_active; // HDMI currently connected (runtime state)

	// Hardware features
	uint32_t hw_features; // Feature bitmask (HW_FEATURE_*)

	// Platform-specific data
	void* platform_data; // Pointer to platform-specific variant data (optional)
} PlatformVariant;

///////////////////////////////
// Global Variant Instance
///////////////////////////////

/**
 * Global platform variant information.
 * Initialized once during platform initialization.
 */
extern PlatformVariant platform_variant;

///////////////////////////////
// Convenience Macros
///////////////////////////////

/**
 * Syntactic sugar for variant checking.
 * Usage: VARIANT_IS(VARIANT_TG5040_BRICK)
 */
#define VARIANT_IS(v) (platform_variant.variant == (v))

/**
 * Syntactic sugar for feature checking.
 * Usage: HAS_FEATURE(HW_FEATURE_PMIC)
 */
#define HAS_FEATURE(f) (platform_variant.hw_features & (f))

///////////////////////////////
// Platform API
///////////////////////////////

/**
 * Detect and populate platform variant information.
 * Called once during platform initialization.
 *
 * Each platform must implement this function to:
 * 1. Detect the specific device model (via env vars, files, etc.)
 * 2. Look up the device in the platform's device registry
 * 3. Apply the corresponding variant configuration
 * 4. Set screen resolution and hardware features
 *
 * @param variant Pointer to PlatformVariant structure to populate
 */
void PLAT_detectVariant(PlatformVariant* variant);

/**
 * Get human-readable device name.
 * Returns "<Manufacturer> <DeviceName>" (e.g., "Anbernic RG35XX Plus").
 *
 * @return Device name string (static storage, do not free)
 */
const char* PLAT_getDeviceName(void);

///////////////////////////////

#endif // PLATFORM_VARIANT_H
