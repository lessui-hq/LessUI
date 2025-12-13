/**
 * minarch_archive.c - Archive extraction utilities using 7z
 *
 * Provides functions to extract files from ZIP and 7z archives by
 * shelling out to the 7z binary (available in PATH at runtime).
 */

#include "minarch_archive.h"
#include "../common/defines.h"
#include "../common/log.h"
#include "../common/utils.h"
#include "minarch_game.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Buffer sizes
#define CMD_BUF_SIZE 1024
#define LINE_BUF_SIZE 1024

bool MinArchArchive_isArchive(const char* path) {
	if (!path)
		return false;
	return suffixMatch(".zip", (char*)path) || suffixMatch(".7z", (char*)path);
}

/**
 * Escapes a string for safe use in shell commands.
 * Wraps string in single quotes and escapes internal single quotes.
 *
 * @param src Source string to escape
 * @param dst Destination buffer
 * @param dst_size Size of destination buffer
 * @return true if successful, false if buffer too small
 */
static bool escapeShellArg(const char* src, char* dst, size_t dst_size) {
	if (!src || !dst || dst_size < 3)
		return false;

	size_t j = 0;
	dst[j++] = '\'';

	for (size_t i = 0; src[i] && j < dst_size - 2; i++) {
		if (src[i] == '\'') {
			// Replace ' with '\'' (end quote, escaped quote, start quote)
			if (j + 4 >= dst_size - 1)
				return false;
			dst[j++] = '\'';
			dst[j++] = '\\';
			dst[j++] = '\'';
			dst[j++] = '\'';
		} else {
			dst[j++] = src[i];
		}
	}

	if (j >= dst_size - 1)
		return false;
	dst[j++] = '\'';
	dst[j] = '\0';
	return true;
}

int MinArchArchive_findMatch(const char* archive_path, char* const* extensions, char* out_filename,
                             size_t filename_size) {
	if (!archive_path || !extensions || !out_filename || filename_size == 0)
		return MINARCH_ARCHIVE_ERR_OPEN;

	out_filename[0] = '\0';

	// Build escaped path
	char escaped_path[MAX_PATH * 2];
	if (!escapeShellArg(archive_path, escaped_path, sizeof(escaped_path))) {
		LOG_error("Archive path too long: %s", archive_path);
		return MINARCH_ARCHIVE_ERR_OPEN;
	}

	// Build command: 7z l -slt <archive>
	// -slt = show technical information (one property per line)
	char cmd[CMD_BUF_SIZE];
	snprintf(cmd, sizeof(cmd), "7z l -slt %s 2>/dev/null", escaped_path);

	LOG_debug("Running: %s", cmd);

	FILE* pipe = popen(cmd, "r");
	if (!pipe) {
		LOG_error("Failed to run 7z: %s", strerror(errno));
		return MINARCH_ARCHIVE_ERR_LIST;
	}

	// Parse output looking for "Path = <filename>" lines
	// 7z -slt output format:
	// Path = folder/game.gb
	// Folder = -
	// Size = 12345
	// ...
	char line[LINE_BUF_SIZE];
	bool found = false;

	while (fgets(line, sizeof(line), pipe)) {
		// Remove newline
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		// Check for "Path = " prefix
		if (strncmp(line, "Path = ", 7) == 0) {
			const char* filename = line + 7;

			// Skip directory entries (7z marks these with trailing /)
			len = strlen(filename);
			if (len > 0 && filename[len - 1] == '/')
				continue;

			// Check if this file matches any extension
			if (MinArchGame_matchesExtension(filename, extensions)) {
				// Get just the basename (strip directory path from archive)
				const char* base = strrchr(filename, '/');
				if (base)
					base++;
				else
					base = filename;

				if (strlen(base) >= filename_size) {
					LOG_error("Filename too long: %s", base);
					pclose(pipe);
					return MINARCH_ARCHIVE_ERR_NO_MATCH;
				}

				strcpy(out_filename, base);
				found = true;
				break;
			}
		}
	}

	pclose(pipe);

	if (!found) {
		LOG_debug("No matching file in archive: %s", archive_path);
		return MINARCH_ARCHIVE_ERR_NO_MATCH;
	}

	LOG_info("Found matching file in archive: %s", out_filename);
	return MINARCH_ARCHIVE_OK;
}

int MinArchArchive_extract(const char* archive_path, char* const* extensions,
                           char* out_extracted_path, size_t path_size) {
	if (!archive_path || !extensions || !out_extracted_path || path_size == 0)
		return MINARCH_ARCHIVE_ERR_OPEN;

	out_extracted_path[0] = '\0';

	// Find matching file in archive
	char filename[MAX_PATH];
	int result = MinArchArchive_findMatch(archive_path, extensions, filename, sizeof(filename));
	if (result != MINARCH_ARCHIVE_OK)
		return result;

	// Create temp directory
	char tmp_template[MAX_PATH];
	strcpy(tmp_template, "/tmp/minarch-XXXXXX");
	char* tmp_dirname = mkdtemp(tmp_template);
	if (!tmp_dirname) {
		LOG_error("Failed to create temp directory: %s", strerror(errno));
		return MINARCH_ARCHIVE_ERR_TMPDIR;
	}

	LOG_debug("Created temp directory: %s", tmp_dirname);

	// Build escaped arguments
	char escaped_archive[MAX_PATH * 2];
	char escaped_filename[MAX_PATH * 2];
	char escaped_outdir[MAX_PATH * 2];

	if (!escapeShellArg(archive_path, escaped_archive, sizeof(escaped_archive)) ||
	    !escapeShellArg(filename, escaped_filename, sizeof(escaped_filename)) ||
	    !escapeShellArg(tmp_dirname, escaped_outdir, sizeof(escaped_outdir))) {
		LOG_error("Path too long for extraction");
		rmdir(tmp_dirname);
		return MINARCH_ARCHIVE_ERR_EXTRACT;
	}

	// Build extract command: 7z e -y -o<dir> <archive> <filename>
	// -e = extract without directory structure (flat)
	// -y = assume yes to all prompts
	// -o = output directory (no space after -o)
	char cmd[CMD_BUF_SIZE];
	snprintf(cmd, sizeof(cmd), "7z e -y -o%s %s %s >/dev/null 2>&1", escaped_outdir,
	         escaped_archive, escaped_filename);

	LOG_debug("Running: %s", cmd);

	int ret = system(cmd);
	if (ret != 0) {
		LOG_error("7z extraction failed with code %d", ret);
		rmdir(tmp_dirname);
		return MINARCH_ARCHIVE_ERR_EXTRACT;
	}

	// Build output path
	if (snprintf(out_extracted_path, path_size, "%s/%s", tmp_dirname, filename) >= (int)path_size) {
		LOG_error("Extracted path too long");
		rmdir(tmp_dirname);
		return MINARCH_ARCHIVE_ERR_EXTRACT;
	}

	// Verify file exists
	struct stat st;
	if (stat(out_extracted_path, &st) != 0) {
		LOG_error("Extracted file not found: %s", out_extracted_path);
		rmdir(tmp_dirname);
		out_extracted_path[0] = '\0';
		return MINARCH_ARCHIVE_ERR_EXTRACT;
	}

	LOG_info("Extracted: %s", out_extracted_path);
	return MINARCH_ARCHIVE_OK;
}
