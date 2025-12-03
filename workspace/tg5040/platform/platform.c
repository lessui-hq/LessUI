/**
 * platform.c - Trimui Smart Pro (TG5040) platform implementation
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Supports Trimui Smart Pro and Brick variant. Hardware differences
 * detected via DEVICE environment variable.
 *
 * Hardware features:
 * - SDL2-based video with sharpness control (via render_sdl2)
 * - Joystick input via SDL2
 * - Display effects (scanlines, grid with DMG color support)
 * - AXP2202 power management
 * - LED control (multi-LED on Brick variant)
 * - CPU frequency scaling
 * - Rumble motor support
 *
 * Brick variant differences:
 * - Multiple LED zones (max_scale, max_scale_lr, max_scale_f1f2)
 * - Different backlight behavior (minimum brightness of 8)
 */

#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <msettings.h>

#include "api.h"
#include "defines.h"
#include "platform.h"
#include "utils.h"

#include "render_sdl2.h"
#include "scaler.h"

///////////////////////////////
// Device Registry and Variant Configuration
///////////////////////////////

// Device registry - all known devices that work with this platform
static const DeviceInfo tg5040_devices[] = {
    // Standard TG5040
    {.device_id = "tg5040", .display_name = "Smart Pro", .manufacturer = "Trimui"},

    // Brick variant
    {.device_id = "brick", .display_name = "Brick", .manufacturer = "Trimui"},

    // Sentinel
    {NULL, NULL, NULL}};

// Variant configuration table
typedef struct {
	VariantType variant;
	int screen_width;
	int screen_height;
	float screen_diagonal_default;
	uint32_t hw_features;
} VariantConfig;

static const VariantConfig tg5040_variants[] = {
    {.variant = VARIANT_TG5040_STANDARD,
     .screen_width = 1280,
     .screen_height = 720,
     .screen_diagonal_default = 4.95f,
     .hw_features = HW_FEATURE_ANALOG | HW_FEATURE_RUMBLE},
    {.variant = VARIANT_TG5040_BRICK,
     .screen_width = 1024,
     .screen_height = 768,
     .screen_diagonal_default = 3.2f,
     .hw_features = HW_FEATURE_ANALOG | HW_FEATURE_RUMBLE},
    {.variant = VARIANT_NONE} // Sentinel
};

// Device-to-variant mapping
typedef struct {
	const char* device_string; // What to look for in DEVICE env var
	VariantType variant;
	const DeviceInfo* device;
} DeviceVariantMap;

static const DeviceVariantMap tg5040_device_map[] = {
    // Standard TG5040 - VARIANT_TG5040_STANDARD
    {"smartpro", VARIANT_TG5040_STANDARD, &tg5040_devices[0]},

    // Brick variant - VARIANT_TG5040_BRICK
    {"brick", VARIANT_TG5040_BRICK, &tg5040_devices[1]},

    // Sentinel
    {NULL, VARIANT_NONE, NULL}};

static const VariantConfig* getVariantConfig(VariantType variant) {
	for (int i = 0; tg5040_variants[i].variant != VARIANT_NONE; i++) {
		if (tg5040_variants[i].variant == variant)
			return &tg5040_variants[i];
	}
	return NULL;
}

void PLAT_detectVariant(PlatformVariant* v) {
	v->platform = PLATFORM;
	v->has_hdmi = 0;

	// Read device string from environment
	char* device = getenv("DEVICE");

	// Look up device in mapping table
	const DeviceVariantMap* map = NULL;
	if (device) {
		for (int i = 0; tg5040_device_map[i].device_string != NULL; i++) {
			if (exactMatch((char*)tg5040_device_map[i].device_string, device)) {
				map = &tg5040_device_map[i];
				break;
			}
		}
	}

	// Fallback to default if not found
	if (!map) {
		LOG_warn("Unknown DEVICE '%s', defaulting to Smart Pro\n", device ? device : "(unset)");
		map = &tg5040_device_map[0]; // Smart Pro
	}

	// Set device info
	v->device = map->device;
	v->variant = map->variant;

	// Apply variant configuration
	const VariantConfig* config = getVariantConfig(map->variant);
	if (config) {
		v->screen_width = config->screen_width;
		v->screen_height = config->screen_height;
		v->screen_diagonal = config->screen_diagonal_default;
		v->hw_features = config->hw_features;
	}

	LOG_info("Detected device: %s %s (%s variant, %dx%d, %.1f\")\n", v->device->manufacturer,
	         v->device->display_name,
	         v->variant == VARIANT_TG5040_BRICK ? "Brick (4:3)" : "standard (16:9)",
	         v->screen_width, v->screen_height, v->screen_diagonal);
}

///////////////////////////////
// Video - Using shared SDL2 backend
///////////////////////////////

static SDL2_RenderContext vid_ctx;

static const SDL2_Config vid_config = {
    // No rotation needed (landscape display)
    .auto_rotate = 0,
    .rotate_cw = 0,
    .rotate_null_center = 0,
    // Display features
    .has_hdmi = 0,
    .default_sharpness = SHARPNESS_SOFT,
};

SDL_Surface* PLAT_initVideo(void) {
	// Detect device variant
	PLAT_detectVariant(&platform_variant);

	return SDL2_initVideo(&vid_ctx, FIXED_WIDTH, FIXED_HEIGHT, &vid_config);
}

void PLAT_quitVideo(void) {
	SDL2_quitVideo(&vid_ctx);
	system("cat /dev/zero > /dev/fb0 2>/dev/null");
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL2_clearVideo(&vid_ctx);
}

void PLAT_clearAll(void) {
	SDL2_clearAll(&vid_ctx);
}

void PLAT_setVsync(int vsync) {
	// Vsync handled by SDL_RENDERER_PRESENTVSYNC in SDL2 backend
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	return SDL2_resizeVideo(&vid_ctx, w, h, p);
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// Not supported on this platform
}

void PLAT_setNearestNeighbor(int enabled) {
	// Always enabled via sharpness setting
}

void PLAT_setSharpness(int sharpness) {
	SDL2_setSharpness(&vid_ctx, sharpness);
}

void PLAT_setEffect(int effect) {
	SDL2_setEffect(&vid_ctx, effect);
}

void PLAT_setEffectColor(int color) {
	SDL2_setEffectColor(&vid_ctx, color);
}

void PLAT_vsync(int remaining) {
	SDL2_vsync(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return SDL2_getScaler(&vid_ctx, renderer);
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	SDL2_blitRenderer(&vid_ctx, renderer);
}

void PLAT_flip(SDL_Surface* screen, int sync) {
	SDL2_flip(&vid_ctx, sync);
}

///////////////////////////////
// Input
///////////////////////////////

static SDL_Joystick* joystick;

void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}

void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////
// Power and Hardware
///////////////////////////////

static int online = 0;

/**
 * Reads battery status from AXP2202 power management IC.
 *
 * Quantizes battery level to reduce UI noise during gameplay.
 * Also checks WiFi status via network interface state.
 *
 * @param is_charging Set to 1 if USB power connected
 * @param charge Set to quantized battery level (10-100)
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	int i = getInt("/sys/class/power_supply/axp2202-battery/capacity");
	// Quantize battery level to reduce UI flicker during gameplay
	if (i > 80)
		*charge = 100;
	else if (i > 60)
		*charge = 80;
	else if (i > 40)
		*charge = 60;
	else if (i > 20)
		*charge = 40;
	else if (i > 10)
		*charge = 20;
	else
		*charge = 10;

	// WiFi status (polled during battery check)
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status, 16);
	online = prefixMatch("up", status);
}

#define LED_PATH1 "/sys/class/led_anim/max_scale"
#define LED_PATH2 "/sys/class/led_anim/max_scale_lr"
#define LED_PATH3 "/sys/class/led_anim/max_scale_f1f2" // front facing (Brick only)

/**
 * Enables or disables LED indicators.
 *
 * Brick variant has three LED zones that are all controlled.
 * LED brightness is 60 when enabled, 0 (off) when disabled.
 *
 * @param enable 1 to turn LEDs on, 0 to turn off
 */
static void PLAT_enableLED(int enable) {
	if (enable) {
		putInt(LED_PATH1, 60);
		if (VARIANT_IS(VARIANT_TG5040_BRICK))
			putInt(LED_PATH2, 60);
		if (VARIANT_IS(VARIANT_TG5040_BRICK))
			putInt(LED_PATH3, 60);
	} else {
		putInt(LED_PATH1, 0);
		if (VARIANT_IS(VARIANT_TG5040_BRICK))
			putInt(LED_PATH2, 0);
		if (VARIANT_IS(VARIANT_TG5040_BRICK))
			putInt(LED_PATH3, 0);
	}
}

#define BLANK_PATH "/sys/class/graphics/fb0/blank"

/**
 * Enables or disables backlight and LEDs.
 *
 * On Brick variant, sets minimum brightness to 8 when waking
 * to prevent completely black screen.
 *
 * @param enable 1 to wake, 0 to sleep
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		// Brick needs minimum brightness to be visible
		if (VARIANT_IS(VARIANT_TG5040_BRICK))
			SetRawBrightness(8);
		SetBrightness(GetBrightness());
	} else {
		SetRawBrightness(0);
	}
	PLAT_enableLED(!enable);
}

/**
 * Powers off the device.
 *
 * Breaks MinUI launch loop by removing /tmp/minui_exec.
 * Actual shutdown handled by PLATFORM/bin/shutdown script.
 */
void PLAT_powerOff(void) {
	// Break the MinUI.pak/launch.sh while loop
	unlink("/tmp/minui_exec");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0); // Also turns on LEDs via PLAT_enableLED(!enable)
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	exit(0); // Poweroff handled by PLATFORM/bin/shutdown
}

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"

/**
 * Sets CPU frequency based on performance mode.
 *
 * Frequencies:
 * - MENU: 1.2GHz (64-bit needs more headroom than 32-bit platforms)
 * - POWERSAVE: 1.2GHz
 * - NORMAL: 1.608GHz
 * - PERFORMANCE: 2GHz (maximum)
 *
 * @param speed CPU_SPEED_* constant
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
	case CPU_SPEED_MENU:
		freq = 800000;
		break;
	case CPU_SPEED_POWERSAVE:
		freq = 1200000;
		break;
	case CPU_SPEED_NORMAL:
		freq = 1608000;
		break;
	case CPU_SPEED_PERFORMANCE:
		freq = 2000000;
		break;
	}
	putInt(GOVERNOR_PATH, freq);
}

#define RUMBLE_PATH "/sys/class/gpio/gpio227/value"

/**
 * Controls rumble motor.
 *
 * Rumble disabled when muted to respect user audio preferences.
 *
 * @param strength 0=off, non-zero=on
 */
void PLAT_setRumble(int strength) {
	putInt(RUMBLE_PATH, (strength && !GetMute()) ? 1 : 0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Returns device model name.
 *
 * Uses TRIMUI_MODEL environment variable if set.
 *
 * @return Model string (e.g., "Trimui Smart Pro")
 */
char* PLAT_getModel(void) {
	char* model = getenv("TRIMUI_MODEL");
	if (model)
		return model;
	return "Trimui Smart Pro";
}

/**
 * Returns network online status.
 *
 * @return 1 if WiFi connected, 0 otherwise
 */
int PLAT_isOnline(void) {
	return online;
}
