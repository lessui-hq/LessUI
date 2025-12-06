#!/bin/sh
# ADBD.pak - Enable ADB over WiFi

PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR" .pak)"
cd "$PAK_DIR" || exit 1

# Check if already running
if pidof adbd >/dev/null 2>&1; then
	IP=$(ip -4 addr show dev wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
	if [ -n "$IP" ]; then
		shui message "ADB is already running.\n\nConnect via:\nadb connect $IP:5555" --confirm "Done"
	else
		shui message "ADB is already running.\n\nGet IP from WiFi settings,\nthen: adb connect <ip>:5555" --confirm "Done"
	fi
	exit 0
fi

# Platform-specific implementation
case "$PLATFORM" in
	miyoomini)
		# Confirm before enabling
		if ! shui message "Enable ADB debugging?\n\nThis will also enable WiFi\nif not already connected." \
			--confirm "Enable" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Enabling WiFi..." --indeterminate

		{
			# Load the WiFi driver from the SD card
			if ! grep -q 8188fu /proc/modules 2>/dev/null; then
				insmod "$PAK_DIR/miyoomini/8188fu.ko"
			fi

			# Enable WiFi hardware
			/customer/app/axp_test wifion
			sleep 2

			# Bring up WiFi interface (configured in stock)
			ifconfig wlan0 up
			wpa_supplicant -B -D nl80211 -iwlan0 -c /appconfigs/wpa_supplicant.conf
			udhcpc -i wlan0 -s /etc/init.d/udhcpc.script &

			# Wait for WiFi connection
			sleep 3

			# Surgical strike to nop /etc/profile
			# because it brings up the entire system again
			mount -o bind "$PAK_DIR/miyoomini/profile" /etc/profile

			# Launch adbd
			export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$PAK_DIR/miyoomini"
			"$PAK_DIR/miyoomini/adbd" &

			# Give adbd time to start
			sleep 1

			echo "Success"
		} > "$LOGS_PATH/$PAK_NAME.txt" 2>&1

		# Verify adbd started and get IP
		if pidof adbd >/dev/null 2>&1; then
			IP=$(ip -4 addr show dev wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
			if [ -n "$IP" ]; then
				shui message "ADB enabled!\n\nConnect via:\nadb connect $IP:5555" --confirm "Done"
			else
				shui message "ADB enabled!\n\nGet IP from WiFi settings,\nthen: adb connect <ip>:5555" --confirm "Done"
			fi
		else
			shui message "Failed to start ADB.\n\nCheck logs for details." --confirm "Dismiss"
		fi
		;;
	*)
		shui message "ADB is not supported on $PLATFORM." --confirm "Dismiss"
		exit 1
		;;
esac
