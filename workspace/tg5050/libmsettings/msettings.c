// tg5050 - Allwinner A523 (Smart Pro S)
//
// Key differences from tg5040:
// - Backlight via sysfs /sys/class/backlight/backlight0/brightness (0-255)
// - Volume via amixer "DAC Volume" control
// - Speaker mute via /sys/class/speaker/mute sysfs
// - Audio initialization unmutes HPOUT, SPK, LINEOUTL, LINEOUTR

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include "msettings.h"

///////////////////////////////////////

#define SETTINGS_VERSION 3
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int headphones;
	int speaker;
	int mute;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack;
} Settings;

static Settings DefaultSettings = {
    .version = SETTINGS_VERSION,
    .brightness = 2,
    .headphones = 4,
    .speaker = 8,
    .mute = 0,
    .jack = 0,
};

static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

#define BACKLIGHT_PATH "/sys/class/backlight/backlight0/brightness"
#define SPEAKER_MUTE_PATH "/sys/class/speaker/mute"

static int getInt(char* path) {
	int i = 0;
	FILE* file = fopen(path, "r");
	if (file != NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

static void putInt(char* path, int value) {
	FILE* file = fopen(path, "w");
	if (file != NULL) {
		fprintf(file, "%d\n", value);
		fclose(file);
	} else {
		printf("putInt: failed to open %s\n", path);
		fflush(stdout);
	}
}

void InitSettings(void) {
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));

	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (shm_fd == -1 && errno == EEXIST) {
		// Already exists - we're a client
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	} else {
		// We created it - we're the host (keymon)
		is_host = 1;
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

		int fd = open(SettingsPath, O_RDONLY);
		if (fd >= 0) {
			read(fd, settings, shm_size);
			close(fd);
		} else {
			// Load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}

		// Reset mute state on init
		settings->mute = 0;
	}

	// A523 audio initialization - unmute all outputs
	system("amixer sset 'HPOUT' unmute 2>/dev/null");
	system("amixer sset 'SPK' unmute 2>/dev/null");
	system("amixer sset 'LINEOUTL' unmute 2>/dev/null");
	system("amixer sset 'LINEOUTR' unmute 2>/dev/null");

	SetVolume(GetVolume());
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
	// tg5050 uses sysfs backlight with linear mapping
	// Stock OS clamps to 10-220, we use similar curve
	int raw;
	switch (value) {
	case 0:
		raw = 10;
		break;
	case 1:
		raw = 20;
		break;
	case 2:
		raw = 35;
		break;
	case 3:
		raw = 50;
		break;
	case 4:
		raw = 70;
		break;
	case 5:
		raw = 95;
		break;
	case 6:
		raw = 120;
		break;
	case 7:
		raw = 150;
		break;
	case 8:
		raw = 180;
		break;
	case 9:
		raw = 210;
		break;
	case 10:
	default:
		raw = 255;
		break;
	}
	SetRawBrightness(raw);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	if (settings->mute)
		return 0;
	return settings->jack ? settings->headphones : settings->speaker;
}

void SetVolume(int value) { // 0-20
	if (settings->mute)
		return SetRawVolume(0);

	if (settings->jack)
		settings->headphones = value;
	else
		settings->speaker = value;

	int raw = value * 5;
	SetRawVolume(raw);
	SaveSettings();
}

void SetRawBrightness(int val) { // 0-255
	printf("SetRawBrightness(%i)\n", val);
	fflush(stdout);

	// tg5050 uses sysfs backlight interface
	putInt(BACKLIGHT_PATH, val);
}

void SetRawVolume(int val) { // 0-100
	printf("SetRawVolume(%i)\n", val);
	fflush(stdout);
	if (settings->mute)
		val = 0;

	// A523 uses 'DAC Volume' control via amixer
	char cmd[256];
	sprintf(cmd, "amixer sset 'DAC Volume' %d%% &> /dev/null", val);
	system(cmd);

	// Full mute requires speaker mute sysfs
	putInt(SPEAKER_MUTE_PATH, val == 0 ? 1 : 0);
}

// Monitored and set by thread in keymon
int GetJack(void) {
	return settings->jack;
}

void SetJack(int value) {
	printf("SetJack(%i)\n", value);
	fflush(stdout);

	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {
	// HDMI not verified on tg5050 yet
	return 0;
}

void SetHDMI(int value) {
	// HDMI not verified on tg5050 yet
	(void)value;
}

int GetMute(void) {
	return settings->mute;
}

void SetMute(int value) {
	settings->mute = value;
	if (settings->mute)
		SetRawVolume(0);
	else
		SetVolume(GetVolume());
}
