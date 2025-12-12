#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
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

int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

void InitSettings(void) {	
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));
	
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd==-1 && errno==EEXIST) { // already exists
		// puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else { // host
		// puts("Settings host"); // keymon
		is_host = 1;
		// we created it so set initial size and populate
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		
		int fd = open(SettingsPath, O_RDONLY);
		if (fd>=0) {
			read(fd, settings, shm_size);
			// TODO: use settings->version for future proofing?
			close(fd);
		}
		else {
			// load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}
		
		settings->mute = 0;
	}

	system("amixer sset 'Headphone' 0"); // 100%
	system("amixer sset 'digital volume' 0"); // 100%
	system("amixer sset 'Soft Volume Master' 255"); // 100%
	// volume is set with 'DAC volume'
	
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
}
void QuitSettings(void) {
	munmap(settings, shm_size);
	if (is_host) shm_unlink(SHM_KEY);
}
static inline void SaveSettings(void) {
	int fd = open(SettingsPath, O_CREAT|O_WRONLY, 0644);
	if (fd>=0) {
		write(fd, settings, shm_size);
		close(fd);
		sync();
	}
}

int GetBrightness(void) { // 0-10
	return settings->brightness;
}
void SetBrightness(int value) {
	
	int raw;
	switch (value) {
		// TODO: revisit
		case  0: raw =   1; break; 	// 0
		case  1: raw =   8; break; 	// 8
		case  2: raw =  16; break; 	// 8
		case  3: raw =  32; break; 	// 16
		case  4: raw =  48; break;	// 16
		case  5: raw =  72; break;	// 24
		case  6: raw =  96; break;	// 24
		case  7: raw = 128; break;	// 32
		case  8: raw = 160; break;	// 32
		case  9: raw = 192; break;	// 32
		case 10: raw = 255; break;	// 64
	}
	SetRawBrightness(raw);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	if (settings->mute) return 0;
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) { // 0-20
	if (settings->mute) return SetRawVolume(0);
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	int raw = value * 5;
	if (raw>0) raw = 96 + (64 * raw) / 100;
	SetRawVolume(raw);
	SaveSettings();
}

#define DISP_LCD_SET_BRIGHTNESS  0x102
void SetRawBrightness(int val) { // 0 - 255
	val = 255-val; // zero28 display driver uses inverted values

    int fd = open("/dev/disp", O_RDWR);
	if (fd) {
	    unsigned long param[4]={0,val,0,0};
		ioctl(fd, DISP_LCD_SET_BRIGHTNESS, &param);
		close(fd);
	}
}
void SetRawVolume(int val) { // 0 or 96 - 160
	if (settings->mute) val = 0;

	char cmd[256];
	sprintf(cmd, "amixer sset 'DAC volume' %i &> /dev/null", val);
	system(cmd);
}

int GetJack(void) {
	return settings->jack;
}
void SetJack(int value) {
	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {
	return 0; // zero28 has no HDMI
}
void SetHDMI(int value) {
	(void)value; // zero28 has no HDMI
}

int GetMute(void) {
	return settings->mute;
}
void SetMute(int value) {
	settings->mute = value;
	if (settings->mute) SetRawVolume(0);
	else SetVolume(GetVolume());
}