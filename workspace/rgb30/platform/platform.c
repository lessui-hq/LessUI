/**
 * platform.c - Powkiddy RGB30 platform implementation
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Platform-specific code for the Powkiddy RGB30 handheld device.
 * Key features:
 * - Dual analog sticks with swapped right stick axes (X/Y reversed)
 * - WiFi support with status monitoring
 * - Grid and line visual effects for retro aesthetics
 * - Rotation support for display output
 * - Dynamic device model detection from device tree
 * - Overscan support (PLAT_supportsOverscan returns 1)
 *
 * The RGB30 uses the Rockchip RK3566 SoC with 720x720 display.
 * Input events are read directly from /dev/input/event* devices.
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
// Video - Using shared SDL2 backend
///////////////////////////////

static SDL2_RenderContext vid_ctx;

static const SDL2_Config vid_config = {
    // No rotation needed (square 720x720 display)
    .auto_rotate = 0,
    .rotate_cw = 0,
    .rotate_null_center = 0,
    // Display features
    .has_hdmi = 1,
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

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {}
void PLAT_setNearestNeighbor(int enabled) {}

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

void PLAT_clearBlit(void) {
	SDL2_clearBlit(&vid_ctx);
}

void PLAT_flip(SDL_Surface* screen, int sync) {
	SDL2_flip(&vid_ctx, sync);
}

int PLAT_supportsOverscan(void) {
	return 1;
}

///////////////////////////////
// Input Handling
///////////////////////////////

#define RAW_UP 544
#define RAW_DOWN 545
#define RAW_LEFT 546
#define RAW_RIGHT 547
#define RAW_A 305
#define RAW_B 304
#define RAW_X 307
#define RAW_Y 308
#define RAW_START 315
#define RAW_SELECT 314
#define RAW_MENU 139
#define RAW_L1 310
#define RAW_L2 312
#define RAW_L3 317
#define RAW_R1 311
#define RAW_R2 313
#define RAW_R3 318
#define RAW_PLUS 115
#define RAW_MINUS 114
#define RAW_POWER 116
#define RAW_HATY 17
#define RAW_HATX 16
#define RAW_LSY 1
#define RAW_LSX 0
#define RAW_RSY 3
#define RAW_RSX 4

#define RAW_MENU1 RAW_L3
#define RAW_MENU2 RAW_R3

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[2] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[3] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	for (int i = 0; i < INPUT_COUNT; i++) {
		if (inputs[i] < 0)
			LOG_warn("Failed to open /dev/input/event%d\n", i);
	}
}

void PLAT_quitInput(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY 0x01
#define EV_ABS 0x03

void PLAT_pollInput(void) {
	uint32_t tick = SDL_GetTicks();
	PAD_beginPolling();
	PAD_handleRepeat(tick);

	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type != EV_KEY && event.type != EV_ABS)
				continue;

			int btn = BTN_NONE;
			int pressed = 0;
			int type = event.type;
			int code = event.code;
			int value = event.value;

			if (type == EV_KEY) {
				if (value > 1)
					continue;
				pressed = value;
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
				if (code == RAW_LSX) {
					pad.laxis.x = (value * 32767) / 1800;
					PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x,
					              tick + PAD_REPEAT_DELAY);
				} else if (code == RAW_LSY) {
					pad.laxis.y = (value * 32767) / 1800;
					PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y,
					              tick + PAD_REPEAT_DELAY);
					// Right stick axes are swapped in hardware
				} else if (code == RAW_RSX) {
					pad.raxis.y = (value * 32767) / 1800;
				} else if (code == RAW_RSY) {
					pad.raxis.x = (value * 32767) / 1800;
				}
				btn = BTN_NONE;
			}

			PAD_updateButton(btn, pressed, tick);
		}
	}
}

int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type == EV_KEY && event.code == RAW_POWER && event.value == 0)
				return 1;
		}
	}
	return 0;
}

///////////////////////////////
// Power Management
///////////////////////////////

static int online = 0;

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/ac/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
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

	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status, 16);
	online = prefixMatch("up", status);
}

void PLAT_enableBacklight(int enable) {
	putInt("/sys/class/backlight/backlight/bl_power",
	       enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
}

void PLAT_powerOff(void) {
	sleep(2);
	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
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

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"

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

void PLAT_setRumble(int strength) {}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

static char model[256];
char* PLAT_getModel(void) {
	char buffer[256];
	getFile("/proc/device-tree/model", buffer, 256);
	char* tmp = strrchr(buffer, ' ');
	if (tmp) {
		strncpy(model, tmp + 1, sizeof(model) - 1);
		model[sizeof(model) - 1] = '\0';
	} else {
		strncpy(model, "RGB30", sizeof(model) - 1);
		model[sizeof(model) - 1] = '\0';
	}
	return model;
}

int PLAT_isOnline(void) {
	return online;
}
