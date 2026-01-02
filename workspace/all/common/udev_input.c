/**
 * udev_input.c - Dynamic input device discovery using libudev
 *
 * Implementation of libudev-based input device enumeration.
 * Provides robust device discovery without hardcoded event paths.
 */

#include "udev_input.h"

#include <fcntl.h>
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

/**
 * Compare function for sorting device paths.
 * Ensures consistent ordering (event0, event1, event2, ...).
 */
static int compare_device_paths(const void* a, const void* b) {
	return strcmp(*(const char**)a, *(const char**)b);
}

int udev_open_joysticks(int* fds) {
	struct udev* udev = NULL;
	struct udev_enumerate* enumerate = NULL;
	struct udev_list_entry* devices = NULL;
	struct udev_list_entry* entry = NULL;
	const char* device_paths[UDEV_MAX_DEVICES];
	int path_count = 0;
	int opened = 0;

	// Initialize all fds to -1
	for (int i = 0; i < UDEV_MAX_DEVICES; i++) {
		fds[i] = -1;
	}

	udev = udev_new();
	if (!udev) {
		LOG_error("Failed to create udev context");
		return 0;
	}

	enumerate = udev_enumerate_new(udev);
	if (!enumerate) {
		LOG_error("Failed to create udev enumerate");
		udev_unref(udev);
		return 0;
	}

	// Filter for input subsystem with joystick property
	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_add_match_property(enumerate, "ID_INPUT_JOYSTICK", "1");
	udev_enumerate_scan_devices(enumerate);

	devices = udev_enumerate_get_list_entry(enumerate);

	// Collect device paths
	udev_list_entry_foreach(entry, devices) {
		const char* syspath = udev_list_entry_get_name(entry);
		struct udev_device* dev = udev_device_new_from_syspath(udev, syspath);
		if (!dev)
			continue;

		const char* devnode = udev_device_get_devnode(dev);
		if (devnode && path_count < UDEV_MAX_DEVICES) {
			// Only accept /dev/input/event* devices (not js* or others)
			if (strstr(devnode, "/dev/input/event") != NULL) {
				char* dup = strdup(devnode);
				if (dup) {
					device_paths[path_count++] = dup;
					LOG_debug("Found joystick: %s", devnode);
				} else {
					LOG_warn("Failed to allocate memory for device path");
				}
			}
		}
		udev_device_unref(dev);
	}

	// Sort paths for consistent ordering
	if (path_count > 1) {
		qsort(device_paths, path_count, sizeof(char*), compare_device_paths);
	}

	// Open devices
	for (int i = 0; i < path_count; i++) {
		int fd = open(device_paths[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd >= 0) {
			fds[opened++] = fd;
			LOG_info("Opened joystick: %s (fd=%d)", device_paths[i], fd);
		} else {
			LOG_warn("Failed to open %s", device_paths[i]);
		}
		free((void*)device_paths[i]);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	if (opened == 0) {
		LOG_warn("No joystick devices found");
	}

	return opened;
}

int udev_open_all_inputs(int* fds) {
	struct udev* udev = NULL;
	struct udev_enumerate* enumerate = NULL;
	struct udev_list_entry* devices = NULL;
	struct udev_list_entry* entry = NULL;
	const char* device_paths[UDEV_MAX_DEVICES];
	int path_count = 0;
	int opened = 0;

	// Initialize all fds to -1
	for (int i = 0; i < UDEV_MAX_DEVICES; i++) {
		fds[i] = -1;
	}

	udev = udev_new();
	if (!udev) {
		LOG_error("Failed to create udev context");
		return 0;
	}

	enumerate = udev_enumerate_new(udev);
	if (!enumerate) {
		LOG_error("Failed to create udev enumerate");
		udev_unref(udev);
		return 0;
	}

	// Filter for input subsystem only (gets all input devices)
	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);

	devices = udev_enumerate_get_list_entry(enumerate);

	// Collect device paths
	udev_list_entry_foreach(entry, devices) {
		const char* syspath = udev_list_entry_get_name(entry);
		struct udev_device* dev = udev_device_new_from_syspath(udev, syspath);
		if (!dev)
			continue;

		const char* devnode = udev_device_get_devnode(dev);
		if (devnode && path_count < UDEV_MAX_DEVICES) {
			// Only accept /dev/input/event* devices
			if (strstr(devnode, "/dev/input/event") != NULL) {
				char* dup = strdup(devnode);
				if (dup) {
					device_paths[path_count++] = dup;
					LOG_debug("Found input device: %s", devnode);
				} else {
					LOG_warn("Failed to allocate memory for device path");
				}
			}
		}
		udev_device_unref(dev);
	}

	// Sort paths for consistent ordering
	if (path_count > 1) {
		qsort(device_paths, path_count, sizeof(char*), compare_device_paths);
	}

	// Open devices
	for (int i = 0; i < path_count; i++) {
		int fd = open(device_paths[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd >= 0) {
			fds[opened++] = fd;
			LOG_debug("Opened input: %s (fd=%d)", device_paths[i], fd);
		} else {
			LOG_warn("Failed to open %s", device_paths[i]);
		}
		free((void*)device_paths[i]);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	return opened;
}

const char* udev_find_device_by_name(const char* device_name) {
	static char result_path[128];
	struct udev* udev = NULL;
	struct udev_enumerate* enumerate = NULL;
	struct udev_list_entry* devices = NULL;
	struct udev_list_entry* entry = NULL;
	const char* found = NULL;

	if (!device_name) {
		return NULL;
	}

	udev = udev_new();
	if (!udev) {
		LOG_error("Failed to create udev context");
		return NULL;
	}

	enumerate = udev_enumerate_new(udev);
	if (!enumerate) {
		LOG_error("Failed to create udev enumerate");
		udev_unref(udev);
		return NULL;
	}

	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(entry, devices) {
		const char* syspath = udev_list_entry_get_name(entry);
		struct udev_device* dev = udev_device_new_from_syspath(udev, syspath);
		if (!dev)
			continue;

		const char* devnode = udev_device_get_devnode(dev);
		if (!devnode || strstr(devnode, "/dev/input/event") == NULL) {
			udev_device_unref(dev);
			continue;
		}

		// Get the device name from sysattr
		const char* name = udev_device_get_sysattr_value(dev, "name");
		if (name && strcmp(name, device_name) == 0) {
			(void)snprintf(result_path, sizeof(result_path), "%s", devnode);
			found = result_path;
			LOG_debug("Found '%s' at %s", device_name, result_path);
			udev_device_unref(dev);
			break;
		}

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	if (!found) {
		LOG_warn("Device '%s' not found", device_name);
	}

	return found;
}

void udev_close_all(int* fds, int count) {
	for (int i = 0; i < count; i++) {
		if (fds[i] >= 0) {
			close(fds[i]);
			fds[i] = -1;
		}
	}
}
