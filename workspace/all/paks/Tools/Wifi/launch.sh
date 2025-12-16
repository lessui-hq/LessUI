#!/bin/sh
# Wifi.pak - Manage WiFi settings
# Based on Jose Gonzalez's launcher-wifi-pak, adapted for LessUI cross-platform paks

PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR" .pak)"
cd "$PAK_DIR" || exit 1

# Logging
rm -f "$LOGS_PATH/$PAK_NAME.txt"
exec >>"$LOGS_PATH/$PAK_NAME.txt" 2>&1
echo "$0" "$@"

# Setup paths
mkdir -p "$USERDATA_PATH/$PAK_NAME"
export HOME="$USERDATA_PATH/$PAK_NAME"

# Add pak binaries to PATH (platform-specific tools like iw)
export PATH="$PAK_DIR/bin/$PLATFORM:$PAK_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$PAK_DIR/lib/$PLATFORM:$PAK_DIR/lib:$LD_LIBRARY_PATH"

# ============================================================================
# Utility Functions
# ============================================================================

show_message() {
	message="$1"
	echo "$message"
	# Fire-and-forget: shui returns immediately for messages without buttons
	shui message "$message"
}

show_message_wait() {
	message="$1"
	echo "$message"
	# Wait for user to acknowledge
	shui message "$message" --confirm "OK"
}

get_ssid_and_ip() {
	# Check if wlan0 is up
	enabled="$(cat /sys/class/net/wlan0/operstate 2>/dev/null)"
	[ "$enabled" != "up" ] && return

	ssid=""
	ip_address=""

	for _ in 1 2 3 4 5; do
		if [ "$PLATFORM" = "my355" ]; then
			ssid="$(wpa_cli -i wlan0 status 2>/dev/null | grep ssid= | grep -v bssid= | cut -d'=' -f2)"
			ip_address="$(wpa_cli -i wlan0 status 2>/dev/null | grep ip_address= | cut -d'=' -f2)"
		else
			ssid="$(iw dev wlan0 link 2>/dev/null | grep SSID: | cut -d':' -f2- | sed -e 's/^[ \t]*//' -e 's/[ \t]*$//')"
			ip_address="$(ip addr show wlan0 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1)"
		fi

		[ -n "$ip_address" ] && [ -n "$ssid" ] && break
		sleep 1
	done

	[ -z "$ssid" ] && ssid="N/A"
	[ -z "$ip_address" ] && ip_address="N/A"

	printf "%s\t%s" "$ssid" "$ip_address"
}

# ============================================================================
# Boot Configuration
# ============================================================================

will_start_on_boot() {
	grep -q "${PAK_NAME}.pak-on-boot" "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh" 2>/dev/null
}

enable_start_on_boot() {
	if [ ! -f "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh" ]; then
		mkdir -p "$SDCARD_PATH/.userdata/$PLATFORM"
		echo '#!/bin/sh' >"$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
		echo '' >>"$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
	fi

	echo "test -f \"\$SDCARD_PATH/Tools/\$PLATFORM/$PAK_NAME.pak/bin/on-boot\" && \"\$SDCARD_PATH/Tools/\$PLATFORM/$PAK_NAME.pak/bin/on-boot\" # ${PAK_NAME}.pak-on-boot" >>"$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
	chmod +x "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh"
	sync
}

disable_start_on_boot() {
	sed -i "/${PAK_NAME}.pak-on-boot/d" "$SDCARD_PATH/.userdata/$PLATFORM/auto.sh" 2>/dev/null
	sync
}

# ============================================================================
# Credential Management
# ============================================================================

has_credentials() {
	[ ! -s "$SDCARD_PATH/wifi.txt" ] && return 1

	while read -r line; do
		line="$(echo "$line" | xargs)"
		[ -z "$line" ] && continue
		echo "$line" | grep -q "^#" && continue
		echo "$line" | grep -q ":" || continue
		ssid="$(echo "$line" | cut -d: -f1 | xargs)"
		[ -n "$ssid" ] && return 0
	done <"$SDCARD_PATH/wifi.txt"

	return 1
}

# ============================================================================
# Configuration Generation
# ============================================================================

write_config() {
	ENABLING_WIFI="${1:-true}"

	echo "Generating wpa_supplicant.conf"

	# Select platform-specific template
	template_file="$PAK_DIR/res/wpa_supplicant.conf.tmpl"
	if [ -f "$PAK_DIR/res/wpa_supplicant.conf.$PLATFORM.tmpl" ]; then
		template_file="$PAK_DIR/res/wpa_supplicant.conf.$PLATFORM.tmpl"
	fi

	cp "$template_file" "$PAK_DIR/res/wpa_supplicant.conf"

	if [ "$PLATFORM" = "rg35xxplus" ]; then
		echo "Generating netplan.yaml"
		cp "$PAK_DIR/res/netplan.yaml.tmpl" "$PAK_DIR/res/netplan.yaml"
	fi

	# Move wifi.txt from pak to SD card if needed
	if [ ! -f "$SDCARD_PATH/wifi.txt" ] && [ -f "$PAK_DIR/wifi.txt" ]; then
		mv "$PAK_DIR/wifi.txt" "$SDCARD_PATH/wifi.txt"
	fi

	touch "$SDCARD_PATH/wifi.txt"
	sed -i '/^$/d' "$SDCARD_PATH/wifi.txt" 2>/dev/null

	if [ ! -s "$SDCARD_PATH/wifi.txt" ]; then
		echo "No credentials found in wifi.txt"
	fi

	if [ "$ENABLING_WIFI" = "true" ]; then
		has_passwords=false
		priority_used=false
		echo "" >>"$SDCARD_PATH/wifi.txt"

		while read -r line; do
			line="$(echo "$line" | xargs)"
			[ -z "$line" ] && continue
			echo "$line" | grep -q "^#" && continue
			echo "$line" | grep -q ":" || continue

			ssid="$(echo "$line" | cut -d: -f1 | xargs)"
			psk="$(echo "$line" | cut -d: -f2- | xargs)"
			[ -z "$ssid" ] && continue

			has_passwords=true

			{
				echo "network={"
				echo "    ssid=\"$ssid\""
				if [ "$priority_used" = false ]; then
					echo "    priority=1"
					priority_used=true
				fi
				if [ -z "$psk" ]; then
					echo "    key_mgmt=NONE"
				else
					echo "    psk=\"$psk\""
				fi
				echo "}"
			} >>"$PAK_DIR/res/wpa_supplicant.conf"

			if [ "$PLATFORM" = "rg35xxplus" ]; then
				{
					echo "                \"$ssid\":"
					echo "                    password: \"$psk\""
				} >>"$PAK_DIR/res/netplan.yaml"
			fi
		done <"$SDCARD_PATH/wifi.txt"
	fi

	# Copy config to platform-specific locations
	case "$PLATFORM" in
		miyoomini)
			cp "$PAK_DIR/res/wpa_supplicant.conf" /etc/wifi/wpa_supplicant.conf
			cp "$PAK_DIR/res/wpa_supplicant.conf" /appconfigs/wpa_supplicant.conf
			;;
		my282)
			cp "$PAK_DIR/res/wpa_supplicant.conf" /etc/wifi/wpa_supplicant.conf
			cp "$PAK_DIR/res/wpa_supplicant.conf" /config/wpa_supplicant.conf
			;;
		my355)
			cp "$PAK_DIR/res/wpa_supplicant.conf" /userdata/cfg/wpa_supplicant.conf
			;;
		rg35xxplus)
			cp "$PAK_DIR/res/wpa_supplicant.conf" /etc/wpa_supplicant/wpa_supplicant.conf
			cp "$PAK_DIR/res/netplan.yaml" /etc/netplan/01-netcfg.yaml
			if [ "$has_passwords" = false ]; then
				rm -f /etc/netplan/01-netcfg.yaml
			fi
			;;
		tg5040)
			cp "$PAK_DIR/res/wpa_supplicant.conf" /etc/wifi/wpa_supplicant.conf
			;;
		*)
			show_message_wait "$PLATFORM is not a supported platform"
			return 1
			;;
	esac
}

# ============================================================================
# WiFi Control
# ============================================================================

wifi_on() {
	echo "Preparing to toggle wifi on"

	if ! write_config "true"; then
		return 1
	fi

	if ! "$PAK_DIR/bin/service-on"; then
		return 1
	fi

	if ! has_credentials; then
		show_message_wait "No credentials found in wifi.txt"
		return 0
	fi

	# Wait for connection (up to 30 seconds)
	shui progress "Connecting..." --indeterminate
	for _ in $(seq 1 30); do
		STATUS=$(cat "/sys/class/net/wlan0/operstate" 2>/dev/null)
		[ "$STATUS" = "up" ] && break
		sleep 1
	done

	[ "$STATUS" != "up" ] && return 1
	return 0
}

wifi_off() {
	echo "Preparing to toggle wifi off"

	if ! write_config "false"; then
		return 1
	fi

	if ! "$PAK_DIR/bin/service-off"; then
		return 1
	fi
	return 0
}

# ============================================================================
# UI Screens
# ============================================================================

main_screen() {
	launcher_list_file="/tmp/launcher-list"
	rm -f "$launcher_list_file" "/tmp/launcher-output"
	touch "$launcher_list_file"

	template_file="$PAK_DIR/res/settings.json"

	start_on_boot=false
	will_start_on_boot && start_on_boot=true

	enabled=false
	if "$PAK_DIR/bin/wifi-enabled"; then
		enabled=true
		template_file="$PAK_DIR/res/settings.enabled.json"
	fi

	ssid_and_ip="$(get_ssid_and_ip)"
	if [ -n "$ssid_and_ip" ]; then
		ssid="$(echo "$ssid_and_ip" | cut -f1)"
		ip_address="$(echo "$ssid_and_ip" | cut -f2)"
		template_file="$PAK_DIR/res/settings.connected.json"
		if [ "$ip_address" = "N/A" ]; then
			template_file="$PAK_DIR/res/settings.no-ip.json"
		fi
	fi

	cp "$template_file" "$launcher_list_file"

	if [ "$enabled" = true ]; then
		sed -i "s/IS_ENABLED/1/" "$launcher_list_file"
	else
		sed -i "s/IS_ENABLED/0/" "$launcher_list_file"
	fi

	if [ "$start_on_boot" = true ]; then
		sed -i "s/IS_START_ON_BOOT/1/" "$launcher_list_file"
	else
		sed -i "s/IS_START_ON_BOOT/0/" "$launcher_list_file"
	fi

	sed -i "s/NETWORK_SSID/$ssid/" "$launcher_list_file"
	sed -i "s/NETWORK_IP_ADDRESS/$ip_address/" "$launcher_list_file"

	shui list --item-key settings --file "$launcher_list_file" --format json --confirm "Save" --cancel "Exit" --title "Wifi Configuration" --write-location /tmp/launcher-output --write-value state
}

networks_screen() {
	launcher_list_file="/tmp/launcher-list"
	rm -f "$launcher_list_file" "/tmp/launcher-output"
	touch "$launcher_list_file"

	DELAY=30

	if [ "$PLATFORM" = "my355" ]; then
		wpa_cli -i wlan0 scan
		for _ in $(seq 1 "$DELAY"); do
			shui progress "Scanning for networks..." --indeterminate
			wpa_cli -i wlan0 scan_results | grep -v "ssid" | cut -f 5 | sort -u >>"$launcher_list_file"
			[ -s "$launcher_list_file" ] && break
			sleep 1
		done
	else
		for _ in $(seq 1 "$DELAY"); do
			shui progress "Scanning for networks..." --indeterminate
			iw dev wlan0 scan 2>/dev/null | grep SSID: | cut -d':' -f2- | sed -e 's/^[ \t]*//' -e 's/[ \t]*$//' | sort -u >>"$launcher_list_file"
			[ -s "$launcher_list_file" ] && break
			sleep 1
		done
	fi

	shui list --file "$launcher_list_file" --format text --confirm "Connect" --title "Wifi Networks" --write-location /tmp/launcher-output
}

saved_networks_screen() {
	launcher_list_file="/tmp/launcher-list"
	rm -f "$launcher_list_file" "/tmp/launcher-output"
	touch "$launcher_list_file"

	if [ ! -f "$SDCARD_PATH/wifi.txt" ]; then
		show_message_wait "No wifi.txt file found"
		return 1
	fi

	sed '/^#/d; /^$/d; s/:.*//' "$SDCARD_PATH/wifi.txt" >"$launcher_list_file"

	if [ ! -s "$launcher_list_file" ]; then
		show_message_wait "No saved networks found"
		return 1
	fi

	shui list --file "$launcher_list_file" --format text --title "Saved Networks" --confirm "Forget" --write-location /tmp/launcher-output
}

password_screen() {
	SSID="$1"

	rm -f "/tmp/launcher-output"
	touch "$SDCARD_PATH/wifi.txt"

	initial_password=""
	if grep -q "^$SSID:" "$SDCARD_PATH/wifi.txt" 2>/dev/null; then
		initial_password="$(grep "^$SSID:" "$SDCARD_PATH/wifi.txt" | cut -d':' -f2- | xargs)"
	fi

	shui keyboard --title "Enter Password" --initial-value "$initial_password" --write-location /tmp/launcher-output
	exit_code=$?

	[ "$exit_code" -eq 2 ] && return 2
	[ "$exit_code" -eq 3 ] && return 3

	if [ "$exit_code" -ne 0 ]; then
		show_message_wait "Error entering password"
		return 1
	fi

	password="$(cat /tmp/launcher-output)"
	# Allow empty passwords for open networks

	touch "$SDCARD_PATH/wifi.txt"

	if grep -q "^$SSID:" "$SDCARD_PATH/wifi.txt" 2>/dev/null; then
		sed -i "/^$SSID:/d" "$SDCARD_PATH/wifi.txt"
	fi

	echo "$SSID:$password" >"$SDCARD_PATH/wifi.txt.tmp"
	cat "$SDCARD_PATH/wifi.txt" >>"$SDCARD_PATH/wifi.txt.tmp"
	mv "$SDCARD_PATH/wifi.txt.tmp" "$SDCARD_PATH/wifi.txt"
	return 0
}

# ============================================================================
# UI Loops
# ============================================================================

forget_network_loop() {
	next_screen="main"
	while true; do
		saved_networks_screen
		exit_code=$?

		[ "$exit_code" -eq 2 ] && break
		if [ "$exit_code" -eq 3 ]; then
			next_screen="exit"
			break
		fi
		if [ "$exit_code" -ne 0 ]; then
			next_screen="main"
			break
		fi

		SSID="$(cat /tmp/launcher-output)"
		# Use grep -Fv for literal matching to avoid SSID injection attacks
		grep -Fv "$SSID:" "$SDCARD_PATH/wifi.txt" >/tmp/wifi.txt.tmp \
			&& mv /tmp/wifi.txt.tmp "$SDCARD_PATH/wifi.txt"

		shui progress "Updating config..." --indeterminate
		if ! write_config "true"; then
			show_message_wait "Failed to write wireless config"
			break
		fi

		shui progress "Disconnecting..." --indeterminate
		if ! wifi_off; then
			show_message_wait "Failed to disable wifi"
			break
		fi

		# wifi_on shows its own connection progress
		if ! wifi_on; then
			show_message_wait "Failed to enable wifi"
			break
		fi
		break
	done

	echo "$next_screen" >/tmp/wifi-next-screen
}

network_loop() {
	if ! "$PAK_DIR/bin/wifi-enabled"; then
		shui progress "Enabling wifi..." --indeterminate
		if ! "$PAK_DIR/bin/service-on"; then
			show_message_wait "Failed to enable wifi"
			return 1
		fi
	fi

	next_screen="main"
	while true; do
		networks_screen
		exit_code=$?

		[ "$exit_code" -eq 2 ] && break
		if [ "$exit_code" -eq 3 ]; then
			next_screen="exit"
			break
		fi
		if [ "$exit_code" -ne 0 ]; then
			show_message_wait "Error selecting a network"
			next_screen="main"
			break
		fi

		SSID="$(cat /tmp/launcher-output)"
		password_screen "$SSID"
		exit_code=$?

		[ "$exit_code" -eq 2 ] && continue
		if [ "$exit_code" -eq 3 ]; then
			next_screen="exit"
			break
		fi
		[ "$exit_code" -ne 0 ] && continue

		# wifi_on shows connection progress
		if ! wifi_on; then
			show_message_wait "Failed to start wifi"
			break
		fi

		break
	done

	echo "$next_screen" >/tmp/wifi-next-screen
}

# ============================================================================
# Cleanup
# ============================================================================

cleanup() {
	rm -f /tmp/stay_awake /tmp/wifi-next-screen
	shui stop 2>/dev/null || true
}

# ============================================================================
# Main
# ============================================================================

main() {
	echo "1" >/tmp/stay_awake
	trap "cleanup" EXIT INT TERM HUP QUIT

	# Handle platform aliases
	if [ "$PLATFORM" = "tg3040" ] && [ -z "$DEVICE" ]; then
		export DEVICE="brick"
		export PLATFORM="tg5040"
	fi

	if [ "$PLATFORM" = "miyoomini" ] && [ -z "$DEVICE" ]; then
		export DEVICE="miyoomini"
		if [ -f /customer/app/axp_test ]; then
			export DEVICE="miyoominiplus"
		fi
	fi

	# Check required tools
	if ! command -v shui >/dev/null 2>&1; then
		echo "shui not found"
		return 1
	fi

	# Platform validation
	case "$PLATFORM" in
		miyoomini | my282 | my355 | tg5040 | rg35xxplus) ;;
		*)
			show_message_wait "$PLATFORM is not a supported platform"
			return 1
			;;
	esac

	# Platform-specific checks
	if [ "$PLATFORM" = "miyoomini" ]; then
		if [ ! -f /customer/app/axp_test ]; then
			show_message_wait "Wifi not supported on non-Plus version"
			return 1
		fi

		# Load WiFi kernel module
		if ! grep -q 8188fu /proc/modules 2>/dev/null; then
			insmod "$PAK_DIR/res/miyoomini/8188fu.ko"
		fi
	fi

	if [ "$PLATFORM" = "rg35xxplus" ]; then
		RGXX_MODEL="$(strings /mnt/vendor/bin/dmenu.bin 2>/dev/null | grep ^RG)"
		if [ "$RGXX_MODEL" = "RG28xx" ]; then
			show_message_wait "Wifi not supported on RG28XX"
			return 1
		fi
	fi

	# Main UI loop
	while true; do
		main_screen
		exit_code=$?

		# Exit on back or menu button
		[ "$exit_code" -ne 0 ] && break

		output="$(cat /tmp/launcher-output)"
		selected_index="$(echo "$output" | jq -r '.selected')"
		selection="$(echo "$output" | jq -r ".settings[$selected_index].name")"

		if [ "$selection" = "Enable" ] || [ "$selection" = "Start on boot" ]; then
			# Handle Enable toggle
			selected_option_index="$(echo "$output" | jq -r ".settings[0].selected")"
			selected_option="$(echo "$output" | jq -r ".settings[0].options[$selected_option_index]")"

			if [ "$selected_option" = "On" ]; then
				if ! "$PAK_DIR/bin/wifi-enabled"; then
					# wifi_on shows connection progress
					if ! wifi_on; then
						show_message_wait "Failed to enable wifi"
						continue
					fi
				fi
			else
				if "$PAK_DIR/bin/wifi-enabled"; then
					shui progress "Disabling wifi..." --indeterminate
					if ! wifi_off; then
						show_message_wait "Failed to disable wifi"
						continue
					fi
				fi
			fi

			# Handle Start on boot toggle
			selected_option_index="$(echo "$output" | jq -r ".settings[1].selected")"
			selected_option="$(echo "$output" | jq -r ".settings[1].options[$selected_option_index]")"

			if [ "$selected_option" = "On" ]; then
				if ! will_start_on_boot; then
					shui progress "Enabling start on boot..." --indeterminate
					if ! enable_start_on_boot; then
						show_message_wait "Failed to enable start on boot"
						continue
					fi
				fi
			else
				if will_start_on_boot; then
					shui progress "Disabling start on boot..." --indeterminate
					if ! disable_start_on_boot; then
						show_message_wait "Failed to disable start on boot"
						continue
					fi
				fi
			fi
		elif echo "$selection" | grep -q "^Connect to network$"; then
			network_loop
			next_screen="$(cat /tmp/wifi-next-screen)"
			[ "$next_screen" = "exit" ] && break
		elif echo "$selection" | grep -q "^Forget a network$"; then
			forget_network_loop
			next_screen="$(cat /tmp/wifi-next-screen)"
			[ "$next_screen" = "exit" ] && break
		elif echo "$selection" | grep -q "^Refresh connection$"; then
			shui progress "Disconnecting..." --indeterminate
			if ! wifi_off; then
				show_message_wait "Failed to stop wifi"
				return 1
			fi

			shui progress "Updating config..." --indeterminate
			if ! write_config "true"; then
				show_message_wait "Failed to write config"
			fi

			shui progress "Reconnecting..." --indeterminate
			if ! "$PAK_DIR/bin/service-on"; then
				show_message_wait "Failed to enable wifi"
				continue
			fi
		fi
	done
}

main "$@"
