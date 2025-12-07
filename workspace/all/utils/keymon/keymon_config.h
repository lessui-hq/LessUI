/**
 * keymon_config.h - Platform-specific keymon configuration
 *
 * Each platform defines these macros in their platform.h to configure keymon behavior.
 * This allows a single unified keymon.c to work across all platforms.
 */

#ifndef KEYMON_CONFIG_H
#define KEYMON_CONFIG_H

/*
 * Required platform configuration (must be defined in platform.h):
 *
 * KEYMON_BUTTON_MENU      - Main menu button code (e.g., 312, 1, 317, or -1 if not used)
 * KEYMON_BUTTON_MENU_ALT  - Alternative menu button (e.g., 318 for rgb30, or -1 if not used)
 * KEYMON_BUTTON_MENU_ALT2 - Third menu button (e.g., 316 for tg5040, optional)
 * KEYMON_BUTTON_PLUS      - Volume/brightness up button (e.g., 115, 114)
 * KEYMON_BUTTON_MINUS     - Volume/brightness down button (e.g., 114, 115)
 *
 * KEYMON_USE_SELECT_START - 1 if platform uses SELECT/START instead of MENU (trimuismart, m17)
 * KEYMON_BUTTON_SELECT    - SELECT button code (if USE_SELECT_START=1)
 * KEYMON_BUTTON_START     - START button code (if USE_SELECT_START=1)
 * KEYMON_BUTTON_L1        - L1 shoulder button (if USE_SELECT_START=1)
 * KEYMON_BUTTON_R1        - R1 shoulder button (if USE_SELECT_START=1)
 *
 * Optional features (define to 1 to enable, 0 to disable):
 *
 * KEYMON_HAS_HDMI         - Platform has HDMI output to monitor
 * KEYMON_HDMI_STATE_PATH  - Path to HDMI state sysfs file
 * KEYMON_HDMI_USE_STRING  - HDMI uses string comparison ("connected\n") instead of int
 *
 * KEYMON_HAS_JACK         - Platform has headphone jack to monitor (via sysfs polling)
 * KEYMON_JACK_STATE_PATH  - Path to headphone jack state sysfs file
 * KEYMON_JACK_INVERTED    - Jack GPIO value is inverted (0 = plugged, 1 = unplugged)
 * KEYMON_HAS_JACK_SWITCH  - Jack detection via EV_SW events (tg5040, zero28)
 *
 * KEYMON_HAS_MUTE         - Platform has hardware mute switch (tg5040)
 * KEYMON_MUTE_STATE_PATH  - Path to mute switch GPIO
 *
 * KEYMON_HAS_VOLUME_QUIRK - Platform needs hardware volume button quirk (m17)
 *                           Re-applies volume when PLUS/MINUS pressed (SELECT_START mode)
 *
 * KEYMON_INPUT_COUNT      - Number of input devices to poll (default: 1)
 * KEYMON_INPUT_DEVICE     - Input device path for single device (default: "/dev/input/event0")
 * KEYMON_INPUT_DEVICE_0..5- Custom device paths when KEYMON_INPUT_COUNT > 1
 *                           (e.g., rgb30 uses event0-4 + js0)
 *
 * KEYMON_VOLUME_MIN       - Minimum volume (default: 0)
 * KEYMON_VOLUME_MAX       - Maximum volume (default: 20)
 * KEYMON_BRIGHTNESS_MIN   - Minimum brightness (default: 0)
 * KEYMON_BRIGHTNESS_MAX   - Maximum brightness (default: 10)
 */

// Defaults for optional configuration
#ifndef KEYMON_HAS_HDMI
#define KEYMON_HAS_HDMI 0
#endif

#ifndef KEYMON_HAS_JACK
#define KEYMON_HAS_JACK 0
#endif

#ifndef KEYMON_INPUT_COUNT
#define KEYMON_INPUT_COUNT 1
#endif

#ifndef KEYMON_INPUT_DEVICE
#define KEYMON_INPUT_DEVICE "/dev/input/event0"
#endif

#ifndef KEYMON_VOLUME_MIN
#define KEYMON_VOLUME_MIN 0
#endif

#ifndef KEYMON_VOLUME_MAX
#define KEYMON_VOLUME_MAX 20
#endif

#ifndef KEYMON_BRIGHTNESS_MIN
#define KEYMON_BRIGHTNESS_MIN 0
#endif

#ifndef KEYMON_BRIGHTNESS_MAX
#define KEYMON_BRIGHTNESS_MAX 10
#endif

#ifndef KEYMON_USE_SELECT_START
#define KEYMON_USE_SELECT_START 0
#endif

#endif // KEYMON_CONFIG_H
