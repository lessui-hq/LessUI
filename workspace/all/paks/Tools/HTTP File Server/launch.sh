#!/bin/sh
PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR")"
PAK_NAME="${PAK_NAME%.*}"

rm -f "$LOGS_PATH/$PAK_NAME.txt"
exec >>"$LOGS_PATH/$PAK_NAME.txt"
exec 2>&1

echo "$0" "$@"
cd "$PAK_DIR" || exit 1
mkdir -p "$USERDATA_PATH/$PAK_NAME"

# dufs binary is in common/bin/$PLATFORM_ARCH (added to PATH by LessUI.pak)
# Only need pak's bin dir for service scripts
export PATH="$PAK_DIR/bin:$PATH"

SERVICE_NAME="dufs"
HUMAN_READABLE_NAME="HTTP File Server"
LAUNCHES_SCRIPT="false"
NETWORK_PORT=80
NETWORK_SCHEME="http"

show_message() {
	message="$1"
	echo "$message" 1>&2
	shui message "$message" --confirm "Dismiss"
}

show_progress() {
	message="$1"
	echo "$message" 1>&2
	shui progress "$message" --indeterminate
}

disable_start_on_boot() {
	sed -i "/${PAK_NAME}.pak-on-boot/d" "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
	sync
	return 0
}

enable_start_on_boot() {
	if [ ! -f "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh" ]; then
		echo '#!/bin/sh' >"$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
		echo '' >>"$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
	fi

	echo "test -f \"\$SDCARD_PATH/Tools/\$PLATFORM/$PAK_NAME.pak/bin/on-boot\" && \"\$SDCARD_PATH/Tools/\$PLATFORM/$PAK_NAME.pak/bin/on-boot\" # ${PAK_NAME}.pak-on-boot" >>"$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
	chmod +x "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
	sync
	return 0
}

will_start_on_boot() {
	if grep -q "${PAK_NAME}.pak-on-boot" "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh" >/dev/null 2>&1; then
		return 0
	fi
	return 1
}

wait_for_service() {
	max_counter="$1"
	counter=0

	while ! service-is-running; do
		counter=$((counter + 1))
		if [ "$counter" -gt "$max_counter" ]; then
			return 1
		fi
		sleep 1
	done
}

wait_for_service_to_stop() {
	max_counter="$1"
	counter=0

	while service-is-running; do
		counter=$((counter + 1))
		if [ "$counter" -gt "$max_counter" ]; then
			return 1
		fi
		sleep 1
	done
}

get_service_pid() {
	if [ "$LAUNCHES_SCRIPT" = "true" ]; then
		pgrep -fn "$SERVICE_NAME" 2>/dev/null | sort | head -n 1 || true
	else
		pgrep "$SERVICE_NAME" 2>/dev/null | sort | head -n 1 || true
	fi
}

get_ip_address() {
	if [ -z "$NETWORK_PORT" ]; then
		return
	fi

	# Check if WiFi interface is up
	if [ ! -f /sys/class/net/wlan0/operstate ]; then
		echo "WiFi not available"
		return
	fi

	enabled="$(cat /sys/class/net/wlan0/operstate)"
	if [ "$enabled" != "up" ]; then
		echo "WiFi not connected"
		return
	fi

	# Get IP address (no retry loop - screen refreshes on each interaction)
	ip_address="$(ip addr show wlan0 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1)"
	if [ -z "$ip_address" ]; then
		echo "Waiting for IP..."
		return
	fi

	echo "$NETWORK_SCHEME://$ip_address:$NETWORK_PORT"
}

build_settings_json() {
	# Build settings JSON in a single jq call
	is_running="false"
	service_pid=""
	ip_address=""

	if service-is-running; then
		is_running="true"
		service_pid="$(get_service_pid)"
		ip_address="$(get_ip_address)"
	fi

	start_on_boot="false"
	if will_start_on_boot; then
		start_on_boot="true"
	fi

	jq -rM \
		--argjson is_running "$is_running" \
		--argjson start_on_boot "$start_on_boot" \
		--arg pid "$service_pid" \
		--arg ip "$ip_address" \
		'{settings: .settings}
        | if $is_running then .settings[0].selected = 1 else . end
        | if $start_on_boot then .settings[1].selected = 1 else . end
        | if $is_running and $pid != "" then .settings += [{"name": "PID", "options": [$pid], "selected": 0, "features": {"unselectable": true}}] else . end
        | if $is_running and $ip != "" then .settings += [{"name": "Address", "options": [$ip], "selected": 0, "features": {"unselectable": true}}] else . end' \
		"$PAK_DIR/settings.json"
}

main_screen() {
	settings_file="/tmp/${PAK_NAME}-settings.json"
	build_settings_json >"$settings_file"
	shui list --file "$settings_file" --format json --title "$HUMAN_READABLE_NAME" --confirm "Save" --cancel "Back" --item-key "settings" --write-location "/tmp/${PAK_NAME}-output" --write-value state
}

cleanup() {
	rm -f "/tmp/${PAK_NAME}-settings.json"
	rm -f "/tmp/${PAK_NAME}-output"
	shui stop 2>/dev/null || true
}

main() {
	trap "cleanup" EXIT INT TERM HUP QUIT

	if [ "$PLATFORM" = "tg3040" ] && [ -z "$DEVICE" ]; then
		export DEVICE="brick"
		export PLATFORM="tg5040"
	fi

	if ! command -v shui >/dev/null 2>&1; then
		echo "shui not found"
		return 1
	fi

	# Keep device awake while File Server pak is active
	shui auto-sleep off

	if ! command -v jq >/dev/null 2>&1; then
		show_message "jq not found"
		return 1
	fi

	# dufs is in common/bin, already executable
	chmod +x "$PAK_DIR/bin/service-on"
	chmod +x "$PAK_DIR/bin/service-off"
	chmod +x "$PAK_DIR/bin/service-is-running"
	chmod +x "$PAK_DIR/bin/on-boot"

	allowed_platforms="miyoomini my282 my355 rg35xxplus tg5040"
	if ! echo "$allowed_platforms" | grep -q "$PLATFORM"; then
		show_message "$PLATFORM is not a supported platform"
		return 1
	fi

	if [ "$PLATFORM" = "miyoomini" ]; then
		if [ ! -f /customer/app/axp_test ]; then
			show_message "Wifi not supported on non-Plus\nversion of the Miyoo Mini"
			return 1
		fi
	fi

	if [ "$PLATFORM" = "rg35xxplus" ]; then
		RGXX_MODEL="$(strings /mnt/vendor/bin/dmenu.bin | grep ^RG)"
		if [ "$RGXX_MODEL" = "RG28xx" ]; then
			show_message "Wifi not supported on RG28XX"
			return 1
		fi
	fi

	while true; do
		# Capture current state before showing UI
		old_enabled=0
		if service-is-running; then
			old_enabled=1
		fi

		old_start_on_boot=0
		if will_start_on_boot; then
			old_start_on_boot=1
		fi

		# Show settings screen and get user's choices
		main_screen
		exit_code=$?

		# exit codes: 2 = back button, 3 = menu button
		if [ "$exit_code" -ne 0 ]; then
			break
		fi

		# Read new settings from shui output
		enabled="$(jq -rM '.settings[0].selected' "/tmp/${PAK_NAME}-output")"
		start_on_boot="$(jq -rM '.settings[1].selected' "/tmp/${PAK_NAME}-output")"

		if [ "$old_enabled" != "$enabled" ]; then
			if [ "$enabled" = "1" ]; then
				show_progress "Starting..."
				service-on
				if ! wait_for_service 10; then
					show_message "Failed to start server"
				fi
			else
				show_progress "Stopping..."
				service-off
				if ! wait_for_service_to_stop 10; then
					show_message "Failed to stop server"
				fi
			fi
		fi

		# Start on boot changes are instant, no progress needed
		if [ "$old_start_on_boot" != "$start_on_boot" ]; then
			if [ "$start_on_boot" = "1" ]; then
				enable_start_on_boot
			else
				disable_start_on_boot
			fi
		fi
	done
}

main "$@"
