#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>

#include "log.h"
#include "msettings.h"

///////////////////////////////////////

#define SETTINGS_VERSION 2
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int headphones;
	int speaker;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack;
	int hdmi;
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 5,
	.headphones = 8,
	.speaker = 12,
	.jack = 0,
	.hdmi = 0,
};
static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

// SM8250 uses DSI panel backlight (device name: ae94000.dsi.0)
#define BRIGHTNESS_PATH "/sys/class/backlight/ae94000.dsi.0/brightness"
#define BRIGHTNESS_MAX_PATH "/sys/class/backlight/ae94000.dsi.0/max_brightness"
#define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

static int max_brightness = 255;

int getInt(char* path) {
	int i = 0;
	FILE* file = fopen(path, "r");
	if (file != NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

void InitSettings(void) {
	const char* userdata = getenv("USERDATA_PATH");
	if (!userdata)
		userdata = "/storage/.userdata/retroid"; // Fallback if env not set
	snprintf(SettingsPath, sizeof(SettingsPath), "%s/msettings.bin", userdata);

	// Read max brightness from sysfs
	max_brightness = getInt(BRIGHTNESS_MAX_PATH);
	if (max_brightness <= 0)
		max_brightness = 255;

	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd == -1 && errno == EEXIST) { // already exists
		LOG_debug("Settings client (connecting to existing shared memory)");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	} else { // host
		LOG_debug("Settings host (creating new shared memory)");
		is_host = 1;
		// we created it so set initial size and populate
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

		int fd = open(SettingsPath, O_RDONLY);
		if (fd >= 0) {
			read(fd, settings, shm_size);
			close(fd);
		} else {
			// load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}
	}

	int hdmi = getInt(HDMI_STATE_PATH);
	LOG_debug("Loaded settings: brightness=%i hdmi=%i speaker=%i", settings->brightness, hdmi,
	          settings->speaker);

	SetHDMI(hdmi);
	SetBrightness(GetBrightness());
}

void QuitSettings(void) {
	munmap(settings, shm_size);
	if (is_host)
		shm_unlink(SHM_KEY);
}

static inline void SaveSettings(void) {
	int fd = open(SettingsPath, O_CREAT | O_WRONLY, 0644);
	if (fd >= 0) {
		write(fd, settings, shm_size);
		close(fd);
		sync();
	}
}

int GetBrightness(void) { // 0-10
	return settings->brightness;
}

void SetBrightness(int value) {
	if (settings->hdmi)
		return;

	// Map 0-10 to exponential curve for AMOLED perception
	int raw;
	switch (value) {
	case 0:
		raw = max_brightness * 1 / 100;
		break;
	case 1:
		raw = max_brightness * 2 / 100;
		break;
	case 2:
		raw = max_brightness * 4 / 100;
		break;
	case 3:
		raw = max_brightness * 8 / 100;
		break;
	case 4:
		raw = max_brightness * 15 / 100;
		break;
	case 5:
		raw = max_brightness * 25 / 100;
		break;
	case 6:
		raw = max_brightness * 40 / 100;
		break;
	case 7:
		raw = max_brightness * 55 / 100;
		break;
	case 8:
		raw = max_brightness * 70 / 100;
		break;
	case 9:
		raw = max_brightness * 85 / 100;
		break;
	case 10:
		raw = max_brightness;
		break;
	default:
		raw = max_brightness / 2;
		break;
	}
	SetRawBrightness(raw);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	return settings->jack ? settings->headphones : settings->speaker;
}

void SetVolume(int value) {
	if (settings->hdmi)
		return;

	if (settings->jack)
		settings->headphones = value;
	else
		settings->speaker = value;

	int raw = value * 5;
	SetRawVolume(raw);
	SaveSettings();
}

void SetRawBrightness(int val) {
	if (settings->hdmi)
		return;

	FILE* file = fopen(BRIGHTNESS_PATH, "w");
	if (file) {
		fprintf(file, "%d\n", val);
		fclose(file);
	} else {
		LOG_warn("Failed to set brightness to %d: %s", val, strerror(errno));
	}
}

void SetRawVolume(int val) { // 0 - 100
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "amixer sset -M 'Master' %i%% &> /dev/null", val);
	system(cmd);
}

// monitored and set by thread in keymon
int GetJack(void) {
	return settings->jack;
}

void SetJack(int value) {
	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {
	return settings->hdmi;
}

void SetHDMI(int value) {
	settings->hdmi = value;
	if (value)
		SetRawVolume(100); // max
	else
		SetVolume(GetVolume()); // restore
}

int GetMute(void) {
	return 0;
}
void SetMute(int value) {
}
