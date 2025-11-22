#!/bin/sh
#
# log.sh - LessUI Shell Script Logging Library
#
# SOURCE LOCATION: skeleton/SYSTEM/common/log.sh
# BUILD DESTINATIONS (copied during make):
#   1. .system/common/log.sh (runtime - sourced by launch.sh)
#   2. .tmp_update/install/log.sh (update time - sourced by boot.sh)
#
# Provides consistent, timestamped logging for boot scripts, install scripts,
# and launch scripts across all platforms.
#
# Features:
# - Automatic timestamps (HH:MM:SS)
# - Log levels: INFO, WARN, ERROR
# - Automatic log file rotation (1MB limit)
# - Thread-safe append mode
# - Console + file output (tee)
#
# Usage:
#   source $SYSTEM_PATH/common/log.sh
#   log_init "$SDCARD_PATH/boot.log"
#   log_info "System starting..."
#   log_warn "Battery low"
#   log_error "Failed to mount SD card"
#
# Environment variables:
#   LOG_FILE - Current log file path
#   LOG_ENABLED - 1 to enable logging, 0 to disable
#

###############################################################################
# Configuration
###############################################################################

# Only set LOG_FILE if not already set by the calling script
: "${LOG_FILE:=}"
LOG_ENABLED=1
LOG_MAX_SIZE=1048576  # 1MB in bytes
LOG_MAX_BACKUPS=3

###############################################################################
# Internal Functions
###############################################################################

# Get current timestamp in HH:MM:SS format
log_timestamp() {
	date '+%H:%M:%S'
}

# Get file size in bytes (portable across busybox and GNU coreutils)
log_get_size() {
	_log_file="$1"
	if [ ! -f "$_log_file" ]; then
		echo 0
		return
	fi

	# Try stat (GNU coreutils)
	if command -v stat >/dev/null 2>&1; then
		# Linux (busybox or GNU)
		stat -c%s "$_log_file" 2>/dev/null && return
		# macOS/BSD
		stat -f%z "$_log_file" 2>/dev/null && return
	fi

	# Fallback: use wc (slower but universal)
	wc -c < "$_log_file" 2>/dev/null || echo 0
}

# Rotate log file if it exceeds maximum size
log_rotate() {
	_log_file="$1"

	if [ ! -f "$_log_file" ]; then
		return
	fi

	_log_size=$(log_get_size "$_log_file")

	if [ "$_log_size" -lt "$LOG_MAX_SIZE" ]; then
		return
	fi

	# Delete oldest backup
	rm -f "${_log_file}.${LOG_MAX_BACKUPS}" 2>/dev/null

	# Rotate backups (N-1 -> N, N-2 -> N-1, ...)
	_log_i=$((LOG_MAX_BACKUPS - 1))
	while [ $_log_i -ge 1 ]; do
		if [ -f "${_log_file}.${_log_i}" ]; then
			mv "${_log_file}.${_log_i}" "${_log_file}.$((_log_i + 1))" 2>/dev/null
		fi
		_log_i=$((_log_i - 1))
	done

	# Move current log to .1
	mv "$_log_file" "${_log_file}.1" 2>/dev/null
}

###############################################################################
# Public API
###############################################################################

# Initialize logging to a file
#
# Usage: log_init "/path/to/file.log"
#
# Creates log file directory if needed, rotates if file is too large,
# and writes a session header.
log_init() {
	LOG_FILE="$1"

	if [ -z "$LOG_FILE" ]; then
		LOG_ENABLED=0
		return
	fi

	# Create directory if needed
	mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null

	# Rotate if file exists and is too large
	log_rotate "$LOG_FILE"

	# Write session header
	{
		echo "========================================"
		echo "Log started: $(date '+%Y-%m-%d %H:%M:%S')"
		echo "========================================"
	} >> "$LOG_FILE"
}

# Log an informational message
#
# Usage: log_info "message"
#
# Writes to both console (stdout) and log file with [INFO] prefix.
log_info() {
	[ "$LOG_ENABLED" -eq 0 ] && return
	_log_msg="[$(log_timestamp)] [INFO] $*"

	if [ -n "$LOG_FILE" ]; then
		echo "$_log_msg" | tee -a "$LOG_FILE"
	else
		echo "$_log_msg"
	fi
}

# Log a warning message
#
# Usage: log_warn "message"
#
# Writes to both console (stdout) and log file with [WARN] prefix.
log_warn() {
	[ "$LOG_ENABLED" -eq 0 ] && return
	_log_msg="[$(log_timestamp)] [WARN] $*"

	if [ -n "$LOG_FILE" ]; then
		echo "$_log_msg" | tee -a "$LOG_FILE"
	else
		echo "$_log_msg"
	fi
}

# Log an error message
#
# Usage: log_error "message"
#
# Writes to both console (stderr) and log file with [ERROR] prefix.
log_error() {
	[ "$LOG_ENABLED" -eq 0 ] && return
	_log_msg="[$(log_timestamp)] [ERROR] $*"

	if [ -n "$LOG_FILE" ]; then
		echo "$_log_msg" | tee -a "$LOG_FILE" >&2
	else
		echo "$_log_msg" >&2
	fi
}

# Disable logging
#
# Usage: log_disable
#
# Stops writing to log file. Console output continues.
log_disable() {
	LOG_ENABLED=0
}

# Enable logging
#
# Usage: log_enable
#
# Resumes writing to log file.
log_enable() {
	LOG_ENABLED=1
}

# Write session footer and close log
#
# Usage: log_close
#
# Optional cleanup function. Writes a footer to the log file.
log_close() {
	[ "$LOG_ENABLED" -eq 0 ] && return
	[ -z "$LOG_FILE" ] && return

	{
		echo "========================================"
		echo "Log ended: $(date '+%Y-%m-%d %H:%M:%S')"
		echo "========================================"
	} >> "$LOG_FILE"
}
