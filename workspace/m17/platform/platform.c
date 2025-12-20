/**
 * platform.c - Powkiddy RGB10 Max (M17) platform implementation
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Implements the hardware abstraction layer for the Powkiddy RGB10 Max (M17),
 * a 5-inch horizontal handheld gaming device with SDL2-based video rendering.
 *
 * Hardware features:
 * - Display: 1280x720 (720p) IPS screen at ~73Hz
 * - Input: D-pad, 4 face buttons, dual shoulder buttons, plus/minus buttons
 * - Video: SDL2 Window/Renderer/Texture API with hardware acceleration (via render_sdl2)
 * - No analog sticks
 *
 * Platform specifics:
 * - Fixed CPU speed (1200000 MHz, cannot be changed)
 * - Uses evdev input (4 event devices)
 * - Sharpness setting: supports both soft (linear) and crisp (nearest neighbor) scaling
 * - Battery charging detection may be unreliable (see getBatteryStatus comment)
 */

// m17
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
// Input handling (evdev)
///////////////////////////////

#define RAW_UP 103
#define RAW_DOWN 108
#define RAW_LEFT 105
#define RAW_RIGHT 106
#define RAW_A 48
#define RAW_B 30
#define RAW_X 21
#define RAW_Y 45
#define RAW_START 28
#define RAW_SELECT 54
#define RAW_MENU 115
#define RAW_L1 38
#define RAW_L2 44
#define RAW_R1 19
#define RAW_R2 46
#define RAW_PLUS 115
#define RAW_MINUS 114

#define RAW_MENU1 RAW_PLUS
#define RAW_MENU2 RAW_MINUS

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT];

/**
 * Initializes input system by opening evdev devices.
 *
 * Opens 4 event devices for reading button/key events.
 */
void PLAT_initInput(void) {
	char path[256];
	for (int i = 0; i < INPUT_COUNT; i++) {
		snprintf(path, sizeof(path), "/dev/input/event%i", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (inputs[i] < 0)
			LOG_warn("Failed to open /dev/input/event%d\n", i);
	}
}
/**
 * Closes input system and cleans up resources.
 */
void PLAT_quitInput(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

// Struct from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY 0x01
#define EV_ABS 0x03

/**
 * Polls input devices and updates global pad state.
 *
 * Reads events from all input devices and translates hardware button codes
 * to Launcher button constants. Handles button repeat timing based on PAD_REPEAT_DELAY
 * and PAD_REPEAT_INTERVAL.
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

			// TODO: tmp, hardcoded, missing some buttons
			if (type == EV_KEY) {
				if (value > 1)
					continue; // ignore repeats

				pressed = value;
				// LOG_info("key event: %i (%i)\n", code,pressed); // no L3/R3
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
				} else if (code == RAW_R1) {
					btn = BTN_R1;
				} else if (code == RAW_R2) {
					btn = BTN_R2;
				}
			} else if (type == EV_ABS) {
				// LOG_info("axis: %i (%i)\n",code,value);
				// else if (code==RAW_LSX) pad.laxis.x = (value * 32767) / 4096;
				// else if (code==RAW_LSY) pad.laxis.y = (value * 32767) / 4096;
				// else if (code==RAW_RSX) pad.raxis.x = (value * 32767) / 4096;
				// else if (code==RAW_RSY) pad.raxis.y = (value * 32767) / 4096;

				btn = BTN_NONE; // already handled, force continue
			}

			PAD_updateButton(btn, pressed, tick);
		}
	}
}

/**
 * Checks if device should wake from sleep.
 *
 * @return 1 if menu button was released, 0 otherwise
 */
int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type == EV_KEY && (event.code == RAW_MENU1 || event.code == RAW_MENU2) &&
			    event.value == 0)
				return 1;
		}
	}
	return 0;
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
	SDL2_present(&vid_ctx, renderer);
}

///////////////////////////////
// Power management
///////////////////////////////

/**
 * Reads battery status from sysfs.
 *
 * @param is_charging Pointer to store charging state (1=charging, 0=not)
 * @param charge Pointer to store battery level (10, 20, 40, 60, 80, or 100)
 *
 * @note Charging detection may be unreliable - strncmp logic seems inverted
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;

	char state[256];

	// works with old model
	getFile("/sys/class/udc/10180000.usb/state", state, 256);
	*is_charging = strncmp(
	    "not attached", state,
	    strlen(
	        "not attached")); // I don't understand how this works, if it's a match it would equal 0 which is false...

	// nothing works with new model :sob:
	// getFile("/sys/class/power_supply/battery/status", state, 256);
	// *is_charging = exactMatch(state,"Charging\n");

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
}

/**
 * Controls backlight power.
 *
 * @param enable 1 to turn on backlight, 0 to turn off
 *
 * @note Backlight control method is unclear - uses SetRawBrightness(8001) to disable
 */
void PLAT_enableBacklight(int enable) {
	// haven't figured out how to turn it off (or change brightness)
	if (!enable) {
		putInt("/sys/class/graphics/fb0/blank", 1); // clear
		SetRawBrightness(8001); // off
	} else {
		SetBrightness(GetBrightness());
	}
}

/**
 * Powers off the device.
 *
 * Mutes audio, disables backlight, shuts down subsystems, and signals poweroff.
 */
void PLAT_powerOff(void) {
	// system("leds_on");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	touch("/tmp/poweroff");
	exit(0);
}

double PLAT_getDisplayHz(void) {
	return 73.0; // M17 panel is 73Hz (480x272p73)
}

uint32_t PLAT_measureVsyncInterval(void) {
	return SDL2_measureVsyncInterval(&vid_ctx);
}

///////////////////////////////
// Platform capabilities
///////////////////////////////

/**
 * Sets CPU clock speed (not supported).
 *
 * @param speed Requested speed level (ignored)
 *
 * @note M17 has fixed 1200000 MHz clock speed
 */
void PLAT_setCPUSpeed(int speed) {
	// M17 can go any speed you like as long as that speed is 1200000
}

/**
 * Gets available CPU frequencies (not available on M17).
 *
 * M17 has fixed 1200 MHz clock speed, no dynamic scaling.
 *
 * @param frequencies Output array (unused)
 * @param max_count Maximum count (unused)
 * @return 0 (detection not available)
 */
int PLAT_getAvailableCPUFrequencies(int* frequencies, int max_count) {
	(void)frequencies;
	(void)max_count;
	return 0; // Fixed frequency platform
}

/**
 * Sets CPU frequency directly (not supported on M17).
 *
 * @param freq_khz Target frequency (ignored)
 * @return -1 (not supported)
 */
int PLAT_setCPUFrequency(int freq_khz) {
	(void)freq_khz;
	return -1; // Not supported
}

/**
 * Sets rumble/vibration strength (not supported).
 *
 * @param strength Rumble strength (ignored)
 */
void PLAT_setRumble(int strength) {
	// buh
}

/**
 * Selects audio sample rate.
 *
 * @param requested Requested sample rate
 * @param max Maximum supported sample rate
 * @return Selected sample rate (minimum of requested and max)
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Returns device model string.
 *
 * @return "M17"
 */
char* PLAT_getModel(void) {
	return "M17";
}

/**
 * Checks if device is connected to network.
 *
 * @return 0 (no network support)
 */
int PLAT_isOnline(void) {
	return 0;
}
