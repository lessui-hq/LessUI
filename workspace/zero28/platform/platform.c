/**
 * zero28/platform/platform.c - Platform implementation for Mini Zero 28 handheld
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Provides hardware-specific implementation of the MinUI platform API for the
 * Mini Zero 28 device. Key features:
 *
 * - SDL_Joystick API for input (instead of raw evdev)
 * - Display rotation support for portrait/landscape modes
 * - Grid and line overlay effects via render_sdl2
 * - WiFi connectivity detection
 * - AXP2202 battery monitoring
 * - External bl_enable/bl_disable scripts for backlight control
 *
 * The Zero28 uses 640x480 VGA resolution with 2x scaling and supports both
 * soft (bilinear) and crisp (nearest neighbor + linear downscale) rendering.
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
// Input
///////////////////////////////

// Uses SDL_Joystick API instead of raw evdev for button/axis input
static SDL_Joystick* joystick;

/**
 * Initializes joystick input subsystem.
 *
 * Opens the first available joystick device (index 0) using SDL.
 * The Zero28 uses SDL_Joystick for all input including D-pad (HAT),
 * buttons, and analog sticks.
 */
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}

/**
 * Shuts down joystick input subsystem.
 *
 * Closes the joystick device and cleans up SDL joystick subsystem.
 */
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////
// Video - Using shared SDL2 backend
///////////////////////////////

static SDL2_RenderContext vid_ctx;

static const SDL2_Config vid_config = {
    // Rotation: 90° CW with {0,0} center (unique to zero28)
    .auto_rotate = 1,
    .rotate_cw = 1,
    .rotate_null_center = 0,
    // Display features
    .has_hdmi = 0,
    .default_sharpness = SHARPNESS_SOFT,
};

SDL_Surface* PLAT_initVideo(void) {
	return SDL2_initVideo(&vid_ctx, FIXED_WIDTH, FIXED_HEIGHT, &vid_config);
}

void PLAT_quitVideo(void) {
	SDL2_quitVideo(&vid_ctx);
	// Directly blank framebuffer to prevent visual artifacts
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
	// Not implemented
}

void PLAT_setNearestNeighbor(int enabled) {
	// Scaling controlled by sharpness mode
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
// Power Management
///////////////////////////////

// WiFi connectivity state (updated during battery polling)
static int online = 0;

/**
 * Gets battery and charging status.
 *
 * Reads battery level from AXP2202 power management IC via sysfs.
 * The Zero28 uses different battery paths than other platforms:
 * - /sys/class/power_supply/axp2202-battery/capacity
 * - /sys/class/power_supply/axp2202-usb/online
 *
 * Also polls WiFi status as a convenience (updated during regular
 * battery polling to avoid separate polling).
 *
 * @param is_charging Set to 1 if USB power connected, 0 otherwise
 * @param charge Set to battery level (10-100 in 20% increments)
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// Check USB power connection (AXP2202-specific path)
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	// Read battery capacity and round to nearest 20%
	int i = getInt("/sys/class/power_supply/axp2202-battery/capacity");
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

	// Update WiFi status (polled here to avoid separate polling loop)
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status, 16);
	online = prefixMatch("up", status);
}

#define BLANK_PATH "/sys/class/graphics/fb0/blank"

/**
 * Enables or disables backlight.
 *
 * The Zero28 uses external bl_enable/bl_disable scripts for backlight
 * control in addition to standard brightness and blanking controls.
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetRawBrightness(8); // fix screen not turning back on after sleep on some board revs
		SetBrightness(GetBrightness());
		system("bl_enable"); // Platform-specific backlight enable script
		putInt(BLANK_PATH, FB_BLANK_UNBLANK);
	} else {
		SetRawBrightness(0);
		system("bl_disable"); // Platform-specific backlight disable script
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN);
	}
}

/**
 * Powers off the device.
 *
 * Performs clean shutdown sequence:
 * 1. Remove exec file and sync filesystem
 * 2. Mute audio and disable backlight
 * 3. Shutdown subsystems
 * 4. Clear framebuffer
 * 5. Power off system
 */
void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("cat /dev/zero > /dev/fb0 2>/dev/null");
	system("poweroff");
	exit(0);
}

///////////////////////////////
// CPU and Hardware Control
///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"

/**
 * Sets CPU frequency based on performance profile.
 *
 * CPU frequencies:
 * - MENU: 800 MHz (64-bit needs more headroom than 32-bit platforms)
 * - POWERSAVE: 816 MHz (low-demand games)
 * - NORMAL: 1416 MHz (most games)
 * - PERFORMANCE: 1800 MHz (demanding games)
 *
 * @param speed CPU_SPEED_* constant
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
	case CPU_SPEED_IDLE:
		freq = 408000; // 20% of max (408 MHz)
		break;
	case CPU_SPEED_POWERSAVE:
		freq = 1008000; // 55% of max (1008 MHz)
		break;
	case CPU_SPEED_NORMAL:
		freq = 1416000; // 80% of max (1416 MHz)
		break;
	case CPU_SPEED_PERFORMANCE:
		freq = 1800000; // 100% (1800 MHz)
		break;
	}
	putInt(GOVERNOR_PATH, freq);
}

/**
 * Gets available CPU frequencies from sysfs.
 *
 * @param frequencies Output array to fill with frequencies (in kHz)
 * @param max_count Maximum number of frequencies to return
 * @return Number of frequencies found
 */
int PLAT_getAvailableCPUFrequencies(int* frequencies, int max_count) {
	return PWR_getAvailableCPUFrequencies_sysfs(frequencies, max_count);
}

/**
 * Sets CPU frequency directly via sysfs.
 *
 * @param freq_khz Target frequency in kHz
 * @return 0 on success, -1 on failure
 */
int PLAT_setCPUFrequency(int freq_khz) {
	return PWR_setCPUFrequency_sysfs(freq_khz);
}

#define RUMBLE_PATH "/sys/class/gpio/gpio227/value"

/**
 * Sets rumble motor strength (not implemented).
 *
 * @param strength Rumble strength (0-100, ignored)
 */
void PLAT_setRumble(int strength) {
	// Not implemented
}

/**
 * Selects appropriate audio sample rate.
 *
 * @param requested Requested sample rate
 * @param max Maximum supported sample rate
 * @return Lesser of requested or max
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Gets device model name.
 *
 * @return "Mini Zero 28"
 */
char* PLAT_getModel(void) {
	return "Mini Zero 28";
}

/**
 * Checks if device is connected to WiFi.
 *
 * Status is updated during battery polling (see PLAT_getBatteryStatus).
 *
 * @return 1 if WiFi connected, 0 otherwise
 */
int PLAT_isOnline(void) {
	return online;
}
