/**
 * utils.c - Core utility functions for Launcher
 *
 * Provides cross-platform utilities for string manipulation, file I/O,
 * timing, and name processing. This file is shared across all platforms
 * and should not contain platform-specific code.
 */

#define _GNU_SOURCE // Required for strcasestr on glibc systems
#include "utils.h"
#include "defines.h"
#include "log.h"
#include "nointro_parser.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

///////////////////////////////
// Timing utilities
///////////////////////////////

/**
 * Gets current time in microseconds since Unix epoch.
 *
 * Used for frame timing, performance measurement, and sleep calculations.
 *
 * @return Time in microseconds (1/1,000,000th of a second)
 */
uint64_t getMicroseconds(void) {
	uint64_t ret;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ret = (uint64_t)tv.tv_sec * 1000000;
	ret += (uint64_t)tv.tv_usec;

	return ret;
}

///////////////////////////////
// String utilities
///////////////////////////////

/**
 * Checks if a string starts with a given prefix (case-insensitive).
 *
 * @param pre Prefix to search for
 * @param str String to search in
 * @return 1 if str starts with pre, 0 otherwise
 */
int prefixMatch(const char* pre, const char* str) {
	return (strncasecmp(pre, str, strlen(pre)) == 0);
}

/**
 * Checks if a string ends with a given suffix (case-insensitive).
 *
 * Commonly used to check file extensions (e.g., ".pak", ".gb").
 *
 * @param suf Suffix to search for
 * @param str String to search in
 * @return 1 if str ends with suf, 0 otherwise
 */
int suffixMatch(const char* suf, const char* str) {
	int len = strlen(suf);
	int offset = strlen(str) - len;
	return (offset >= 0 && strncasecmp(suf, str + offset, len) == 0);
}

/**
 * Checks if two strings are exactly equal (case-sensitive).
 *
 * Unlike strcmp, this returns 1 for match, 0 for non-match.
 * Safely handles NULL pointers.
 *
 * @param str1 First string to compare
 * @param str2 Second string to compare
 * @return 1 if strings are identical, 0 otherwise
 */
int exactMatch(const char* str1, const char* str2) {
	if (!str1 || !str2)
		return 0;
	return (strcmp(str1, str2) == 0);
}

/**
 * Checks if haystack contains needle (case-insensitive).
 *
 * @param haystack String to search in
 * @param needle Substring to search for
 * @return 1 if needle is found in haystack, 0 otherwise
 */
int containsString(char* haystack, char* needle) {
	return strcasestr(haystack, needle) != NULL;
}

int strArrayContains(char** arr, const char* str) {
	if (!arr || !str)
		return 0;
	for (int i = 0; arr[i]; i++) {
		if (strcmp(arr[i], str) == 0)
			return 1;
	}
	return 0;
}

/**
 * Safe string copy with bounds checking.
 *
 * Like BSD strlcpy - always null-terminates the destination buffer,
 * never writes more than dest_size bytes, and returns the length of
 * the source string (allowing truncation detection).
 *
 * @param dest Destination buffer
 * @param src Source string (must be null-terminated)
 * @param dest_size Size of destination buffer (must be > 0)
 * @return Length of src. If return value >= dest_size, truncation occurred.
 */
size_t safe_strcpy(char* dest, const char* src, size_t dest_size) {
	if (dest_size == 0)
		return 0;

	size_t src_len = strlen(src);
	size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

	memcpy(dest, src, copy_len);
	dest[copy_len] = '\0';

	return src_len;
}

/**
 * Determines if a file should be hidden in the UI.
 *
 * Files are hidden if they:
 * - Start with '.' (hidden files)
 * - End with '.disabled'
 * - Are named 'map.txt' (ROM mapping metadata)
 *
 * @param file_name Name of the file (with or without path)
 * @return 1 if file should be hidden, 0 if it should be shown
 */
int hide(char* file_name) {
	return file_name[0] == '.' || suffixMatch(".disabled", file_name) ||
	       exactMatch("map.txt", file_name) || exactMatch("about.txt", file_name);
}

/**
 * Converts Windows line endings (CRLF) to Unix line endings (LF).
 *
 * Modifies the string in place. Useful when reading text files that
 * may have been edited on Windows.
 *
 * @param line String to normalize (modified in place)
 */
void normalizeNewline(char* line) {
	int len = strlen(line);
	if (len > 1 && line[len - 1] == '\n' && line[len - 2] == '\r') {
		line[len - 2] = '\n';
		line[len - 1] = '\0';
	}
}

/**
 * Removes all trailing newline characters from a string.
 *
 * Handles multiple consecutive newlines. Modifies string in place.
 *
 * @param line String to trim (modified in place)
 */
void trimTrailingNewlines(char* line) {
	int len = strlen(line);
	while (len > 0 && line[len - 1] == '\n') {
		line[len - 1] = '\0';
		len -= 1;
	}
}

/**
 * Strips sorting metadata prefix from a string pointer.
 *
 * Advances the string pointer past prefixes like "001) " that are
 * used to control display order in file lists. If no valid prefix
 * is found, the pointer is not modified.
 *
 * Example: "001) Super Mario.gb" -> "Super Mario.gb"
 *
 * @param str Pointer to string pointer (modified in place)
 *
 * @note Modifies the pointer itself, not the string content
 */
void trimSortingMeta(char** str) {
	char* safe = *str;

	// Skip leading digits
	while (isdigit(**str))
		*str += 1;

	// Must have closing parenthesis
	if (*str[0] == ')') {
		*str += 1;
	} else {
		// No valid prefix found, restore original pointer
		*str = safe;
		return;
	}

	// Skip whitespace after parenthesis
	while (isblank(**str))
		*str += 1;
}

/**
 * Splits a text buffer into an array of line pointers.
 *
 * Does NOT modify the input string or allocate memory - it returns
 * pointers into the original buffer. Useful for processing multi-line
 * text files after reading with allocFile().
 *
 * @param str Input text buffer (not modified)
 * @param lines Output array of pointers to line starts
 * @param max_lines Maximum number of lines to process
 * @return Number of lines found (may be less than max_lines)
 *
 * @note Does not null-terminate lines - newlines remain in the buffer
 */
int splitTextLines(char* str, char** lines, int max_lines) {
	if (!str || max_lines < 1)
		return 0;

	int count = 0;
	char* tmp;
	lines[count++] = str;
	while ((tmp = strchr(lines[count - 1], '\n')) != NULL) {
		if (count + 1 > max_lines)
			break;
		lines[count++] = tmp + 1;
	}
	return count;
}

///////////////////////////////
// File I/O utilities
///////////////////////////////

/**
 * Checks if a file or directory exists.
 *
 * @param path Path to check
 * @return 1 if path exists, 0 otherwise
 */
int exists(const char* path) {
	return access(path, F_OK) == 0;
}

/**
 * Finds a system file with platform-specific fallback to common.
 *
 * Checks platform-specific location first, then shared common location.
 */
int findSystemFile(const char* relative_path, char* output_path) {
	char candidate[512];

	// Check platform-specific location first
	(void)sprintf(candidate, "%s/.system/%s/%s", SDCARD_PATH, PLATFORM, relative_path);
	if (exists(candidate)) {
		safe_strcpy(output_path, candidate, MAX_PATH);
		return 1;
	}

	// Fall back to shared common location
	(void)sprintf(candidate, "%s/.system/common/%s", SDCARD_PATH, relative_path);
	if (exists(candidate)) {
		safe_strcpy(output_path, candidate, MAX_PATH);
		return 1;
	}

	return 0;
}

/**
 * Creates an empty file or updates its timestamp.
 *
 * Equivalent to the Unix 'touch' command.
 *
 * @param path Path to file
 */
void touch(char* path) {
	close(open(path, O_RDWR | O_CREAT, 0777));
}

/**
 * Writes a string to a file, overwriting existing content.
 *
 * Creates the file if it doesn't exist. Logs warning if
 * the file cannot be opened (e.g., permissions, disk full).
 *
 * @param path Path to file
 * @param contents String to write (null-terminated)
 */
void putFile(const char* path, const char* contents) {
	FILE* file = fopen(path, "w");
	if (!file) {
		LOG_errno_warn("Failed to write file %s", path);
		return;
	}
	(void)fputs(contents, file);
	(void)fclose(file); // File operation done
}

/**
 * Reads entire file into a pre-allocated buffer.
 *
 * Buffer is always null-terminated. If file is larger than buffer,
 * only the first (buffer_size - 1) bytes are read.
 *
 * @param path Path to file
 * @param buffer Pre-allocated buffer to fill
 * @param buffer_size Size of buffer in bytes
 *
 * @note Buffer is not modified if file cannot be opened
 */
void getFile(const char* path, char* buffer, size_t buffer_size) {
	FILE* file = fopen(path, "r");
	if (!file) {
		LOG_debug("Could not read file %s: %s", path, strerror(errno));
		return;
	}

	size_t bytes_read = 0;
	long file_size;

	// Get file size
	if (fseek(file, 0L, SEEK_END) != 0 || (file_size = ftell(file)) < 0) {
		LOG_errno("Failed to get file size for %s", path);
		goto cleanup;
	}

	// Read file contents
	size_t size = (size_t)file_size;
	if (size > buffer_size - 1)
		size = buffer_size - 1;

	if (fseek(file, 0L, SEEK_SET) != 0) {
		LOG_errno("Failed to rewind file %s", path);
		goto cleanup;
	}

	bytes_read = fread(buffer, sizeof(char), size, file);
	if (bytes_read != size && ferror(file)) {
		LOG_errno("Failed to read file %s", path);
	}

cleanup:
	(void)fclose(file); // File operation done
	buffer[bytes_read] = '\0';
}

/**
 * Reads entire file into dynamically allocated buffer.
 *
 * Allocates exactly the right amount of memory for the file.
 * Returns NULL if file cannot be opened or memory allocation fails.
 *
 * @param path Path to file
 * @return Pointer to allocated buffer, or NULL on failure
 *
 * @warning Caller must free returned pointer
 */
char* allocFile(const char* path) {
	char* contents = NULL;
	FILE* file = fopen(path, "r");
	if (!file) {
		LOG_errno("Failed to open file %s", path);
		return NULL;
	}

	(void)fseek(file, 0L, SEEK_END);
	long file_size = ftell(file);
	if (file_size < 0) {
		LOG_errno("Failed to get file size for %s", path);
		(void)fclose(file); // File operation done
		return NULL;
	}
	size_t size = (size_t)file_size;
	contents = calloc(size + 1, sizeof(char));
	if (!contents) {
		LOG_error("Failed to allocate %zu bytes for file %s", size + 1, path);
		(void)fclose(file); // File operation done
		return NULL;
	}

	(void)fseek(file, 0L, SEEK_SET);
	(void)fread(contents, sizeof(char), size, file);
	contents[size] = '\0';
	(void)fclose(file); // File operation done

	return contents;
}

/**
 * Reads an integer from a text file.
 *
 * Useful for reading simple config values. Supports decimal,
 * octal (0-prefix), and hex (0x-prefix) formats.
 *
 * @param path Path to file
 * @return Integer value, or 0 if file doesn't exist or is invalid
 */
int getInt(const char* path) {
	int i = 0;
	FILE* file = fopen(path, "r");
	if (!file) {
		LOG_debug("Could not read integer from %s: %s", path, strerror(errno));
		return 0;
	}
	(void)fscanf(file, "%i", &i);
	(void)fclose(file); // File operation done
	return i;
}

/**
 * Writes an integer to a text file as a decimal string.
 *
 * Overwrites existing file content.
 *
 * @param path Path to file
 * @param value Integer to write
 */
void putInt(const char* path, int value) {
	char buffer[8];
	(void)sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

///////////////////////////////
// Name processing utilities
///////////////////////////////

/**
 * Helper to move a trailing article to the front of a name.
 *
 * @param name String to transform (modified in place)
 * @param suffix The suffix to look for (e.g., ", The")
 * @param prefix The prefix to prepend (e.g., "The ")
 * @return 1 if transformed, 0 if no match
 */
static int moveArticle(char* name, const char* suffix, const char* prefix) {
	if (!suffixMatch((char*)suffix, name))
		return 0;

	int len = strlen(name);
	int suffix_len = strlen(suffix);
	int prefix_len = strlen(prefix);
	int base_len = len - suffix_len;

	// Shift base name right, prepend article
	memmove(name + prefix_len, name, base_len);
	memcpy(name, prefix, prefix_len);
	name[base_len + prefix_len] = '\0';
	return 1;
}

/**
 * Moves trailing article to the front of a name.
 *
 * Handles No-Intro convention where articles are moved to the end
 * for sorting purposes: "Legend of Zelda, The" -> "The Legend of Zelda"
 *
 * Supported articles: The, A, An (case-insensitive matching)
 *
 * @param name Name to transform (modified in place, same length or shorter)
 */
void fixArticle(char* name) {
	if (!name || !*name)
		return;

	// Try each article pattern (order matters: ", An" before ", A")
	if (moveArticle(name, ", The", "The "))
		return;
	if (moveArticle(name, ", An", "An "))
		return;
	if (moveArticle(name, ", A", "A "))
		return;
}

/**
 * Cleans a ROM or app path for display in the UI.
 *
 * Uses the No-Intro parser for ROM names, which handles:
 * 1. Extracts filename from full path
 * 2. Removes file extensions (including multi-part like .p8.png)
 * 3. Strips No-Intro tags: region (USA), language (En,Ja), version (v1.2), etc.
 * 4. Moves articles to front: "Name, The" -> "The Name"
 * 5. Special handling: strips platform suffix from Tools paths
 *
 * @param in_name Input path (may be full path or just filename)
 * @param out_name Output buffer for cleaned name (min 256 bytes)
 *
 * @note Uses nointro_parser for comprehensive ROM name handling
 */
void getDisplayName(const char* in_name, char* out_name) {
	char work_name[256];
	char* tmp;
	SAFE_STRCPY(work_name, in_name);

	// Special case: hide platform suffix from Tools paths
	if (suffixMatch("/" PLATFORM, work_name)) {
		tmp = strrchr(work_name, '/');
		if (tmp)
			tmp[0] = '\0';
	}

	// Use No-Intro parser to get clean display name
	NoIntroName parsed;
	parseNoIntroName(work_name, &parsed);
	safe_strcpy(out_name, parsed.display_name, 256);
}

/**
 * Extracts emulator/platform name from a ROM path.
 *
 * Examples:
 * - "/mnt/SDCARD/Roms/GB (Nintendo Game Boy)/game.gb" -> "GB"
 * - "/mnt/SDCARD/Roms/NES/mario.nes" -> "NES"
 * - "GBA (Game Boy Advance)" -> "GBA"
 *
 * Logic:
 * 1. If path starts with ROMS_PATH, extract first directory name
 * 2. If that directory name contains parentheses, extract content inside
 *
 * @param in_name ROM path or directory name
 * @param out_name Output buffer for emulator name (must be MAX_PATH length)
 *
 * @note Uses memmove() for safe overlapping buffer copies
 */
void getEmuName(const char* in_name, char* out_name) {
	char* tmp;
	safe_strcpy(out_name, in_name, MAX_PATH);
	tmp = out_name;

	// Extract just the Roms folder name if this is a full path
	if (prefixMatch(ROMS_PATH, tmp)) {
		tmp += strlen(ROMS_PATH) + 1;
		char* tmp2 = strchr(tmp, '/');
		if (tmp2)
			tmp2[0] = '\0';
		// Safe for overlapping buffers
		memmove(out_name, tmp, strlen(tmp) + 1);
		tmp = out_name;
	}

	// Extract abbreviated name from within parentheses if present
	// Example: "GB (Game Boy)" -> "GB"
	tmp = strrchr(tmp, '(');
	if (tmp) {
		tmp += 1;
		// Safe for overlapping buffers
		memmove(out_name, tmp, strlen(tmp) + 1);
		tmp = strchr(out_name, ')');
		tmp[0] = '\0';
	}
}

/**
 * Finds the launch script path for a given emulator.
 *
 * Searches in two locations:
 * 1. Platform-specific: /mnt/SDCARD/Emus/<platform>/<emu_name>.pak/launch.sh
 * 2. Shared: /mnt/SDCARD/Roms/Emus/<emu_name>.pak/launch.sh
 *
 * @param emu_name Emulator name (e.g., "GB", "NES")
 * @param pak_path Output buffer for full path to launch.sh
 *
 * @note pak_path is always modified, even if file doesn't exist
 */
void getEmuPath(char* emu_name, char* pak_path) {
	(void)sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	if (exists(pak_path))
		return;
	(void)sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
}

///////////////////////////////
// Date/time utilities
///////////////////////////////

/**
 * Checks if a year is a leap year.
 *
 * A leap year occurs:
 * - Every 4 years, EXCEPT
 * - Every 100 years, EXCEPT
 * - Every 400 years
 *
 * @param year Year to check (e.g., 2024)
 * @return 1 if leap year, 0 otherwise
 */
int isLeapYear(uint32_t year) {
	return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

/**
 * Gets the number of days in a given month.
 *
 * Handles leap years for February.
 *
 * @param month Month number (1=January, 12=December)
 * @param year Year (needed for February leap year calculation)
 * @return Number of days in the month (28-31)
 */
int getDaysInMonth(int month, uint32_t year) {
	switch (month) {
	case 2:
		return isLeapYear(year) ? 29 : 28;
	case 4:
	case 6:
	case 9:
	case 11:
		return 30;
	default:
		return 31;
	}
}

/**
 * Validates and corrects date/time values.
 *
 * Used by the clock app when user adjusts time values up/down.
 * Wraps values that go out of range (e.g., hour 24 becomes 0,
 * month 13 becomes 1).
 *
 * Constraints:
 * - Year: clamped to 1970-2100
 * - Month: wraps at 1-12
 * - Day: wraps based on days in month
 * - Time: wraps at standard boundaries (24h, 60m, 60s)
 *
 * @param year Pointer to year value (modified in place)
 * @param month Pointer to month value (modified in place)
 * @param day Pointer to day value (modified in place)
 * @param hour Pointer to hour value (modified in place)
 * @param minute Pointer to minute value (modified in place)
 * @param second Pointer to second value (modified in place)
 *
 * @note Simple wrapping - doesn't handle multi-step overflow
 */
void validateDateTime(int32_t* year, int32_t* month, int32_t* day, int32_t* hour, int32_t* minute,
                      int32_t* second) {
	// Month wrapping
	if (*month > 12)
		*month -= 12;
	else if (*month < 1)
		*month += 12;

	// Year clamping (Unix epoch to arbitrary future limit)
	if (*year > 2100)
		*year = 2100;
	else if (*year < 1970)
		*year = 1970;

	// Day validation (depends on month and leap year)
	int max_days = getDaysInMonth(*month, *year);
	if (*day > max_days)
		*day -= max_days;
	else if (*day < 1)
		*day += max_days;

	// Time wrapping
	if (*hour > 23)
		*hour -= 24;
	else if (*hour < 0)
		*hour += 24;

	if (*minute > 59)
		*minute -= 60;
	else if (*minute < 0)
		*minute += 60;

	if (*second > 59)
		*second -= 60;
	else if (*second < 0)
		*second += 60;
}

/**
 * Converts 24-hour time to 12-hour format.
 *
 * @param hour24 Hour in 24-hour format (0-23)
 * @return Hour in 12-hour format (1-12)
 *
 * @note Does not return AM/PM indicator
 */
int convertTo12Hour(int hour24) {
	if (hour24 == 0)
		return 12; // Midnight = 12 AM
	else if (hour24 > 12)
		return hour24 - 12;
	else
		return hour24;
}

///////////////////////////////
// Math utilities
///////////////////////////////

/**
 * Calculates greatest common divisor using Euclidean algorithm.
 *
 * Used for aspect ratio calculations in video scaling.
 *
 * @param a First number
 * @param b Second number
 * @return GCD of a and b
 */
int gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__aarch64__)
/**
 * Averages two RGB565 pixel values (ARM assembly version).
 *
 * Used for bilinear filtering during video scaling. This optimized
 * assembly version is ~2x faster than C on 32-bit ARM devices.
 *
 * RGB565 format: RRRRRGGGGGGBBBBB
 * Mask 0x0821 handles the low bits of each channel to avoid overflow.
 *
 * @param c1 First RGB565 pixel value
 * @param c2 Second RGB565 pixel value
 * @return Average RGB565 value
 *
 * @note Only used on 32-bit ARM (ARMv5+), not on ARM64/AArch64
 */
uint32_t average16(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x0821;
	__asm__("eor %0, %2, %3\r\n"
	        "and %0, %0, %1\r\n"
	        "add %0, %3, %0\r\n"
	        "add %0, %0, %2\r\n"
	        "lsr %0, %0, #1\r\n"
	        : "=&r"(ret)
	        : "r"(lowbits), "r"(c1), "r"(c2)
	        :);
	return ret;
}
#else
/**
 * Averages two RGB565 pixel values (portable C version).
 *
 * Used when ARM assembly optimization is not available.
 * See assembly version for detailed explanation.
 *
 * @param c1 First RGB565 pixel value
 * @param c2 Second RGB565 pixel value
 * @return Average RGB565 value
 */
uint32_t average16(uint32_t c1, uint32_t c2) {
	return (c1 + c2 + ((c1 ^ c2) & 0x0821)) >> 1;
}
#endif

#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__aarch64__)
/**
 * Averages two pairs of RGB565 pixels (ARM assembly version).
 *
 * Processes 32 bits = 2 RGB565 pixels at once for 2x throughput.
 * Handles overflow between pixels using carry flag.
 *
 * @param c1 First pair of RGB565 pixels (packed)
 * @param c2 Second pair of RGB565 pixels (packed)
 * @return Average of both pixel pairs
 *
 * @note Only used on 32-bit ARM (ARMv5+), not on ARM64/AArch64
 */
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x08210821;

	__asm__("eor %0, %3, %1\r\n" // XOR to find differing bits
	        "and %0, %0, %2\r\n" // Mask low bits
	        "adds %0, %1, %0\r\n" // Add with carry flag
	        "and %1, %1, #0\r\n" // Clear c2
	        "movcs %1, #0x80000000\r\n" // Save overflow bit
	        "adds %0, %0, %3\r\n" // Add c1
	        "rrx %0, %0\r\n" // Rotate right through carry
	        "orr %0, %0, %1\r\n" // Restore overflow
	        : "=&r"(ret), "+r"(c2)
	        : "r"(lowbits), "r"(c1)
	        : "cc");

	return ret;
}
#else
/**
 * Averages two pairs of RGB565 pixels (portable C version).
 *
 * Used when ARM assembly optimization is not available.
 * See assembly version for detailed explanation.
 *
 * @param c1 First pair of RGB565 pixels (packed)
 * @param c2 Second pair of RGB565 pixels (packed)
 * @return Average of both pixel pairs
 */
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t sum = c1 + c2;
	uint32_t ret = sum + ((c1 ^ c2) & 0x08210821);
	uint32_t of = ((sum < c1) | (ret < sum)) ? 0x80000000 : 0;

	return (ret >> 1) | of;
}
#endif

/**
 * Sorts an array of uint64_t values in ascending order (in-place).
 *
 * Uses shell sort with Ciura's gap sequence, which provides O(n^1.3) average
 * performance. For our typical 30-element arrays, this completes in ~50
 * comparisons vs ~450 for insertion sort.
 *
 * @param arr Array to sort
 * @param count Number of elements in array
 */
void sortUint64(uint64_t* arr, int count) {
	if (count <= 1)
		return;

	// Ciura's gap sequence (optimal for n < 1000)
	static const int gaps[] = {57, 23, 10, 4, 1};
	int num_gaps = sizeof(gaps) / sizeof(gaps[0]);

	for (int g = 0; g < num_gaps; g++) {
		int gap = gaps[g];
		if (gap >= count)
			continue;

		// Insertion sort with this gap
		for (int i = gap; i < count; i++) {
			uint64_t temp = arr[i];
			int j = i;
			while (j >= gap && arr[j - gap] > temp) {
				arr[j] = arr[j - gap];
				j -= gap;
			}
			arr[j] = temp;
		}
	}
}

/**
 * Calculates a percentile value from an array of uint64_t values.
 *
 * Copies the array to avoid modifying the original, sorts it, and
 * returns the value at the specified percentile position.
 *
 * @param arr Array of values (not modified)
 * @param count Number of elements in array (max 64)
 * @param percentile Percentile to calculate (0.0 to 1.0, e.g., 0.90 for 90th)
 * @return Value at the specified percentile, or 0 if count is 0 or invalid
 */
uint64_t percentileUint64(const uint64_t* arr, int count, float percentile) {
	if (count <= 0 || count > 64 || percentile < 0.0f || percentile > 1.0f)
		return 0;

	// Copy to avoid modifying original
	uint64_t sorted[64];
	memcpy(sorted, arr, (size_t)count * sizeof(uint64_t));

	sortUint64(sorted, count);

	// Calculate index (floor)
	int idx = (int)((float)(count - 1) * percentile);
	return sorted[idx];
}
