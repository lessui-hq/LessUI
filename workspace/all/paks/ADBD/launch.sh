#!/bin/sh
# ADBD.pak - Enable ADB over WiFi

PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR" .pak)"
cd "$PAK_DIR" || exit 1

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

# Bail if already running
if pidof adbd >/dev/null 2>&1; then
	$PRESENTER --message "ADB is already running" --timeout 3
	exit 0
fi

# Platform-specific implementation
case "$PLATFORM" in
	miyoomini)
		# Show progress message
		$PRESENTER --message "Enabling WiFi and ADB...\n\nPlease wait up to 20 seconds." --timeout 20 &
		PRESENTER_PID=$!

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

		# Kill progress indicator
		kill "$PRESENTER_PID" 2>/dev/null

		# Verify adbd started and get IP
		if pidof adbd >/dev/null 2>&1; then
			IP=$(ip -4 addr show dev wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
			if [ -n "$IP" ]; then
				$PRESENTER --message "ADB enabled!\n\nConnect via:\nadb connect $IP:5555" --timeout 5
			else
				$PRESENTER --message "ADB enabled!\n\nGet IP from WiFi menu,\nthen: adb connect <ip>:5555" --timeout 5
			fi
		else
			$PRESENTER --message "Failed to start ADB.\n\nCheck $LOGS_PATH/$PAK_NAME.txt" --timeout 5
		fi
		;;
	*)
		$PRESENTER --message "Platform $PLATFORM not supported" --timeout 3
		exit 1
		;;
esac
