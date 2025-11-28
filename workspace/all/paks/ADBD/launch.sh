#!/bin/sh
# ADBD.pak - Enable ADB over WiFi

PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR" .pak)"
cd "$PAK_DIR" || exit 1

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

# Bail if already running
if pidof adbd >/dev/null 2>&1; then
	$PRESENTER "ADB is already running" 3
	exit 0
fi

# Platform-specific implementation
case "$PLATFORM" in
	miyoomini)
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

			# Surgical strike to nop /etc/profile
			# because it brings up the entire system again
			mount -o bind "$PAK_DIR/miyoomini/profile" /etc/profile

			# Launch adbd
			export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$PAK_DIR/miyoomini"
			"$PAK_DIR/miyoomini/adbd" &

			echo "Success"
		} > "$LOGS_PATH/$PAK_NAME.txt" 2>&1

		if grep -q "Success" "$LOGS_PATH/$PAK_NAME.txt"; then
			$PRESENTER "ADB enabled successfully!\n\nConnect via:\nadb connect <device-ip>" 4
		else
			$PRESENTER "Failed to enable ADB.\nCheck log for details." 4
		fi
		;;
	*)
		$PRESENTER "Platform $PLATFORM not supported" 3
		exit 1
		;;
esac
