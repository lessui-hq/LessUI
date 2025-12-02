/**
 * platform.c - Anbernic RG35XX Plus platform implementation
 *
 * REFACTORED VERSION - Uses shared render_sdl2 backend
 *
 * Supports multiple device variants in the RG35XX+ family:
 * - RG35XX Plus (standard model)
 * - RG CubeXX (variant with overscan support)
 * - RG34XX (variant with different features)
 *
 * Hardware features:
 * - SDL2-based video with HDMI support (via render_sdl2)
 * - Multiple input sources: built-in controls + external gamepads
 * - Lid detection (hall sensor)
 * - Hardware rotation support
 * - Display effects (scanlines, grid)
 * - AXP2202 power management
 *
 * Device detection via RGXX_MODEL environment variable.
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

// Paths for HDMI detection and display blanking
#define HDMI_STATE_PATH "/sys/class/switch/hdmi/cable.0/state"
#define BLANK_PATH "/sys/class/graphics/fb0/blank"

///////////////////////////////
// Device Registry and Variant Configuration
///////////////////////////////

// Device registry - all known devices that work with this platform
static const DeviceInfo rg35xxplus_devices[] = {
    // 640x480 devices (VGA resolution - most devices!)
    {.device_id = "rg28xx", .display_name = "RG28XX", .manufacturer = "Anbernic"},
    {.device_id = "rg35xxplus", .display_name = "RG35XX Plus", .manufacturer = "Anbernic"},
    {.device_id = "rg35xxh", .display_name = "RG35XX H", .manufacturer = "Anbernic"},
    {.device_id = "rg35xxsp", .display_name = "RG35XX SP", .manufacturer = "Anbernic"},
    {.device_id = "rg40xxh", .display_name = "RG40XX H", .manufacturer = "Anbernic"},
    {.device_id = "rg40xxv", .display_name = "RG40XX V", .manufacturer = "Anbernic"},

    // 720x720 square devices
    {.device_id = "rgcubexx", .display_name = "RG CubeXX", .manufacturer = "Anbernic"},

    // 720x480 widescreen devices
    {.device_id = "rg34xx", .display_name = "RG34XX", .manufacturer = "Anbernic"},
    {.device_id = "rg34xxsp", .display_name = "RG34XXSP", .manufacturer = "Anbernic"},

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

static const VariantConfig rg35xxplus_variants[] = {
    {.variant = VARIANT_RG35XX_VGA,
     .screen_width = 640,
     .screen_height = 480,
     .screen_diagonal_default = 3.5f,
     .hw_features = HW_FEATURE_NEON | HW_FEATURE_LID | HW_FEATURE_RUMBLE},
    {.variant = VARIANT_RG35XX_SQUARE,
     .screen_width = 720,
     .screen_height = 720,
     .screen_diagonal_default = 3.95f,
     .hw_features = HW_FEATURE_NEON | HW_FEATURE_LID | HW_FEATURE_RUMBLE},
    {.variant = VARIANT_RG35XX_WIDE,
     .screen_width = 720,
     .screen_height = 480,
     .screen_diagonal_default = 3.4f,
     .hw_features = HW_FEATURE_NEON | HW_FEATURE_LID | HW_FEATURE_RUMBLE},
    {.variant = VARIANT_NONE} // Sentinel
};

// Device-to-variant mapping
typedef struct {
	const char* model_string; // What to look for in RGXX_MODEL
	VariantType variant; // Which variant config to use
	const DeviceInfo* device; // Which device info to use
	float screen_diagonal; // Override if different from variant default (0 = use default)
} DeviceVariantMap;

static const DeviceVariantMap rg35xxplus_device_map[] = {
    // 640x480 devices - VARIANT_RG35XX_VGA (most devices!)
    {"RG28xx", VARIANT_RG35XX_VGA, &rg35xxplus_devices[0], 2.8f},
    {"RG35xxPlus", VARIANT_RG35XX_VGA, &rg35xxplus_devices[1], 3.5f},
    {"RG35xxH", VARIANT_RG35XX_VGA, &rg35xxplus_devices[2], 3.5f},
    {"RG35xxSP", VARIANT_RG35XX_VGA, &rg35xxplus_devices[3], 3.5f},
    {"RG40xxH", VARIANT_RG35XX_VGA, &rg35xxplus_devices[4], 4.0f},
    {"RG40xxV", VARIANT_RG35XX_VGA, &rg35xxplus_devices[5], 4.0f},

    // 720x720 square devices - VARIANT_RG35XX_SQUARE
    {"RGcubexx", VARIANT_RG35XX_SQUARE, &rg35xxplus_devices[6], 3.95f},

    // 720x480 widescreen devices - VARIANT_RG35XX_WIDE
    {"RG34xx", VARIANT_RG35XX_WIDE, &rg35xxplus_devices[7], 3.4f},
    {"RG34xxSP", VARIANT_RG35XX_WIDE, &rg35xxplus_devices[8], 3.4f},

    // Sentinel
    {NULL, VARIANT_NONE, NULL, 0.0f}};

static const VariantConfig* getVariantConfig(VariantType variant) {
	for (int i = 0; rg35xxplus_variants[i].variant != VARIANT_NONE; i++) {
		if (rg35xxplus_variants[i].variant == variant)
			return &rg35xxplus_variants[i];
	}
	return NULL;
}

void PLAT_detectVariant(PlatformVariant* v) {
	v->platform = PLATFORM;
	v->has_hdmi = 1;

	// Read model string from environment
	char* model = getenv("RGXX_MODEL");

	// Look up device in mapping table
	const DeviceVariantMap* map = NULL;
	for (int i = 0; rg35xxplus_device_map[i].model_string != NULL; i++) {
		if (prefixMatch((char*)rg35xxplus_device_map[i].model_string, model)) {
			map = &rg35xxplus_device_map[i];
			break;
		}
	}

	// Fallback to default if not found
	if (!map) {
		LOG_warn("Unknown device model '%s', defaulting to RG35XX Plus\n", model);
		map = &rg35xxplus_device_map[0];
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
	         v->device->display_name,
	         v->variant == VARIANT_RG35XX_SQUARE ? "square"
	         : v->variant == VARIANT_RG35XX_WIDE ? "widescreen"
	                                             : "VGA",
	         v->screen_width, v->screen_height, v->screen_diagonal);
}

///////////////////////////////
// Video - Using shared SDL2 backend
///////////////////////////////

static SDL2_RenderContext vid_ctx;

// rg35xxplus has HDMI support
static SDL2_Config vid_config = {
    .auto_rotate = 1, // Auto-detect portrait displays
    .has_hdmi = 1, // Platform supports HDMI
    .brightness_alpha = 0,
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

void PLAT_setVsync(int vsync) {
	// Vsync handled by SDL_RENDERER_PRESENTVSYNC
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
	// Update HDMI state from settings
	vid_ctx.on_hdmi = GetHDMI();
	SDL2_flip(&vid_ctx, sync);
}

int PLAT_supportsOverscan(void) {
	return VARIANT_IS(VARIANT_RG35XX_SQUARE);
}

///////////////////////////////
// Input - Raw key codes
///////////////////////////////

#define RAW_UP 103
#define RAW_DOWN 108
#define RAW_LEFT 105
#define RAW_RIGHT 106
#define RAW_A 304
#define RAW_B 305
#define RAW_X 307
#define RAW_Y 306
#define RAW_START 311
#define RAW_SELECT 310
#define RAW_MENU 312
#define RAW_L1 308
#define RAW_L2 314
#define RAW_L3 313
#define RAW_R1 309
#define RAW_R2 315
#define RAW_R3 316
#define RAW_PLUS 115
#define RAW_MINUS 114
#define RAW_POWER 116
#define RAW_HATY 17
#define RAW_HATX 16
#define RAW_LSY 3
#define RAW_LSX 2
#define RAW_RSY 5
#define RAW_RSX 4

#define RAW_MENU1 RAW_L3
#define RAW_MENU2 RAW_R3

///////////////////////////////
// Input - External gamepad mappings
///////////////////////////////

// RG P01 (Anbernic official gamepad)
#define RGP01_A 305
#define RGP01_B 304
#define RGP01_X 308
#define RGP01_Y 307
#define RGP01_START 315
#define RGP01_SELECT 314
#define RGP01_MENU 316
#define RGP01_L1 310
#define RGP01_L2 312
#define RGP01_L3 317
#define RGP01_R1 311
#define RGP01_R2 313
#define RGP01_R3 318
#define RGP01_LSY 1
#define RGP01_LSX 0
#define RGP01_RSY 5
#define RGP01_RSX 2
#define RGP01_MENU1 RGP01_L3
#define RGP01_MENU2 RGP01_R3

// Xbox controller (tested with 8BitDo SN30 Pro)
#define XBOX_A 305
#define XBOX_B 304
#define XBOX_X 308
#define XBOX_Y 307
#define XBOX_START 315
#define XBOX_SELECT 314
#define XBOX_MENU 316
#define XBOX_L1 310
#define XBOX_L2 2
#define XBOX_L3 317
#define XBOX_R1 311
#define XBOX_R2 5
#define XBOX_R3 318
#define XBOX_LSY 1
#define XBOX_LSX 0
#define XBOX_RSY 4
#define XBOX_RSX 3
#define XBOX_MENU1 XBOX_L3
#define XBOX_MENU2 XBOX_R3

typedef enum GamepadType {
	kGamepadTypeUnknown,
	kGamepadTypeRGP01,
	kGamepadTypeXbox,
} GamepadType;

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

#define kPadIndex 2
static GamepadType pad_type = kGamepadTypeUnknown;

///////////////////////////////
// Input - Lid detection
///////////////////////////////

#define LID_PATH "/sys/class/power_supply/axp2202-battery/hallkey"

void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}

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
// Input - Gamepad hotplug detection
///////////////////////////////

static void checkForGamepad(void) {
	uint32_t now = SDL_GetTicks();
	static uint32_t last_check = 0;
	if (last_check == 0 || now - last_check > 2000) {
		last_check = now;
		int connected = exists("/dev/input/event3");
		if (inputs[kPadIndex] < 0 && connected) {
			LOG_info("Connecting gamepad: ");
			char pad_name[256];
			getFile("/sys/class/input/event3/device/name", pad_name, 256);
			if (containsString(pad_name, "Anbernic")) {
				LOG_info("P01\n");
				pad_type = kGamepadTypeRGP01;
			} else if (containsString(pad_name, "Microsoft")) {
				LOG_info("Xbox\n");
				pad_type = kGamepadTypeXbox;
			} else {
				LOG_info("Unknown\n");
				pad_type = kGamepadTypeUnknown;
			}
			inputs[kPadIndex] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		} else if (inputs[kPadIndex] >= 0 && !connected) {
			LOG_info("Gamepad disconnected\n");
			close(inputs[kPadIndex]);
			inputs[kPadIndex] = -1;
			pad_type = kGamepadTypeUnknown;
		}
	}
}

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[kPadIndex] = -1;
	checkForGamepad();
}

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

void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i = 0; i < BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick >= pad.repeat_at[i])) {
			pad.just_repeated |= btn;
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	checkForGamepad();

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
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;

			if (type == EV_KEY) {
				if (value > 1)
					continue;

				pressed = value;
				if (i == kPadIndex) {
					if (pad_type == kGamepadTypeRGP01) {
						if (code == RGP01_A) {
							btn = BTN_A;
							id = BTN_ID_A;
						} else if (code == RGP01_B) {
							btn = BTN_B;
							id = BTN_ID_B;
						} else if (code == RGP01_X) {
							btn = BTN_X;
							id = BTN_ID_X;
						} else if (code == RGP01_Y) {
							btn = BTN_Y;
							id = BTN_ID_Y;
						} else if (code == RGP01_START) {
							btn = BTN_START;
							id = BTN_ID_START;
						} else if (code == RGP01_SELECT) {
							btn = BTN_SELECT;
							id = BTN_ID_SELECT;
						} else if (code == RGP01_MENU) {
							btn = BTN_MENU;
							id = BTN_ID_MENU;
						} else if (code == RGP01_MENU1) {
							btn = BTN_MENU;
							id = BTN_ID_MENU;
						} else if (code == RGP01_MENU2) {
							btn = BTN_MENU;
							id = BTN_ID_MENU;
						} else if (code == RGP01_L1) {
							btn = BTN_L1;
							id = BTN_ID_L1;
						} else if (code == RGP01_L2) {
							btn = BTN_L2;
							id = BTN_ID_L2;
						} else if (code == RGP01_L3) {
							btn = BTN_L3;
							id = BTN_ID_L3;
						} else if (code == RGP01_R1) {
							btn = BTN_R1;
							id = BTN_ID_R1;
						} else if (code == RGP01_R2) {
							btn = BTN_R2;
							id = BTN_ID_R2;
						} else if (code == RGP01_R3) {
							btn = BTN_R3;
							id = BTN_ID_R3;
						}
					} else if (pad_type == kGamepadTypeXbox) {
						if (code == XBOX_A) {
							btn = BTN_A;
							id = BTN_ID_A;
						} else if (code == XBOX_B) {
							btn = BTN_B;
							id = BTN_ID_B;
						} else if (code == XBOX_X) {
							btn = BTN_X;
							id = BTN_ID_X;
						} else if (code == XBOX_Y) {
							btn = BTN_Y;
							id = BTN_ID_Y;
						} else if (code == XBOX_START) {
							btn = BTN_START;
							id = BTN_ID_START;
						} else if (code == XBOX_SELECT) {
							btn = BTN_SELECT;
							id = BTN_ID_SELECT;
						} else if (code == XBOX_MENU) {
							btn = BTN_MENU;
							id = BTN_ID_MENU;
						} else if (code == XBOX_MENU1) {
							btn = BTN_MENU;
							id = BTN_ID_MENU;
						} else if (code == XBOX_MENU2) {
							btn = BTN_MENU;
							id = BTN_ID_MENU;
						} else if (code == XBOX_L1) {
							btn = BTN_L1;
							id = BTN_ID_L1;
						} else if (code == XBOX_L3) {
							btn = BTN_L3;
							id = BTN_ID_L3;
						} else if (code == XBOX_R1) {
							btn = BTN_R1;
							id = BTN_ID_R1;
						} else if (code == XBOX_R3) {
							btn = BTN_R3;
							id = BTN_ID_R3;
						}
					}
				} else {
					if (code == RAW_UP) {
						btn = BTN_DPAD_UP;
						id = BTN_ID_DPAD_UP;
					} else if (code == RAW_DOWN) {
						btn = BTN_DPAD_DOWN;
						id = BTN_ID_DPAD_DOWN;
					} else if (code == RAW_LEFT) {
						btn = BTN_DPAD_LEFT;
						id = BTN_ID_DPAD_LEFT;
					} else if (code == RAW_RIGHT) {
						btn = BTN_DPAD_RIGHT;
						id = BTN_ID_DPAD_RIGHT;
					} else if (code == RAW_A) {
						btn = BTN_A;
						id = BTN_ID_A;
					} else if (code == RAW_B) {
						btn = BTN_B;
						id = BTN_ID_B;
					} else if (code == RAW_X) {
						btn = BTN_X;
						id = BTN_ID_X;
					} else if (code == RAW_Y) {
						btn = BTN_Y;
						id = BTN_ID_Y;
					} else if (code == RAW_START) {
						btn = BTN_START;
						id = BTN_ID_START;
					} else if (code == RAW_SELECT) {
						btn = BTN_SELECT;
						id = BTN_ID_SELECT;
					} else if (code == RAW_MENU) {
						btn = BTN_MENU;
						id = BTN_ID_MENU;
					} else if (code == RAW_MENU1) {
						btn = BTN_MENU;
						id = BTN_ID_MENU;
					} else if (code == RAW_MENU2) {
						btn = BTN_MENU;
						id = BTN_ID_MENU;
					} else if (code == RAW_L1) {
						btn = BTN_L1;
						id = BTN_ID_L1;
					} else if (code == RAW_L2) {
						btn = BTN_L2;
						id = BTN_ID_L2;
					} else if (code == RAW_L3) {
						btn = BTN_L3;
						id = BTN_ID_L3;
					} else if (code == RAW_R1) {
						btn = BTN_R1;
						id = BTN_ID_R1;
					} else if (code == RAW_R2) {
						btn = BTN_R2;
						id = BTN_ID_R2;
					} else if (code == RAW_R3) {
						btn = BTN_R3;
						id = BTN_ID_R3;
					} else if (code == RAW_PLUS) {
						btn = BTN_PLUS;
						id = BTN_ID_PLUS;
					} else if (code == RAW_MINUS) {
						btn = BTN_MINUS;
						id = BTN_ID_MINUS;
					} else if (code == RAW_POWER) {
						btn = BTN_POWER;
						id = BTN_ID_POWER;
					}
				}
			} else if (type == EV_ABS) {
				if (code == RAW_HATY || code == RAW_HATX) {
					if (value > 1)
						continue;
					int hats[4] = {-1, -1, -1, -1};
					if (code == RAW_HATY) {
						hats[0] = value == -1;
						hats[1] = value == 1;
					} else if (code == RAW_HATX) {
						hats[2] = value == -1;
						hats[3] = value == 1;
					}

					for (id = 0; id < 4; id++) {
						int state = hats[id];
						btn = 1 << id;
						if (state == 0) {
							pad.is_pressed &= ~btn;
							pad.just_repeated &= ~btn;
							pad.just_released |= btn;
						} else if (state == 1 && (pad.is_pressed & btn) == BTN_NONE) {
							pad.just_pressed |= btn;
							pad.just_repeated |= btn;
							pad.is_pressed |= btn;
							pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
						}
					}
					btn = BTN_NONE;
				} else if (i == kPadIndex) {
					if (pad_type == kGamepadTypeRGP01) {
						if (code == RGP01_LSX) {
							pad.laxis.x = ((value - 128) * 32767) / 128;
							PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x,
							              tick + PAD_REPEAT_DELAY);
						} else if (code == RGP01_LSY) {
							pad.laxis.y = ((value - 128) * 32767) / 128;
							PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y,
							              tick + PAD_REPEAT_DELAY);
						} else if (code == RGP01_RSX)
							pad.raxis.x = ((value - 128) * 32767) / 128;
						else if (code == RGP01_RSY)
							pad.raxis.y = ((value - 128) * 32767) / 128;
					} else if (pad_type == kGamepadTypeXbox) {
						if (code == XBOX_LSX) {
							pad.laxis.x = value;
							PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x,
							              tick + PAD_REPEAT_DELAY);
						} else if (code == XBOX_LSY) {
							pad.laxis.y = value;
							PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y,
							              tick + PAD_REPEAT_DELAY);
						} else if (code == XBOX_RSX)
							pad.raxis.x = value;
						else if (code == XBOX_RSY)
							pad.raxis.y = value;
						else if (code == XBOX_L2) {
							pressed = value > 0;
							btn = BTN_L2;
							id = BTN_ID_L2;
						} else if (code == XBOX_R2) {
							pressed = value > 0;
							btn = BTN_R2;
							id = BTN_ID_R2;
						}
					}
				} else {
					if (code == RAW_LSX) {
						pad.laxis.x = (value * 32767) / 4096;
						PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x,
						              tick + PAD_REPEAT_DELAY);
					} else if (code == RAW_LSY) {
						pad.laxis.y = (value * 32767) / 4096;
						PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y,
						              tick + PAD_REPEAT_DELAY);
					} else if (code == RAW_RSX)
						pad.raxis.x = (value * 32767) / 4096;
					else if (code == RAW_RSY)
						pad.raxis.y = (value * 32767) / 4096;
				}
			}

			if (btn == BTN_NONE)
				continue;

			if (!pressed) {
				pad.is_pressed &= ~btn;
				pad.just_repeated &= ~btn;
				pad.just_released |= btn;
			} else if ((pad.is_pressed & btn) == BTN_NONE) {
				pad.just_pressed |= btn;
				pad.just_repeated |= btn;
				pad.is_pressed |= btn;
				pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
			}
		}
	}

	if (lid.has_lid && PLAT_lidChanged(NULL))
		pad.just_released |= BTN_SLEEP;
}

int PLAT_shouldWake(void) {
	int lid_open = 1;
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open)
		return 1;

	int input;
	static struct input_event event;
	for (int i = 0; i < INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type == EV_KEY && event.code == RAW_POWER && event.value == 0) {
				if (lid.has_lid && !lid.is_open)
					return 0;
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////
// Power Management
///////////////////////////////

static int online = 0;

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

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

	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status, 16);
	online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/power_supply/axp2202-battery/work_led"

void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK);
		SetBrightness(GetBrightness());
		putInt(LED_PATH, 0);
	} else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN);
		SetRawBrightness(0);
		putInt(LED_PATH, 1);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	system("echo 1 > /sys/class/power_supply/axp2202-battery/work_led");
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	exit(0);
}

void PLAT_setCPUSpeed(int speed) {
	// Not implemented
}

#define RUMBLE_PATH "/sys/class/power_supply/axp2202-battery/moto"

void PLAT_setRumble(int strength) {
	if (GetHDMI())
		return;
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
