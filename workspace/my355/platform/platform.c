/**
 * platform.c - Platform implementation for Miyoo Flip (MY355)
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Implements the hardware abstraction layer for the Miyoo Flip device,
 * featuring:
 * - Hall sensor lid detection (lid open/close events)
 * - HDMI output detection and handling
 * - Display rotation (disabled when using HDMI)
 * - WiFi status monitoring
 * - Rumble support (disabled when using HDMI)
 * - Sharpness scaling (crisp/soft) via render_sdl2
 * - Overlay effects (scanlines/grids) via render_sdl2
 *
 * Hardware specifics:
 * - Built-in screen: 640x480 display (rotatable)
 * - HDMI output: 1280x720 (no rotation)
 * - Hall sensor for lid detection at /sys/devices/platform/hall-mh248/hallvalue
 * - Rumble motor controlled via GPIO20
 */

#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <msettings.h>

#include "api.h"
#include "defines.h"
#include "platform.h"
#include "utils.h"

#include "render_sdl2.h"
#include "scaler.h"

///////////////////////////////
// Lid Detection (Hall Sensor)
///////////////////////////////

// Hall sensor path: reports 1 when lid is open, 0 when closed
#define LID_PATH "/sys/devices/platform/hall-mh248/hallvalue"

/**
 * Initializes lid detection hardware.
 *
 * Checks for the presence of the hall sensor sysfs interface
 * to determine if the device supports lid detection.
 * Updates the global lid.has_lid flag accordingly.
 */
void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}

/**
 * Checks if lid state has changed since last call.
 *
 * Polls the hall sensor to detect lid open/close events.
 * This enables automatic sleep when the lid is closed.
 *
 * @param state Output pointer to receive new lid state (1=open, 0=closed), can be NULL
 * @return 1 if lid state changed, 0 if unchanged or no lid sensor present
 *
 * @note Updates global lid.is_open state on change
 */
int PLAT_lidChanged(int* state) {
	if (lid.has_lid) {
		int lid_open = getInt(LID_PATH);
		if (lid_open != lid.is_open) {
			lid.is_open = lid_open;
			if (state)
				*state = lid_open;
			return 1;
		}
	}
	return 0;
}

///////////////////////////////
// Input Management
///////////////////////////////

static SDL_Joystick* joystick;

/**
 * Initializes input subsystem.
 *
 * Opens the device's joystick/gamepad interface via SDL.
 */
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}

/**
 * Shuts down input subsystem.
 *
 * Closes joystick handle and cleans up SDL input resources.
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
    // Rotation: 270° CCW with {0,0} center
    .auto_rotate = 1,
    .rotate_cw = 0,
    .rotate_null_center = 0,
    // Display features
    .has_hdmi = 1,
    .default_sharpness = SHARPNESS_SOFT,
};

#define HDMI_STATE_PATH "/sys/class/drm/card0-HDMI-A-1/status"

/**
 * Checks if HDMI cable is currently connected.
 *
 * Reads the DRM subsystem status to detect HDMI hotplug events.
 * Used during video initialization to select appropriate resolution.
 *
 * @return 1 if HDMI connected, 0 if disconnected
 */
static int HDMI_enabled(void) {
	char value[64];
	getFile(HDMI_STATE_PATH, value, 64);
	return exactMatch(value, "connected\n");
}

SDL_Surface* PLAT_initVideo(void) {
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;

	// Check for HDMI before settings are loaded
	if (HDMI_enabled()) {
		w = HDMI_WIDTH;
		h = HDMI_HEIGHT;
		vid_ctx.on_hdmi = 1;
	}

	return SDL2_initVideo(&vid_ctx, w, h, &vid_config);
}

void PLAT_quitVideo(void) {
	SDL2_quitVideo(&vid_ctx);
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
	// Use sharpness setting instead
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

void PLAT_present(GFX_Renderer* renderer) {
	vid_ctx.on_hdmi = GetHDMI();
	SDL2_present(&vid_ctx, renderer);
}

/**
 * Checks if platform supports overscan adjustment.
 *
 * @return 0 (overscan not supported on this platform)
 */
int PLAT_supportsOverscan(void) {
	return 0;
}

///////////////////////////////
// Power and Battery Management
///////////////////////////////

#define BLANK_PATH "/sys/class/backlight/backlight/bl_power"

static int online = 0;

/**
 * Gets battery charge level and charging status.
 *
 * Also monitors WiFi connection status as a side effect (updates online flag).
 * Battery percentage is simplified into 6 levels to reduce icon flickering.
 *
 * @param is_charging Output pointer for charging state (1=charging, 0=on battery)
 * @param charge Output pointer for charge level (10/20/40/60/80/100)
 *
 * @note Also updates global 'online' flag from wlan0 interface state
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/ac/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
	// worry less about battery and more about the game you're playing
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

	// wifi status, just hooking into the regular PWR polling
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status, 16);
	online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/leds/work/brightness"

/**
 * Enables or disables screen backlight.
 *
 * When disabling:
 * - Powers down display completely (FB_BLANK_POWERDOWN)
 * - Sets brightness to 0
 * - Turns on LED indicator (255 = full brightness)
 *
 * When enabling:
 * - Wakes display (FB_BLANK_UNBLANK)
 * - Restores user brightness setting
 * - Turns off LED indicator
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK); // wake
		SetBrightness(GetBrightness());
		putInt(LED_PATH, 0);
	} else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN); // sleep
		SetRawBrightness(0);
		putInt(LED_PATH, 255);
	}
}

/**
 * Performs graceful system shutdown.
 *
 * Calls shutdown script directly for consistent behavior regardless of
 * which process triggers the shutdown (launcher, player, shui, or paks).
 */
void PLAT_powerOff(void) {
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	system("echo 255 > /sys/class/leds/work/brightness");
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("shutdown");
	while (1)
		pause();
}

double PLAT_getDisplayHz(void) {
	return SDL2_getDisplayHz();
}

uint32_t PLAT_measureVsyncInterval(void) {
	return SDL2_measureVsyncInterval(&vid_ctx);
}

///////////////////////////////
// CPU and Performance
///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"

/**
 * Sets CPU clock speed based on performance level.
 *
 * Speed mappings:
 * - CPU_SPEED_IDLE:        800 MHz (64-bit needs more headroom)
 * - CPU_SPEED_POWERSAVE:  1104 MHz (battery-friendly gaming)
 * - CPU_SPEED_NORMAL:     1608 MHz (default gaming)
 * - CPU_SPEED_PERFORMANCE: 1992 MHz (demanding games)
 *
 * @param speed CPU speed constant
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
	case CPU_SPEED_IDLE:
		freq = 408000; // 20% of max (408 MHz)
		break;
	case CPU_SPEED_POWERSAVE:
		freq = 1104000; // 55% of max (1104 MHz)
		break;
	case CPU_SPEED_NORMAL:
		freq = 1608000; // 80% of max (1608 MHz)
		break;
	case CPU_SPEED_PERFORMANCE:
		freq = 1992000; // 100% (1992 MHz)
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

///////////////////////////////
// Rumble
///////////////////////////////

#define RUMBLE_PATH "/sys/class/gpio/gpio20/value"

/**
 * Controls rumble motor.
 *
 * Disabled when HDMI is active (assumes external controller is in use).
 *
 * @param strength Rumble strength (0=off, non-zero=on, binary control only)
 */
void PLAT_setRumble(int strength) {
	if (GetHDMI())
		return; // Disable rumble on HDMI (likely using controller)
	putInt(RUMBLE_PATH, strength ? 1 : 0);
}

///////////////////////////////
// Audio
///////////////////////////////

/**
 * Selects audio sample rate based on constraints.
 *
 * @param requested Desired sample rate
 * @param max Maximum supported sample rate
 * @return Selected sample rate (minimum of requested and max)
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

///////////////////////////////
// Platform Info
///////////////////////////////

/**
 * Returns platform model name.
 *
 * @return "Miyoo Flip"
 */
char* PLAT_getModel(void) {
	return "Miyoo Flip";
}

/**
 * Checks if WiFi is currently connected.
 *
 * @return 1 if WiFi connected, 0 if disconnected
 *
 * @note Updated by PLAT_getBatteryStatus() which polls wlan0 state
 */
int PLAT_isOnline(void) {
	return online;
}
