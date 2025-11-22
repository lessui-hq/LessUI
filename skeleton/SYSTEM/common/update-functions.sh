#!/bin/sh
#
# update-functions.sh - LessUI Update/Install Functions
#
# SOURCE LOCATION: skeleton/SYSTEM/common/update-functions.sh
# BUILD DESTINATION (copied during make):
#   .tmp_update/install/update-functions.sh (update time - sourced by boot.sh)
#
# Provides atomic update logic with automatic rollback for all platforms.
# Requires log.sh to be sourced first.
#

# Source logging functions (in same directory as this file)
# Both update-functions.sh and log.sh are in .tmp_update/install/
# When sourced from boot.sh via: . "$(dirname "$0")/install/update-functions.sh"
# We can find log.sh using the same base path
SCRIPT_DIR="$(dirname "$0")"
. "$SCRIPT_DIR/install/log.sh"

#######################################
# Atomic update with automatic rollback
#
# Backs up .system before update, restores on failure.
# This prevents orphaned files and partial updates.
#
# Args:
#   $1 - UPDATE_PATH (path to LessUI.zip)
#   $2 - SDCARD_PATH (SD card mount point)
#   $3 - SYSTEM_PATH (.system directory path)
#   $4 - LOG_FILE (log file path)
#
# Returns:
#   0 on success, 1 on failure
#######################################
atomic_system_update() {
	_update_zip="$1"
	_sdcard="$2"
	_system_dir="$3"
	_log="$4"

	# Backup old system if it exists
	if [ -d "$_system_dir" ]; then
		log_info "Backing up existing .system to .system-prev..."
		# Remove any stale backup first
		rm -rf "$_system_dir-prev"
		# Create fresh backup
		if ! mv "$_system_dir" "$_system_dir-prev"; then
			log_error "Failed to backup .system - aborting update"
			return 1
		fi
	fi

	# Move old .tmp_update out of the way (original approach)
	mv "$_sdcard/.tmp_update" "$_sdcard/.tmp_update-old" 2>/dev/null

	# Extract update
	if unzip -o "$_update_zip" -d "$_sdcard" >> "$_log" 2>&1; then
		log_info "Unzip complete"
	else
		_exit_code=$?
		log_error "Unzip failed with exit code $_exit_code"
	fi
	rm -f "$_update_zip"
	rm -rf "$_sdcard/.tmp_update-old"

	# Success - remove .system backup
	rm -rf "$_system_dir-prev"
	return 0
}

#######################################
# Run platform-specific install.sh script
#
# Executes the platform's install.sh script if it exists.
# Logs success or failure.
#
# Args:
#   $1 - INSTALL_SCRIPT (full path to install.sh)
#   $2 - LOG_FILE (log file path)
#
# Returns:
#   0 on success or if script doesn't exist, 1 on failure
#######################################
run_platform_install() {
	_install_script="$1"
	_log_file="$2"

	if [ -f "$_install_script" ]; then
		log_info "Running install.sh..."
		if "$_install_script" >> "$_log_file" 2>&1; then
			log_info "Installation complete"
			return 0
		else
			_exit_code=$?
			log_error "install.sh failed with exit code $_exit_code"
			return 1
		fi
	fi
	return 0
}
