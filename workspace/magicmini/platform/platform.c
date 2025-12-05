/**
 * platform.c - MagicX XU Mini M platform implementation
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Provides hardware abstraction for the MagicX XU Mini M handheld device.
 * This RK3566-based device features a 640x480 display with rotation support,
 * analog sticks (left stick via absolute events), L3/R3 buttons, and advanced
 * visual effects including scanline/grid overlays with DMG color support.
 *
 * Key Features:
 * - Analog stick support (left stick via EV_ABS events)
 * - L3/R3 buttons that also trigger MENU
 * - Display rotation support (portrait mode detection)
 * - Crisp/soft scaling modes via render_sdl2
 * - Grid and line effects via render_sdl2
 * - Brightness-based alpha blending for low brightness compensation
 * - Wi-Fi status tracking via network interface
 *
 * @note This platform uses SDL2 for video and relies on msettings library
 *       for brightness/volume control
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
// Input Configuration
///////////////////////////////

#define RAW_UP 544
#define RAW_DOWN 545
#define RAW_LEFT 546
#define RAW_RIGHT 547

#define RAW_A 308
#define RAW_B 305
#define RAW_X 307
#define RAW_Y 304

#define RAW_START 315
#define RAW_SELECT 314
#define RAW_MENU 704

#define RAW_L1 310
#define RAW_L2 313
#define RAW_L3 317
#define RAW_R1 311
#define RAW_R2 312
#define RAW_R3 318

#define RAW_PLUS 115
#define RAW_MINUS 114
#define RAW_POWER 116

#define RAW_LSY 1
#define RAW_LSX 0
#define RAW_RSY 2
#define RAW_RSX 5

#define RAW_MENU1 RAW_L3
#define RAW_MENU2 RAW_R3

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

///////////////////////////////
// Input Initialization
///////////////////////////////

/**
 * Initializes input devices for the MagicX XU Mini M.
 *
 * Opens three input event devices:
 * - event0: Power button
 * - event2: Gamepad (buttons and analog sticks)
 * - event3: Volume buttons
 *
 * All devices are opened with O_NONBLOCK to prevent blocking reads
 * and O_CLOEXEC to prevent inheritance by child processes.
 */
void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // gamepad
	inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume
}

/**
 * Closes all input device file descriptors.
 */
void PLAT_quitInput(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY 0x01
#define EV_ABS 0x03

///////////////////////////////
// Input Polling
///////////////////////////////

/**
 * Polls all input devices and updates the global pad state.
 *
 * Reads input events from all three input devices (power, gamepad, volume)
 * and translates raw input codes to button masks. Handles both digital buttons
 * (EV_KEY) and analog sticks (EV_ABS). The left analog stick generates digital
 * button presses via PAD_setAnalog().
 *
 * Special behavior:
 * - L3 and R3 buttons also trigger BTN_MENU
 * - Analog stick values are stored in pad.laxis and pad.raxis
 * - Button repeat timing is automatically handled for held buttons
 *
 * @note Transient state (just_pressed, just_released, just_repeated) is
 *       reset at the start of each poll
 */
void PLAT_pollInput(void) {
	uint32_t tick = SDL_GetTicks();
	PAD_beginPolling();
	PAD_handleRepeat(tick);

	// Poll input devices
	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type != EV_KEY && event.type != EV_ABS)
				continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int type = event.type;
			int code = event.code;
			int value = event.value;

			if (type == EV_KEY) {
				if (value > 1)
					continue; // ignore kernel key repeats (we handle repeats ourselves)

				pressed = value;
				// LOG_info("key event: %i (%i)\n", code,pressed);
				if (code == RAW_UP) {
					btn = BTN_DPAD_UP;
				} else if (code == RAW_DOWN) {
					btn = BTN_DPAD_DOWN;
				} else if (code == RAW_LEFT) {
					btn = BTN_DPAD_LEFT;
				} else if (code == RAW_RIGHT) {
					btn = BTN_DPAD_RIGHT;
				} else if (code == RAW_A) {
					btn = BTN_A;
				} else if (code == RAW_B) {
					btn = BTN_B;
				} else if (code == RAW_X) {
					btn = BTN_X;
				} else if (code == RAW_Y) {
					btn = BTN_Y;
				} else if (code == RAW_START) {
					btn = BTN_START;
				} else if (code == RAW_SELECT) {
					btn = BTN_SELECT;
				} else if (code == RAW_MENU) {
					btn = BTN_MENU;
				} else if (code == RAW_MENU1) {
					btn = BTN_MENU;
				} else if (code == RAW_MENU2) {
					btn = BTN_MENU;
				} else if (code == RAW_L1) {
					btn = BTN_L1;
				} else if (code == RAW_L2) {
					btn = BTN_L2;
				} else if (code == RAW_L3) {
					btn = BTN_L3;
				} else if (code == RAW_R1) {
					btn = BTN_R1;
				} else if (code == RAW_R2) {
					btn = BTN_R2;
				} else if (code == RAW_R3) {
					btn = BTN_R3;
				} else if (code == RAW_PLUS) {
					btn = BTN_PLUS;
				} else if (code == RAW_MINUS) {
					btn = BTN_MINUS;
				} else if (code == RAW_POWER) {
					btn = BTN_POWER;
				}
			} else if (type == EV_ABS) {
				// Analog stick events - left stick generates digital button presses
				LOG_info("abs event: %i (%i)\n", code, value);
				if (code == RAW_LSX) {
					pad.laxis.x = value;
					PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x,
					              tick + PAD_REPEAT_DELAY);
				} else if (code == RAW_LSY) {
					pad.laxis.y = value;
					PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y,
					              tick + PAD_REPEAT_DELAY);
				} else if (code == RAW_RSX) {
					pad.raxis.x = value;
				} else if (code == RAW_RSY) {
					pad.raxis.y = value;
				}
			}

			if (btn == BTN_NONE)
				continue;

			PAD_updateButton(btn, pressed, tick);
			// L3/R3 buttons also trigger MENU button
			if (btn == BTN_L3 || btn == BTN_R3)
				PAD_updateButton(BTN_MENU, pressed, tick);
		}
	}
}

/**
 * Checks if the device should wake from sleep.
 *
 * Drains the input event queue looking for a power button release event.
 * This is called during sleep mode to detect wake-up triggers.
 *
 * @return 1 if power button was released, 0 otherwise
 */
int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type == EV_KEY && event.code == RAW_POWER && event.value == 0) {
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////
// Video - Using shared SDL2 backend
///////////////////////////////

static SDL2_RenderContext vid_ctx;

static const SDL2_Config vid_config = {
    // Rotation: 270° CCW with NULL center (rotate around rect center)
    .auto_rotate = 1,
    .rotate_cw = 0,
    .rotate_null_center = 1,
    // Display features
    .has_hdmi = 0,
    .default_sharpness = SHARPNESS_SOFT,
};

SDL_Surface* PLAT_initVideo(void) {
	return SDL2_initVideo(&vid_ctx, FIXED_WIDTH, FIXED_HEIGHT, &vid_config);
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
	// Not implemented on this platform
}

void PLAT_setNearestNeighbor(int enabled) {
	// Use PLAT_setSharpness() for scaling control
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

static int online = 0; // Wi-Fi connection status

/**
 * Gets battery charging status and charge level.
 *
 * Reads battery status from sysfs. Charge level is bucketed into 6 levels
 * (10%, 20%, 40%, 60%, 80%, 100%) to reduce UI flicker from minor fluctuations.
 *
 * @param is_charging Output: 1 if charging, 0 if not
 * @param charge Output: Battery percentage (10, 20, 40, 60, 80, or 100)
 *
 * @note Wi-Fi status tracking is currently disabled but could be polled here
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/ac/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
	// Bucket charge level to reduce UI flicker from minor fluctuations
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
}

#define BACKLIGHT_PATH "/sys/class/backlight/backlight/bl_power"

/**
 * Enables or disables the backlight.
 *
 * When disabling, sets brightness to 0 and powers down the backlight.
 * When enabling, restores previous brightness and powers up the backlight.
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		putInt(BACKLIGHT_PATH, FB_BLANK_UNBLANK);
	} else {
		SetRawBrightness(0);
		system("dd if=/dev/zero of=/dev/fb0"); // Clear framebuffer
		putInt(BACKLIGHT_PATH, FB_BLANK_POWERDOWN);
	}
}

/**
 * Powers off the device.
 *
 * Performs cleanup sequence:
 * 1. Removes exec flag and syncs filesystem
 * 2. Waits 2 seconds for sync to complete
 * 3. Mutes audio and disables backlight
 * 4. Shuts down subsystems
 * 5. Exits process (system will handle power-off)
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
	exit(0);
}

///////////////////////////////
// CPU/GPU Control
///////////////////////////////

#define CPU_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
#define GPU_PATH "/sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/governor"
#define DMC_PATH "/sys/devices/platform/dmc/devfreq/dmc/governor"

/**
 * Sets CPU/GPU/memory frequency based on performance mode.
 *
 * RK3566 frequency settings:
 * - MENU: 800MHz (64-bit needs more headroom than 32-bit platforms)
 * - POWERSAVE: 816MHz (conservative for simple games)
 * - NORMAL: 1.416GHz (balanced for most games)
 * - PERFORMANCE: 2.016GHz (maximum, GPU/DMC also set to performance)
 *
 * @param speed CPU_SPEED_IDLE, CPU_SPEED_POWERSAVE, CPU_SPEED_NORMAL,
 *              or CPU_SPEED_PERFORMANCE
 *
 * @note PERFORMANCE mode may not be stable on all chips (depends on binning)
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
	case CPU_SPEED_IDLE:
		freq = 408000; // 20% of max (360 → 408 MHz)
		break;
	case CPU_SPEED_POWERSAVE:
		freq = 1008000; // 55% of max (990 → 1008 MHz)
		break;
	case CPU_SPEED_NORMAL:
		freq = 1416000; // 80% of max (1440 → 1416 MHz)
		break;
	case CPU_SPEED_PERFORMANCE:
		freq = 1800000; // 100% (1800 MHz)
		break;
	}

	// Performance mode: maximize GPU and memory controller
	if (speed == CPU_SPEED_PERFORMANCE) {
		putFile(GPU_PATH, "performance");
		putFile(DMC_PATH, "performance");
	} else {
		putFile(GPU_PATH, "simple_ondemand");
		putFile(DMC_PATH, "dmc_ondemand");
	}
	putInt(CPU_PATH, freq);
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

/**
 * Sets rumble/vibration strength (not supported on this platform).
 *
 * @param strength Ignored (no vibration motor)
 */
void PLAT_setRumble(int strength) {
	// Not supported on this platform
}

/**
 * Picks appropriate audio sample rate for the platform.
 *
 * @param requested Requested sample rate
 * @param max Maximum allowed sample rate
 * @return Chosen sample rate (minimum of requested and max)
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Gets the device model name.
 *
 * @return Device model string
 */
char* PLAT_getModel(void) {
	return "MagicX XU Mini M";
}

/**
 * Checks if device is online (Wi-Fi connected).
 *
 * @return 1 if online, 0 if offline
 *
 * @note Currently always returns 0 (Wi-Fi status polling is disabled)
 */
int PLAT_isOnline(void) {
	return online;
}
