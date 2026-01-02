/**
 * udev_input.h - Dynamic input device discovery using libudev
 *
 * Provides robust input device enumeration for LessOS platforms.
 * Uses libudev to dynamically discover joystick and input devices,
 * replacing hardcoded /dev/input/event* paths.
 *
 * This approach is more reliable than sysfs parsing because:
 * - No assumptions about event device numbering
 * - Handles devices appearing in any order
 * - Matches ROCKNIX upstream pattern
 */

#ifndef UDEV_INPUT_H
#define UDEV_INPUT_H

// Maximum number of input devices to enumerate
#define UDEV_MAX_DEVICES 16

/**
 * Open all joystick input devices.
 *
 * Uses libudev to find devices with ID_INPUT_JOYSTICK=1 property.
 * Opens each device with O_RDONLY | O_NONBLOCK | O_CLOEXEC.
 *
 * @param fds Array to store file descriptors (must have UDEV_MAX_DEVICES entries)
 * @return Number of devices successfully opened (0 if none found or error)
 */
int udev_open_joysticks(int* fds);

/**
 * Open all input event devices (joystick + keyboard + keys).
 *
 * Discovers all /dev/input/event* devices and opens them.
 * Useful for keymon which needs to monitor power button, volume keys, etc.
 *
 * @param fds Array to store file descriptors (must have UDEV_MAX_DEVICES entries)
 * @return Number of devices successfully opened
 */
int udev_open_all_inputs(int* fds);

/**
 * Find device path by device name.
 *
 * Searches udev for an input device matching the specified name.
 * This is useful for finding specific devices like "Retroid Pocket Gamepad".
 *
 * @param device_name Exact device name to search for
 * @return Device path (e.g., "/dev/input/event2") or NULL if not found.
 *         Returns pointer to static buffer - copy if you need to keep it.
 */
const char* udev_find_device_by_name(const char* device_name);

/**
 * Close all open file descriptors in an array.
 *
 * Closes any fd >= 0 and sets it to -1.
 *
 * @param fds Array of file descriptors
 * @param count Number of entries in the array
 */
void udev_close_all(int* fds, int count);

#endif // UDEV_INPUT_H
