/**
 * platform.c - Miyoo A30 (MY282) platform implementation
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Platform-specific code for the Miyoo A30 handheld device. This platform
 * features analog stick support, display rotation, LED control, rumble
 * feedback, and grid/line visual effects.
 *
 * Key features:
 * - Analog stick input via mstick library
 * - Automatic rotation detection (portrait/landscape)
 * - LED brightness control on low-power states
 * - Motor rumble support via sysfs interface
 * - CPU frequency scaling via overclock.elf
 * - Overlay effects (scanlines/grid) with DMG color support (via render_sdl2)
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
#include <mstick.h>

#include "api.h"
#include "defines.h"
#include "platform.h"
#include "utils.h"

#include "render_sdl2.h"
#include "scaler.h"

///////////////////////////////
// Input Handling
///////////////////////////////

#define RAW_UP 103
#define RAW_DOWN 108
#define RAW_LEFT 105
#define RAW_RIGHT 106
#define RAW_A 57
#define RAW_B 29
#define RAW_X 42
#define RAW_Y 56
#define RAW_START 28
#define RAW_SELECT 97
#define RAW_MENU 1
#define RAW_L1 18
#define RAW_L2 15
#define RAW_R1 20
#define RAW_R2 14
#define RAW_PLUS 115
#define RAW_MINUS 114
#define RAW_POWER 116

#define INPUT_COUNT 2
static int inputs[INPUT_COUNT];

/**
 * Initializes input system (buttons and analog stick).
 *
 * Opens input event devices for power button (event0) and
 * controller buttons/dpad (event3). Also initializes analog
 * stick support via Stick_init() from mstick library.
 */
void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
	Stick_init(); // analog
}

/**
 * Shuts down input system and closes file descriptors.
 */
void PLAT_quitInput(void) {
	Stick_quit();
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

/**
 * Polls input devices and updates global pad state.
 *
 * Reads from all input event devices, processes button presses/releases,
 * handles key repeat logic, and updates analog stick position. Updates
 * the global pad structure with current button and analog state.
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
				} else if (code == RAW_L1) {
					btn = BTN_L1;
				} else if (code == RAW_L2) {
					btn = BTN_L2;
				} else if (code == RAW_R1) {
					btn = BTN_R1;
				} else if (code == RAW_R2) {
					btn = BTN_R2;
				} else if (code == RAW_PLUS) {
					btn = BTN_PLUS;
				} else if (code == RAW_MINUS) {
					btn = BTN_MINUS;
				} else if (code == RAW_POWER) {
					btn = BTN_POWER;
				}
			}

			PAD_updateButton(btn, pressed, tick);
		}
	}

	Stick_get(&(pad.laxis.x), &(pad.laxis.y));
	PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick + PAD_REPEAT_DELAY);
	PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y, tick + PAD_REPEAT_DELAY);
}

/**
 * Checks if device should wake from sleep.
 *
 * Polls input devices looking for power button release event.
 * Used to wake device from low-power sleep state.
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
    // Rotation: 270° CCW with {0,0} center
    .auto_rotate = 1,
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
// Power Management
///////////////////////////////

static int online = 0;

/**
 * Reads battery status from sysfs.
 *
 * Reads charging state and capacity from kernel power supply interface.
 * Battery level is quantized into 6 levels (10%, 20%, 40%, 60%, 80%, 100%).
 *
 * @param is_charging Output: 1 if USB power connected, 0 otherwise
 * @param charge Output: Battery percentage (10-100)
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/usb/online");

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

#define LED_PATH "/sys/class/leds/led1/brightness"

/**
 * Enables or disables backlight and LED indicator.
 *
 * When disabled (sleep mode), turns off LCD backlight and enables
 * LED indicator at full brightness. When enabled, restores backlight
 * to saved brightness level and turns off LED.
 *
 * @param enable 1 to enable backlight, 0 to disable (sleep)
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		putInt(LED_PATH, 0); // Turn off LED indicator
	} else {
		SetRawBrightness(0);
		putInt(LED_PATH, 255); // Full brightness LED during sleep
	}
}

/**
 * Powers off the device.
 *
 * Performs orderly shutdown: syncs filesystem, mutes audio, turns off
 * backlight, enables LED, and quits all subsystems before exiting.
 */
void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	putInt(LED_PATH, 255);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////
// System Control
///////////////////////////////

/**
 * Sets CPU frequency and core count.
 *
 * Uses overclock.elf utility to adjust CPU governor, core count,
 * and frequency based on performance profile:
 * - MENU: 576MHz, 1 core (lowest power)
 * - POWERSAVE: 1056MHz, 1 core
 * - NORMAL: 1344MHz, 2 cores
 * - PERFORMANCE: 1512MHz, 2 cores (highest performance)
 *
 * Command format: overclock.elf userspace <cores> <freq> 384 1080 0
 *
 * @param speed CPU_SPEED_MENU/POWERSAVE/NORMAL/PERFORMANCE
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	int cpu = 1;
	switch (speed) {
	case CPU_SPEED_MENU:
		freq = 576;
		cpu = 1;
		break;
	case CPU_SPEED_POWERSAVE:
		freq = 1056;
		cpu = 1;
		break;
	case CPU_SPEED_NORMAL:
		freq = 1344;
		cpu = 2;
		break;
	case CPU_SPEED_PERFORMANCE:
		freq = 1512;
		cpu = 2;
		break;
	}

	char cmd[128];
	// Set CPU governor to userspace mode with specified cores and frequency
	sprintf(cmd, "overclock.elf userspace %d %d 384 1080 0", cpu, freq);
	system(cmd);
}

#define RUMBLE_PATH "/sys/devices/virtual/timed_output/vibrator/enable"

/**
 * Activates rumble motor.
 *
 * Controls vibration motor via sysfs timed output interface.
 * When enabled, motor runs for 1000ms (1 second).
 *
 * @param strength Non-zero to enable rumble, 0 to disable
 */
void PLAT_setRumble(int strength) {
	putInt(RUMBLE_PATH, strength ? 1000 : 0); // 1000ms vibration duration
}

/**
 * Selects appropriate audio sample rate.
 *
 * @param requested Requested sample rate
 * @param max Maximum supported sample rate
 * @return Actual sample rate to use (min of requested and max)
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Returns device model name.
 *
 * @return "Miyoo A30"
 */
char* PLAT_getModel(void) {
	return "Miyoo A30";
}

/**
 * Checks if device is online (WiFi connected).
 *
 * @return 1 if online, 0 otherwise
 */
int PLAT_isOnline(void) {
	return online;
}
