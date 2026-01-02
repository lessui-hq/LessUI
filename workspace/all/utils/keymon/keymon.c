/**
 * keymon.c - Unified hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles system-level
 * shortcuts across all supported handheld devices.
 *
 * Features (platform-dependent, configured via keymon_config.h):
 * - Volume and brightness control through button combinations
 * - HDMI output detection and routing
 * - Headphone jack detection and routing
 * - Multiple input device support
 *
 * Button combinations (most platforms):
 * - MENU+PLUS/MINUS: Adjust brightness
 * - PLUS/MINUS alone: Adjust volume
 *
 * Alternative (trimuismart):
 * - START+R1/L1: Adjust brightness
 * - SELECT+R1/L1: Adjust volume
 *
 * Runs continuously at 60Hz polling input devices for button events.
 * Implements repeat functionality (initial 300ms delay, then 100ms interval).
 */

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "platform.h"
#include "keymon_config.h"
#include "log.h"
#include <msettings.h>

#if KEYMON_HAS_HDMI || KEYMON_HAS_JACK || defined(KEYMON_HAS_MUTE)
#include <pthread.h>
#endif

#ifdef KEYMON_USE_LIBUDEV
#include "udev_input.h"
#endif

// Input event values from linux/input.h
#define RELEASED 0
#define PRESSED 1
#define REPEAT 2

// Shutdown flag for clean exit
static volatile sig_atomic_t running = 1;

/**
 * Signal handler for clean shutdown.
 */
static void handle_signal(int sig) {
	(void)sig;
	running = 0;
}

// Input device management
#ifdef KEYMON_USE_LIBUDEV
static int inputs[UDEV_MAX_DEVICES];
static int input_count = 0;
#elif KEYMON_INPUT_COUNT > 1
static int inputs[KEYMON_INPUT_COUNT];
#else
static int input_fd = 0;
#endif

static struct input_event ev;

#if KEYMON_HAS_HDMI || KEYMON_HAS_JACK
static pthread_t ports_pt;
#endif

#ifdef KEYMON_HAS_MUTE
static pthread_t mute_pt;
#endif

/**
 * Reads an integer value from a sysfs file.
 *
 * Used for reading hardware state from kernel interfaces.
 *
 * @param path Path to sysfs file
 * @return Integer value read from file, or 0 if file cannot be opened
 */
static int getInt(char* path) {
	int i = 0;
	FILE* file = fopen(path, "r");
	if (file != NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

#if KEYMON_HAS_HDMI && defined(KEYMON_HDMI_USE_STRING)
/**
 * Reads a text file into a buffer.
 *
 * Used for HDMI status detection on some platforms.
 *
 * @param path Path to file
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
static void getFile(char* path, char* buffer, size_t buffer_size) {
	buffer[0] = '\0'; // Initialize in case fopen fails
	FILE* file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		if (size > buffer_size - 1)
			size = buffer_size - 1;
		rewind(file);
		fread(buffer, sizeof(char), size, file);
		fclose(file);
		buffer[size] = '\0';
	}
}

/**
 * Checks if two strings are exactly equal.
 *
 * @param str1 First string
 * @param str2 Second string
 * @return 1 if strings match, 0 otherwise
 */
static int exactMatch(char* str1, char* str2) {
	size_t len1 = strlen(str1);
	if (len1 != strlen(str2))
		return 0;
	return (strncmp(str1, str2, len1) == 0);
}
#endif

#if KEYMON_HAS_JACK
/**
 * Checks if headphones are plugged in.
 *
 * Logic depends on platform (some use inverted GPIO values).
 *
 * @return 1 if headphones connected, 0 otherwise
 */
static int JACK_enabled(void) {
#ifdef KEYMON_JACK_INVERTED
	return !getInt(KEYMON_JACK_STATE_PATH);
#else
	return getInt(KEYMON_JACK_STATE_PATH);
#endif
}
#endif

#if KEYMON_HAS_HDMI
/**
 * Checks if HDMI is connected.
 *
 * Implementation varies by platform:
 * - extcon interface: Read integer value
 * - DRM connector: Read "connected\n" string
 *
 * @return 1 if HDMI connected, 0 otherwise
 */
static int HDMI_enabled(void) {
#ifdef KEYMON_HDMI_USE_STRING
	char value[64];
	getFile(KEYMON_HDMI_STATE_PATH, value, 64);
	return exactMatch(value, "connected\n");
#else
	return getInt(KEYMON_HDMI_STATE_PATH);
#endif
}
#endif

#if KEYMON_HAS_HDMI && KEYMON_HAS_JACK
/**
 * Background thread that monitors headphone jack and HDMI state.
 *
 * Polls the hardware interfaces every second and updates audio/video
 * routing when states change.
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* watchPorts(void* arg) {
	int has_jack, had_jack;
	int has_hdmi, had_hdmi;

	// Initialize state
	has_jack = had_jack = JACK_enabled();
	has_hdmi = had_hdmi = HDMI_enabled();
	SetJack(has_jack);
	SetHDMI(has_hdmi);

	while (1) {
		sleep(1);

		// Check for headphone jack state changes
		has_jack = JACK_enabled();
		if (had_jack != has_jack) {
			had_jack = has_jack;
			SetJack(has_jack);
		}

		// Check for HDMI state changes
		has_hdmi = HDMI_enabled();
		if (had_hdmi != has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}

	return 0;
}
#elif KEYMON_HAS_HDMI
/**
 * Background thread that monitors HDMI state only.
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* watchHDMI(void* arg) {
	int has_hdmi, had_hdmi;

	// Initialize HDMI state
	has_hdmi = had_hdmi = HDMI_enabled();
	SetHDMI(has_hdmi);

	while (1) {
		sleep(1);

		// Check for HDMI state changes
		has_hdmi = HDMI_enabled();
		if (had_hdmi != has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}

	return 0;
}
#elif KEYMON_HAS_JACK
/**
 * Background thread that monitors headphone jack state only.
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* watchJack(void* arg) {
	int has_jack, had_jack;

	// Initialize jack state
	has_jack = had_jack = JACK_enabled();
	SetJack(has_jack);

	while (1) {
		sleep(1);

		// Check for jack state changes
		has_jack = JACK_enabled();
		if (had_jack != has_jack) {
			had_jack = has_jack;
			SetJack(has_jack);
		}
	}

	return 0;
}
#endif

#ifdef KEYMON_HAS_MUTE
/**
 * Background thread that monitors mute switch state (tg5040).
 *
 * Polls the GPIO mute switch every second and updates audio routing.
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* watchMute(void* arg) {
	int is_muted, was_muted;

	// Initialize mute state
	is_muted = was_muted = getInt(KEYMON_MUTE_STATE_PATH);
	SetMute(is_muted);

	while (1) {
		sleep(1);

		// Check for mute state changes
		is_muted = getInt(KEYMON_MUTE_STATE_PATH);
		if (was_muted != is_muted) {
			was_muted = is_muted;
			SetMute(is_muted);
		}
	}

	return 0;
}
#endif

/**
 * Main event loop for hardware button monitoring.
 *
 * Continuously polls input device(s) for button events and handles:
 * - Volume control (PLUS/MINUS buttons)
 * - Brightness control (MENU+PLUS/MINUS or START+R1/L1)
 *
 * Implements repeat functionality (initial 300ms delay, then 100ms interval)
 * and ignores stale input after system sleep (> 1 second gap).
 *
 * @param argc Argument count (unused)
 * @param argv Argument values (unused)
 * @return 0 on clean shutdown (SIGTERM/SIGINT)
 */
int main(int argc, char* argv[]) {
	// Initialize logging (reads LOG_FILE environment variable)
	log_open(NULL);

	// Register signal handlers for clean shutdown
	signal(SIGTERM, handle_signal);
	signal(SIGINT, handle_signal);

	InitSettings();

	// Open input device(s)
#ifdef KEYMON_USE_LIBUDEV
	// Use libudev for dynamic input device discovery
	input_count = udev_open_all_inputs(inputs);
	if (input_count == 0) {
		LOG_warn("No input devices found via udev\n");
	} else {
		LOG_info("Opened %d input devices via udev\n", input_count);
	}
#elif KEYMON_INPUT_COUNT > 1
	// Custom device paths (if platform defines them)
#if defined(KEYMON_INPUT_DEVICE_0)
	inputs[0] = open(KEYMON_INPUT_DEVICE_0, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#if KEYMON_INPUT_COUNT > 1 && defined(KEYMON_INPUT_DEVICE_1)
	inputs[1] = open(KEYMON_INPUT_DEVICE_1, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
#if KEYMON_INPUT_COUNT > 2 && defined(KEYMON_INPUT_DEVICE_2)
	inputs[2] = open(KEYMON_INPUT_DEVICE_2, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
#if KEYMON_INPUT_COUNT > 3 && defined(KEYMON_INPUT_DEVICE_3)
	inputs[3] = open(KEYMON_INPUT_DEVICE_3, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
#if KEYMON_INPUT_COUNT > 4 && defined(KEYMON_INPUT_DEVICE_4)
	inputs[4] = open(KEYMON_INPUT_DEVICE_4, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
#if KEYMON_INPUT_COUNT > 5 && defined(KEYMON_INPUT_DEVICE_5)
	inputs[5] = open(KEYMON_INPUT_DEVICE_5, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
#if KEYMON_INPUT_COUNT > 6 && defined(KEYMON_INPUT_DEVICE_6)
	inputs[6] = open(KEYMON_INPUT_DEVICE_6, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
#if KEYMON_INPUT_COUNT > 7 && defined(KEYMON_INPUT_DEVICE_7)
	inputs[7] = open(KEYMON_INPUT_DEVICE_7, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif
#else
	// Sequential device paths (default)
	for (int i = 0; i < KEYMON_INPUT_COUNT; i++) {
		char path[32];
		snprintf(path, sizeof(path), "/dev/input/event%d", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
#endif
#else
	input_fd = open(KEYMON_INPUT_DEVICE, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
#endif

	// Start hardware monitoring threads if enabled
#if KEYMON_HAS_HDMI && KEYMON_HAS_JACK
	pthread_create(&ports_pt, NULL, watchPorts, NULL);
#elif KEYMON_HAS_HDMI
	pthread_create(&ports_pt, NULL, watchHDMI, NULL);
#elif KEYMON_HAS_JACK
	pthread_create(&ports_pt, NULL, watchJack, NULL);
#endif

#ifdef KEYMON_HAS_MUTE
	pthread_create(&mute_pt, NULL, watchMute, NULL);
#endif

	uint32_t pressed; // Button state: PRESSED (1) or RELEASED (0)

	// Button state tracking
#if KEYMON_USE_SELECT_START
	uint32_t start_pressed = 0;
	uint32_t select_pressed = 0;
#else
	uint32_t menu_pressed = 0;
#endif

	// Track PLUS button state for repeat handling
	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;

	// Track MINUS button state for repeat handling
	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;

	uint32_t now;
	uint32_t then = 0;
	uint8_t ignore_stale = 0;
	struct timeval tod;

	gettimeofday(&tod, NULL);
	then = tod.tv_sec * 1000 + tod.tv_usec / 1000;

	while (running) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;

		// Detect stale input after system sleep (> 1 second gap)
		if (now - then > 1000)
			ignore_stale = 1;

		// Read and process all available input events
#ifdef KEYMON_USE_LIBUDEV
		for (int i = 0; i < input_count; i++) {
			while (read(inputs[i], &ev, sizeof(ev)) == sizeof(ev)) {
#elif KEYMON_INPUT_COUNT > 1
		for (int i = 0; i < KEYMON_INPUT_COUNT; i++) {
			while (read(inputs[i], &ev, sizeof(ev)) == sizeof(ev)) {
#else
		while (read(input_fd, &ev, sizeof(ev)) == sizeof(ev)) {
#endif
				// Skip stale input after system sleep
				if (ignore_stale)
					continue;

				// Process key events
				if (ev.type == EV_KEY) {
					pressed = ev.value;
				}
#ifdef KEYMON_HAS_JACK_SWITCH
				// Process switch events for headphone jack (tg5040, zero28)
				else if (ev.type == EV_SW && ev.code == 2) { // SW_HEADPHONE_INSERT
					SetJack(ev.value);
					continue;
				}
#endif
				else {
					continue; // Skip non-key/non-switch events
				}

				// Process hardware button events
				switch (ev.code) {
#if KEYMON_USE_SELECT_START
				case KEYMON_BUTTON_START:
					start_pressed = pressed;
					break;
				case KEYMON_BUTTON_SELECT:
					select_pressed = pressed;
					break;
				case KEYMON_BUTTON_R1:
					// R1 button (brightness/volume up when combined)
					up_pressed = up_just_pressed = pressed;

					if (pressed)
						up_repeat_at = now + 300; // 300ms initial delay
					break;
				case KEYMON_BUTTON_L1:
					// L1 button (brightness/volume down when combined)
					down_pressed = down_just_pressed = pressed;

					if (pressed)
						down_repeat_at = now + 300; // 300ms initial delay
					break;
#ifdef KEYMON_HAS_VOLUME_QUIRK
				// m17: Hardware volume buttons trigger quirk (re-apply volume)
				case KEYMON_BUTTON_PLUS:
				case KEYMON_BUTTON_MINUS:
					system("echo 0 > /sys/devices/platform/0gpio-keys/scaled");
					SetVolume(GetVolume());
					break;
#endif
#else
				case KEYMON_BUTTON_MENU:
#if KEYMON_BUTTON_MENU_ALT != -1
				case KEYMON_BUTTON_MENU_ALT:
#endif
#ifdef KEYMON_BUTTON_MENU_ALT2
				case KEYMON_BUTTON_MENU_ALT2:
#endif
					menu_pressed = pressed;

					break;
				case KEYMON_BUTTON_PLUS:
					up_pressed = up_just_pressed = pressed;

					if (pressed)
						up_repeat_at = now + 300;
					break;
				case KEYMON_BUTTON_MINUS:
					down_pressed = down_just_pressed = pressed;

					if (pressed)
						down_repeat_at = now + 300;
					break;
#endif
				default:
					break;
				}
#if defined(KEYMON_USE_LIBUDEV) || KEYMON_INPUT_COUNT > 1
			}
#endif
		}

		// Reset button state after ignoring stale input
		if (ignore_stale) {
#if KEYMON_USE_SELECT_START
			start_pressed = 0;
			select_pressed = 0;
#else
			menu_pressed = 0;
#endif
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
		}

		// Update timing for next iteration
		then = now;
		ignore_stale = 0;

		// Handle PLUS/R1 button (initial press or repeat after delay)
		if (up_just_pressed || (up_pressed && now >= up_repeat_at)) {
			uint32_t level;
#if KEYMON_USE_SELECT_START
			if (start_pressed) {
				// START+R1: Brightness up
				level = GetBrightness();
				if (level < KEYMON_BRIGHTNESS_MAX)
					SetBrightness(level + 1);
			} else if (select_pressed) {
				// SELECT+R1: Volume up
				level = GetVolume();
				if (level < KEYMON_VOLUME_MAX)
					SetVolume(level + 1);
			}
#else
			if (menu_pressed) {
				// MENU+PLUS: Brightness up
				level = GetBrightness();
				if (level < KEYMON_BRIGHTNESS_MAX)
					SetBrightness(level + 1);
			} else {
				// PLUS alone: Volume up
#ifdef KEYMON_HAS_VOLUME_QUIRK
				// m17 hardware volume button quirk
				system("echo 0 > /sys/devices/platform/0gpio-keys/scaled");
				SetVolume(GetVolume());
#else
				level = GetVolume();
				if (level < KEYMON_VOLUME_MAX)
					SetVolume(level + 1);
#endif
			}
#endif

			if (up_just_pressed)
				up_just_pressed = 0;
			else
				up_repeat_at += 100; // 100ms repeat interval
		}

		// Handle MINUS/L1 button (initial press or repeat after delay)
		if (down_just_pressed || (down_pressed && now >= down_repeat_at)) {
			uint32_t level;
#if KEYMON_USE_SELECT_START
			if (start_pressed) {
				// START+L1: Brightness down
				level = GetBrightness();
				if (level > KEYMON_BRIGHTNESS_MIN)
					SetBrightness(level - 1);
			} else if (select_pressed) {
				// SELECT+L1: Volume down
				level = GetVolume();
				if (level > KEYMON_VOLUME_MIN)
					SetVolume(level - 1);
			}
#else
			if (menu_pressed) {
				// MENU+MINUS: Brightness down
				level = GetBrightness();
				if (level > KEYMON_BRIGHTNESS_MIN)
					SetBrightness(level - 1);
			} else {
				// MINUS alone: Volume down
#ifdef KEYMON_HAS_VOLUME_QUIRK
				// m17 hardware volume button quirk
				system("echo 0 > /sys/devices/platform/0gpio-keys/scaled");
				SetVolume(GetVolume());
#else
				level = GetVolume();
				if (level > KEYMON_VOLUME_MIN)
					SetVolume(level - 1);
#endif
			}
#endif

			if (down_just_pressed)
				down_just_pressed = 0;
			else
				down_repeat_at += 100; // 100ms repeat interval
		}

		usleep(16666); // 60Hz polling rate
	}

	// Clean shutdown
#ifdef KEYMON_USE_LIBUDEV
	udev_close_all(inputs, UDEV_MAX_DEVICES);
#endif
	QuitSettings();
	log_close();

	return 0;
}
