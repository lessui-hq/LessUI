/**
 * platform.c - Retroid Pocket SM8250 platform implementation
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Supports multiple device variants in the Retroid Pocket family:
 * - Retroid Pocket 5 (1920x1080)
 * - Retroid Pocket Flip 2 (1920x1080)
 * - Retroid Pocket Mini V2 (1240x1080)
 *
 * Hardware features:
 * - SDL2-based video with HDMI support (via render_sdl2)
 * - Retroid Pocket Gamepad via serdev kernel driver
 * - Haptic feedback via pmi8998_haptics
 * - Display effects (scanlines, grid)
 *
 * Device detection via LESSOS_DEVICE environment variable.
 *
 * LessOS/ROCKNIX upstream references (in LessOS repository):
 * - Retroid gamepad kernel driver: devices/SM8250/patches/linux/0008-retroid-gamepad.patch
 * - Input discovery pattern: packages/sysutils/system-utils/sources/scripts/input_sense
 * - Button mappings: packages/emulators/libretro/retroarch/retroarch-joypads/gamepads/Retroid Pocket Gamepad.cfg
 * - UDEV rules: devices/SM8250/filesystem/usr/lib/udev/rules.d/99-retroid-pocket.rules
 */

#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Paths for HDMI detection
#define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

///////////////////////////////
// Device Registry and Variant Configuration
///////////////////////////////

// Device registry - all known devices that work with this platform
static const DeviceInfo retroid_devices[] = {
    // 1920x1080 FHD devices
    {.device_id = "pocket5", .display_name = "Pocket 5", .manufacturer = "Retroid"},
    {.device_id = "flip2", .display_name = "Pocket Flip 2", .manufacturer = "Retroid"},

    // 1240x1080 Mini V2
    {.device_id = "miniv2", .display_name = "Pocket Mini V2", .manufacturer = "Retroid"},

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

static const VariantConfig retroid_variants[] = {
    {.variant = VARIANT_RETROID_FHD,
     .screen_width = 1920,
     .screen_height = 1080,
     .screen_diagonal_default = 5.5f,
     .hw_features = HW_FEATURE_NEON | HW_FEATURE_ANALOG | HW_FEATURE_RUMBLE},
    {.variant = VARIANT_RETROID_MINI_V2,
     .screen_width = 1240,
     .screen_height = 1080,
     .screen_diagonal_default = 3.92f,
     .hw_features = HW_FEATURE_NEON | HW_FEATURE_ANALOG | HW_FEATURE_RUMBLE},
    {.variant = VARIANT_NONE} // Sentinel
};

// Device-to-variant mapping
typedef struct {
	const char* device_string; // What to look for in LESSOS_DEVICE
	VariantType variant; // Which variant config to use
	const DeviceInfo* device; // Which device info to use
	float screen_diagonal; // Override if different from variant default (0 = use default)
} DeviceVariantMap;

static const DeviceVariantMap retroid_device_map[] = {
    // 1920x1080 FHD devices - VARIANT_RETROID_FHD
    {"Retroid Pocket 5", VARIANT_RETROID_FHD, &retroid_devices[0], 5.5f},
    {"Pocket 5", VARIANT_RETROID_FHD, &retroid_devices[0], 5.5f},
    {"RP5", VARIANT_RETROID_FHD, &retroid_devices[0], 5.5f},

    {"Retroid Pocket Flip 2", VARIANT_RETROID_FHD, &retroid_devices[1], 5.5f},
    {"Pocket Flip 2", VARIANT_RETROID_FHD, &retroid_devices[1], 5.5f},
    {"Flip 2", VARIANT_RETROID_FHD, &retroid_devices[1], 5.5f},
    {"RPF2", VARIANT_RETROID_FHD, &retroid_devices[1], 5.5f},

    // 1240x1080 Mini V2 - VARIANT_RETROID_MINI_V2
    {"Retroid Pocket Mini V2", VARIANT_RETROID_MINI_V2, &retroid_devices[2], 3.92f},
    {"Pocket Mini V2", VARIANT_RETROID_MINI_V2, &retroid_devices[2], 3.92f},
    {"Mini V2", VARIANT_RETROID_MINI_V2, &retroid_devices[2], 3.92f},
    {"RPMV2", VARIANT_RETROID_MINI_V2, &retroid_devices[2], 3.92f},

    // Sentinel
    {NULL, VARIANT_NONE, NULL, 0.0f}};

static const VariantConfig* getVariantConfig(VariantType variant) {
	for (int i = 0; retroid_variants[i].variant != VARIANT_NONE; i++) {
		if (retroid_variants[i].variant == variant)
			return &retroid_variants[i];
	}
	return NULL;
}

void PLAT_detectVariant(PlatformVariant* v) {
	v->platform = PLATFORM;
	v->has_hdmi = 1;

	// Read device from LessOS environment variable
	char* device = getenv("LESSOS_DEVICE");
	if (!device) {
		LOG_debug("LESSOS_DEVICE not set, defaulting to Pocket 5\n");
		device = "Retroid Pocket 5"; // Fallback to default
	} else {
		LOG_debug("LESSOS_DEVICE=%s\n", device);
	}

	// Look up device in mapping table (substring match)
	const DeviceVariantMap* map = NULL;
	for (int i = 0; retroid_device_map[i].device_string != NULL; i++) {
		if (containsString((char*)device, (char*)retroid_device_map[i].device_string)) {
			LOG_debug("Matched device: %s (table entry: %s)\n", device,
			          retroid_device_map[i].device_string);
			map = &retroid_device_map[i];
			break;
		}
	}

	// Fallback to default if not found
	if (!map) {
		LOG_warn("Unknown device '%s', defaulting to Pocket 5\n", device);
		map = &retroid_device_map[0];
	}

	// Set device info
	v->device = map->device;
	v->variant = map->variant;

	// Apply variant configuration
	const VariantConfig* config = getVariantConfig(map->variant);
	if (config) {
		v->screen_width = config->screen_width;
		v->screen_height = config->screen_height;
		v->screen_diagonal =
		    map->screen_diagonal > 0 ? map->screen_diagonal : config->screen_diagonal_default;
		v->hw_features = config->hw_features;
	}

	// Check for HDMI connection (runtime override)
	v->hdmi_active = getInt(HDMI_STATE_PATH);
	if (v->hdmi_active) {
		v->screen_width = HDMI_WIDTH;
		v->screen_height = HDMI_HEIGHT;
	}

	LOG_info("Detected device: %s %s (%s variant, %dx%d, %.1f\")\n", v->device->manufacturer,
	         v->device->display_name, v->variant == VARIANT_RETROID_MINI_V2 ? "Mini V2" : "FHD",
	         v->screen_width, v->screen_height, v->screen_diagonal);
}

///////////////////////////////
// Video - Using shared SDL2 backend
///////////////////////////////

static SDL2_RenderContext vid_ctx;

static SDL2_Config vid_config = {
    // No rotation needed (landscape display)
    .auto_rotate = 0,
    .rotate_cw = 0,
    .rotate_null_center = 0,
    // Display features
    .has_hdmi = 1,
    .default_sharpness = SHARPNESS_SOFT,
};

SDL_Surface* PLAT_initVideo(void) {
	// Detect device variant
	PLAT_detectVariant(&platform_variant);

	// Use detected resolution (may be overridden by HDMI)
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	vid_ctx.on_hdmi = platform_variant.hdmi_active;

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
	// Not supported
}

void PLAT_setNearestNeighbor(int enabled) {
	// Not supported
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
	vid_ctx.on_hdmi = GetHDMI();
	SDL2_present(&vid_ctx, renderer);
}

SDL_Window* PLAT_getWindow(void) {
	return SDL2_getWindow(&vid_ctx);
}

int PLAT_getRotation(void) {
	return SDL2_getRotation(&vid_ctx);
}

int PLAT_supportsOverscan(void) {
	// Only Mini V2 with near-square aspect ratio benefits from overscan
	return VARIANT_IS(VARIANT_RETROID_MINI_V2);
}

///////////////////////////////
// Input - Raw key codes from Retroid Pocket Gamepad driver
///////////////////////////////

// Button codes from retroid.c keymap[] (standard Linux BTN_* codes)
#define RAW_UP 544 // BTN_DPAD_UP
#define RAW_DOWN 545 // BTN_DPAD_DOWN
#define RAW_LEFT 546 // BTN_DPAD_LEFT
#define RAW_RIGHT 547 // BTN_DPAD_RIGHT
#define RAW_A 304 // BTN_SOUTH
#define RAW_B 305 // BTN_EAST
#define RAW_X 308 // BTN_WEST
#define RAW_Y 307 // BTN_NORTH
#define RAW_START 315 // BTN_START
#define RAW_SELECT 314 // BTN_SELECT
#define RAW_MENU 316 // BTN_MODE (Home button)
#define RAW_L1 310 // BTN_TL
#define RAW_R1 311 // BTN_TR
#define RAW_L3 317 // BTN_THUMBL
#define RAW_R3 318 // BTN_THUMBR
#define RAW_BACK 158 // BTN_BACK
#define RAW_PLUS 115 // KEY_VOLUMEUP
#define RAW_MINUS 114 // KEY_VOLUMEDOWN
#define RAW_POWER 116 // KEY_POWER

// Analog axes
#define RAW_LSX 0 // ABS_X
#define RAW_LSY 1 // ABS_Y
#define RAW_RSX 3 // ABS_RX
#define RAW_RSY 4 // ABS_RY
#define RAW_L2 16 // ABS_HAT2X (analog trigger)
#define RAW_R2 17 // ABS_HAT2Y (analog trigger)

// Treat Home, L3+R3 as menu buttons
#define RAW_MENU1 RAW_L3
#define RAW_MENU2 RAW_R3

#define INPUT_COUNT 2
static int inputs[INPUT_COUNT];

/**
 * Find input device by name in sysfs.
 * Returns device path (e.g., "/dev/input/event2") or NULL if not found.
 */
static char* findInputDeviceByName(const char* device_name) {
	static char device_path[64];
	char name_path[128];
	char name[256];

	// Search /sys/class/input/event* for matching device name
	for (int i = 0; i < 10; i++) {
		snprintf(name_path, sizeof(name_path), "/sys/class/input/event%d/device/name", i);
		FILE* f = fopen(name_path, "r");
		if (!f)
			continue;

		if (fgets(name, sizeof(name), f)) {
			// Strip newline
			name[strcspn(name, "\n")] = 0;
			if (strcmp(name, device_name) == 0) {
				snprintf(device_path, sizeof(device_path), "/dev/input/event%d", i);
				fclose(f);
				LOG_debug("Found '%s' at %s\n", device_name, device_path);
				return device_path;
			}
		}
		fclose(f);
	}

	LOG_warn("Device '%s' not found in /sys/class/input/\n", device_name);
	return NULL;
}

void PLAT_initInput(void) {
	// Find Retroid Pocket Gamepad dynamically by name
	char* gamepad_path = findInputDeviceByName("Retroid Pocket Gamepad");
	if (gamepad_path) {
		inputs[0] = open(gamepad_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (inputs[0] < 0)
			LOG_warn("Failed to open gamepad at %s\n", gamepad_path);
	} else {
		// Fallback: try event0 (shouldn't happen on Retroid)
		inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (inputs[0] < 0)
			LOG_warn("Failed to open fallback /dev/input/event0\n");
	}

	// Power button and other system inputs (volume, power)
	// Try event1 for power/volume buttons
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (inputs[1] < 0)
		LOG_debug("No secondary input device at event1 (power/volume buttons may not work)\n");
}

void PLAT_quitInput(void) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		if (inputs[i] >= 0)
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

void PLAT_pollInput(void) {
	uint32_t tick = SDL_GetTicks();
	PAD_beginPolling();
	PAD_handleRepeat(tick);

	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		if (input < 0)
			continue;
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
				} else if (code == RAW_R1) {
					btn = BTN_R1;
				} else if (code == RAW_L3) {
					btn = BTN_L3;
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
				// Analog sticks - Retroid uses ~-1408 to +1408 range
				if (code == RAW_LSX) {
					pad.laxis.x = (value * 32767) / 1408;
					PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x,
					              tick + PAD_REPEAT_DELAY);
				} else if (code == RAW_LSY) {
					pad.laxis.y = (value * 32767) / 1408;
					PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y,
					              tick + PAD_REPEAT_DELAY);
				} else if (code == RAW_RSX) {
					pad.raxis.x = (value * 32767) / 1408;
				} else if (code == RAW_RSY) {
					pad.raxis.y = (value * 32767) / 1408;
				} else if (code == RAW_L2) {
					// Analog L2 trigger (0 to ~1552)
					pressed = value > 100; // Threshold for digital press
					btn = BTN_L2;
				} else if (code == RAW_R2) {
					// Analog R2 trigger (0 to ~1552)
					pressed = value > 100;
					btn = BTN_R2;
				}
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
		if (input < 0)
			continue;
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
	// SM8250 uses standard power_supply class
	*is_charging = getInt("/sys/class/power_supply/battery/status") == 2; // Charging status

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

#define BRIGHTNESS_PATH "/sys/class/backlight/panel0-backlight/brightness"
#define BLANK_PATH "/sys/class/graphics/fb0/blank"

void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK);
		SetBrightness(GetBrightness());
	} else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN);
		SetRawBrightness(0);
	}
}

void PLAT_powerOff(void) {
	sleep(2);
	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
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

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"

/**
 * Sets CPU frequency based on performance mode.
 *
 * Snapdragon 865 frequencies (Cortex-A77 prime core):
 * Up to 2.84 GHz (prime), 2.4 GHz (performance), 1.8 GHz (efficiency)
 *
 * @param speed CPU_SPEED_* constant
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
	case CPU_SPEED_IDLE:
		freq = 300000; // 300 MHz (idle)
		break;
	case CPU_SPEED_POWERSAVE:
		freq = 1056000; // ~1 GHz (powersave)
		break;
	case CPU_SPEED_NORMAL:
		freq = 1804800; // ~1.8 GHz (normal)
		break;
	case CPU_SPEED_PERFORMANCE:
		freq = 2419200; // ~2.4 GHz (performance)
		break;
	}
	putInt(GOVERNOR_PATH, freq);
}

/**
 * Gets available CPU frequencies from sysfs.
 */
int PLAT_getAvailableCPUFrequencies(int* frequencies, int max_count) {
	return PWR_getAvailableCPUFrequencies_sysfs(frequencies, max_count);
}

/**
 * Sets CPU frequency directly via sysfs.
 */
int PLAT_setCPUFrequency(int freq_khz) {
	return PWR_setCPUFrequency_sysfs(freq_khz);
}

#define RUMBLE_PATH "/sys/class/leds/vibrator/activate"

void PLAT_setRumble(int strength) {
	if (GetHDMI())
		return;
	// pmi8998_haptics uses /sys/class/leds/vibrator interface
	putInt(RUMBLE_PATH, strength ? 1 : 0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return (char*)PLAT_getDeviceName();
}

int PLAT_isOnline(void) {
	return online;
}
