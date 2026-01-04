/**
 * platform.c - Trimui Smart Pro S (TG5050) platform implementation
 *
 * Uses shared render_sdl2 backend.
 *
 * Hardware features:
 * - SDL2-based video with sharpness control (via render_sdl2)
 * - Joystick input via SDL2
 * - Display effects (scanlines, grid with DMG color support)
 * - AXP2202 power management
 * - LED control (three LED zones)
 * - CPU frequency scaling (disabled - using schedutil governor)
 * - Rumble motor support (GPIO 236)
 *
 * Key differences from TG5040:
 * - Backlight via sysfs (not /dev/disp ioctl)
 * - Different audio mixer controls (DAC Volume via tinyalsa)
 * - Rumble on GPIO 236 (not 227)
 * - Speaker mute via sysfs
 * - Dual-cluster CPU architecture (scaling disabled for now)
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

#include "gl_video.h"
#include "render_sdl2.h"
#include "scaler.h"

///////////////////////////////
// Device Registry
///////////////////////////////

// Single device - no variants
static const DeviceInfo tg5050_device = {
    .device_id = "tg5050", .display_name = "Smart Pro S", .manufacturer = "Trimui"};

void PLAT_detectVariant(PlatformVariant* v) {
	v->platform = PLATFORM;
	v->has_hdmi = 0;
	v->device = &tg5050_device;
	v->variant = VARIANT_NONE;
	v->variant_name = NULL;

	// Fixed screen dimensions (no variants)
	v->screen_width = FIXED_WIDTH;
	v->screen_height = FIXED_HEIGHT;
	v->screen_diagonal = SCREEN_DIAGONAL;
	v->hw_features = HW_FEATURE_ANALOG | HW_FEATURE_RUMBLE;

	LOG_info("Detected device: %s %s (%dx%d, %.1f\")\n", v->device->manufacturer,
	         v->device->display_name, v->screen_width, v->screen_height, v->screen_diagonal);
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
	// Detect device
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
	// Only GL path is used on GLES platforms (SDL2 effect state is unused)
	GLVideo_setEffect(effect);
}

void PLAT_setEffectColor(int color) {
	// Only GL path is used on GLES platforms (SDL2 effect state is unused)
	GLVideo_setEffectColor(color);
}

void PLAT_vsync(int remaining) {
	SDL2_vsync(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return SDL2_getScaler(&vid_ctx, renderer);
}

void PLAT_present(GFX_Renderer* renderer) {
	SDL2_present(&vid_ctx, renderer);
}

SDL_Window* PLAT_getWindow(void) {
	return SDL2_getWindow(&vid_ctx);
}

int PLAT_getRotation(void) {
	return SDL2_getRotation(&vid_ctx);
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
#define LED_PATH2 "/sys/class/led_anim/effect_l" // Left joystick LED
#define LED_PATH3 "/sys/class/led_anim/effect_r" // Right joystick LED
#define LED_PATH4 "/sys/class/led_anim/effect_m" // Logo LED

/**
 * Enables or disables LED indicators.
 *
 * TG5050 has three LED zones (left, right, logo).
 * LED brightness is 60 when enabled, 0 (off) when disabled.
 *
 * @param enable 1 to turn LEDs on, 0 to turn off
 */
static void PLAT_enableLED(int enable) {
	putInt(LED_PATH1, enable ? 60 : 0);
}

#define BACKLIGHT_PATH "/sys/class/backlight/backlight0/brightness"

/**
 * Enables or disables backlight and LEDs.
 *
 * TG5050 uses sysfs backlight control (not /dev/disp ioctl).
 *
 * @param enable 1 to wake, 0 to sleep
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
	} else {
		SetRawBrightness(0);
	}
	PLAT_enableLED(!enable);
}

/**
 * Powers off the device.
 *
 * Uses system poweroff command which handles A523 shutdown properly.
 */
void PLAT_powerOff(void) {
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0); // Also turns on LEDs via PLAT_enableLED(!enable)
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("poweroff");
	while (1)
		pause();
}

double PLAT_getDisplayHz(void) {
	return SDL2_getDisplayHz();
}

uint32_t PLAT_measureVsyncInterval(void) {
	return SDL2_measureVsyncInterval(&vid_ctx);
}

/**
 * Sets CPU frequency based on performance mode.
 *
 * NOTE: CPU scaling is disabled for tg5050 due to dual-cluster A55 architecture.
 * Using schedutil governor and letting the kernel handle scaling.
 *
 * @param speed CPU_SPEED_* constant (ignored)
 */
void PLAT_setCPUSpeed(int speed) {
	(void)speed; // No-op for now - using schedutil governor
}

/**
 * Gets available CPU frequencies.
 *
 * Disabled for tg5050 to prevent auto-CPU scaling.
 * The dual-cluster A523 requires broader overhaul to properly support.
 *
 * @param frequencies Output array (unused)
 * @param max_count Maximum count (unused)
 * @return 0 to disable auto-CPU scaling
 */
int PLAT_getAvailableCPUFrequencies(int* frequencies, int max_count) {
	(void)frequencies;
	(void)max_count;
	return 0; // Return 0 to disable auto-CPU scaling
}

/**
 * Sets CPU frequency directly.
 *
 * Disabled for tg5050 - returns failure.
 *
 * @param freq_khz Target frequency (unused)
 * @return -1 (not supported)
 */
int PLAT_setCPUFrequency(int freq_khz) {
	(void)freq_khz;
	return -1; // Not supported
}

#define RUMBLE_GPIO_PATH "/sys/class/gpio/gpio236/value"
#define RUMBLE_LEVEL_PATH "/sys/class/motor/level"
#define RUMBLE_MAX_STRENGTH 0xFFFF

/**
 * Controls rumble motor.
 *
 * TG5050 uses GPIO 236 for on/off control and /sys/class/motor/level for intensity.
 * Rumble disabled when muted to respect user audio preferences.
 *
 * @param strength 0=off, 1-65535=intensity level
 */
void PLAT_setRumble(int strength) {
	if (GetMute()) {
		putInt(RUMBLE_LEVEL_PATH, 0);
		putInt(RUMBLE_GPIO_PATH, 0);
		return;
	}

	// Set intensity level first
	if (strength > 0 && strength < RUMBLE_MAX_STRENGTH) {
		putInt(RUMBLE_LEVEL_PATH, strength);
	} else {
		putInt(RUMBLE_LEVEL_PATH, 0);
	}

	// Then enable/disable the motor
	putInt(RUMBLE_GPIO_PATH, strength ? 1 : 0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Returns device model name.
 *
 * Uses TRIMUI_MODEL environment variable if set.
 *
 * @return Model string (e.g., "Trimui Smart Pro S")
 */
char* PLAT_getModel(void) {
	char* model = getenv("TRIMUI_MODEL");
	if (model)
		return model;
	return "Trimui Smart Pro S";
}

/**
 * Returns network online status.
 *
 * @return 1 if WiFi connected, 0 otherwise
 */
int PLAT_isOnline(void) {
	return online;
}
