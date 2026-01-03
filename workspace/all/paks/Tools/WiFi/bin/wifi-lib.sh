#!/bin/sh
# Shared library for WiFi.pak scripts
# Source this file: . "$BIN_DIR/wifi-lib.sh"

# ============================================================================
# String Utilities
# ============================================================================

# Trim leading and trailing whitespace (POSIX-compatible, no xargs needed)
# Usage: trimmed=$(trim "  hello world  ")
trim() {
	echo "$1" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
}

# ============================================================================
# Environment Setup
# ============================================================================

# Setup PATH and LD_LIBRARY_PATH for WiFi pak binaries
# Usage: setup_wifi_paths "$PAK_DIR"
setup_wifi_paths() {
	pak_dir="$1"
	export PATH="$pak_dir/bin/$PLATFORM:$pak_dir/bin:$PATH"
	export LD_LIBRARY_PATH="$pak_dir/lib/$PLATFORM:$pak_dir/lib:$LD_LIBRARY_PATH"
}

# ============================================================================
# Platform Handling
# ============================================================================

# Normalize platform aliases (legacy: tg3040 -> tg5040+brick)
# Call this at the start of main() in each script
normalize_platform() {
	if [ "$PLATFORM" = "tg3040" ] && [ -z "$LESSUI_DEVICE" ]; then
		export LESSUI_DEVICE="brick"
		export LESSUI_PLATFORM="tg5040"
		export PLATFORM="tg5040"
	fi
}

# Get system.json path for platforms that use it
# Returns empty string for platforms without system.json:
#   - rg35xxplus: uses systemd networking
#   - rgb30/retroid: use IWD (no system.json)
get_system_json_path() {
	case "$PLATFORM" in
		miyoomini) echo "/appconfigs/system.json" ;;
		my282) echo "/config/system.json" ;;
		my355) echo "/userdata/system.json" ;;
		tg5040) echo "/mnt/UDISK/system.json" ;;
		*) echo "" ;;
	esac
}

# Check if platform uses IWD (vs wpa_supplicant)
is_iwd_platform() {
	case "$PLATFORM" in
		rgb30 | retroid) return 0 ;;
		*) return 1 ;;
	esac
}

# List of supported platforms for validation
WIFI_SUPPORTED_PLATFORMS="miyoomini my282 my355 tg5040 rg35xxplus rgb30 retroid"

is_supported_platform() {
	for p in $WIFI_SUPPORTED_PLATFORMS; do
		[ "$PLATFORM" = "$p" ] && return 0
	done
	return 1
}

# ============================================================================
# Network Interface
# ============================================================================

# Dynamically detect WiFi interface name
# Falls back to wlan0 if no interface found
get_wifi_interface() {
	for iface in /sys/class/net/wlan*; do
		[ -e "$iface" ] && basename "$iface" && return
	done
	echo "wlan0"
}

# ============================================================================
# Process Management
# ============================================================================

# Wait for a process to start (by name)
# Usage: wait_for_process "process_name" [max_deciseconds]
# Default timeout: 30 deciseconds (3 seconds)
# Returns: 0 if process started, 1 if timeout
wait_for_process() {
	process_name="$1"
	max_wait="${2:-30}"
	timeout=0

	while [ $timeout -lt "$max_wait" ]; do
		pgrep "$process_name" >/dev/null 2>&1 && return 0
		sleep 0.1
		timeout=$((timeout + 1))
	done

	return 1
}

# Wait for systemd service to become active
# Usage: wait_for_service "service_name" [max_deciseconds]
# Default timeout: 50 deciseconds (5 seconds)
# Returns: 0 if active, 1 if timeout
wait_for_service() {
	service_name="$1"
	max_wait="${2:-50}"
	timeout=0

	while [ $timeout -lt "$max_wait" ]; do
		systemctl is-active --quiet "$service_name" && return 0
		sleep 0.1
		timeout=$((timeout + 1))
	done

	return 1
}

# Wait for IWD device to be powered on
# Usage: wait_for_iwd_powered "interface" [max_deciseconds]
# Default timeout: 50 deciseconds (5 seconds)
# Returns: 0 if powered on, 1 if timeout
wait_for_iwd_powered() {
	iface="$1"
	max_wait="${2:-50}"
	timeout=0
	# Use printf to generate literal ESC char (busybox sed doesn't support \x1b)
	esc=$(printf '\033')

	while [ $timeout -lt "$max_wait" ]; do
		# Strip ANSI color codes and check Powered status
		powered="$(iwctl device "$iface" show 2>/dev/null | sed "s/${esc}\[[0-9;]*m//g" | grep -i 'Powered' | awk '{print $NF}')"
		[ "$powered" = "on" ] && return 0
		sleep 0.1
		timeout=$((timeout + 1))
	done

	return 1
}

# ============================================================================
# System JSON Helpers
# ============================================================================

# Ensure system.json exists with wifi key
# Usage: ensure_system_json "$path"
ensure_system_json() {
	json_path="$1"
	[ -z "$json_path" ] && return 1
	[ ! -f "$json_path" ] && echo '{"wifi": 0}' >"$json_path"
	[ ! -s "$json_path" ] && echo '{"wifi": 0}' >"$json_path"
	return 0
}

# Set wifi value in system.json
# Usage: set_system_wifi "$path" "0|1"
set_system_wifi() {
	json_path="$1"
	value="$2"
	[ -z "$json_path" ] && return 1

	ensure_system_json "$json_path"

	if [ -x /usr/trimui/bin/systemval ]; then
		/usr/trimui/bin/systemval wifi "$value"
	else
		jq ".wifi = $value" "$json_path" >"/tmp/system.json.tmp"
		mv "/tmp/system.json.tmp" "$json_path"
	fi
}

# Get wifi value from system.json
# Usage: get_system_wifi "$path"
# Returns: "0", "1", or empty if not found
get_system_wifi() {
	json_path="$1"
	[ -z "$json_path" ] && return

	ensure_system_json "$json_path"

	if [ -x /usr/trimui/bin/systemval ]; then
		/usr/trimui/bin/systemval wifi
	else
		jq '.wifi' "$json_path" 2>/dev/null
	fi
}
