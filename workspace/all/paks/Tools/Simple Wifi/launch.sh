#!/bin/sh
# Simple WiFi toggle for RGB30
# Uses stock firmware utilities (wifictl, get_setting, set_setting)

cd "$(dirname "$0")"

# Read credentials from wifi.txt (line 1 = SSID, line 2 = password)
WIFI_NAME=""
WIFI_PASS=""
LINE_NUM=0
while IFS= read -r line || [ -n "$line" ]; do
	if [ "$LINE_NUM" -eq 0 ]; then
		WIFI_NAME="$line"
	elif [ "$LINE_NUM" -eq 1 ]; then
		WIFI_PASS="$line"
		break
	fi
	LINE_NUM=$((LINE_NUM + 1))
done <./wifi.txt

##############

. /etc/profile # NOTE: this nukes MinUI's PATH modifications
PATH=/storage/roms/.system/rgb30/bin:$PATH

CUR_NAME=$(get_setting wifi.ssid)
CUR_PASS=$(get_setting wifi.key)

STATUS=$(cat "/sys/class/net/wlan0/operstate")

disconnect() {
	shui progress "Disconnecting..." --indeterminate
	wifictl disable
	shui message "WiFi disconnected." --confirm "Done"
	STATUS=down
}

connect() {
	shui progress "Connecting to WiFi..." --indeterminate
	wifictl enable &

	DELAY=30
	for _ in $(seq 1 $DELAY); do
		STATUS=$(cat "/sys/class/net/wlan0/operstate")
		if [ "$STATUS" = "up" ]; then
			break
		fi
		sleep 1
	done

	if [ "$STATUS" = "up" ]; then
		IP=$(ip -4 addr show dev wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
		shui message "WiFi connected!" --subtext "IP: $IP" --confirm "Done"
	else
		shui message "WiFi connection failed." \
			--subtext "Check credentials in wifi.txt" --confirm "Dismiss"
	fi
}

{
	if [ "$WIFI_NAME" != "$CUR_NAME" ] || [ "$WIFI_PASS" != "$CUR_PASS" ]; then
		if [ "$STATUS" = "up" ]; then
			shui progress "Disconnecting..." --indeterminate
			wifictl disable
			STATUS=down
		fi

		shui progress "Updating WiFi credentials..." --indeterminate
		set_setting wifi.ssid "$WIFI_NAME"
		set_setting wifi.key "$WIFI_PASS"
	fi

	if [ "$STATUS" = "up" ]; then
		# Already connected, ask what to do
		if shui message "WiFi is connected." \
			--subtext "What would you like to do?" \
			--confirm "Disconnect" --cancel "Keep"; then
			disconnect
		fi
	else
		# Not connected, ask to connect
		if shui message "WiFi is disconnected." \
			--subtext "Connect to $WIFI_NAME?" \
			--confirm "Connect" --cancel "Cancel"; then
			connect
		fi
	fi
} >./log.txt 2>&1
