#!/bin/sh
# Simple WiFi toggle for RGB30
# Uses stock firmware utilities (wifictl, get_setting, set_setting)

cd "$(dirname "$0")"

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

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
done < ./wifi.txt

##############

. /etc/profile # NOTE: this nukes MinUI's PATH modifications
PATH=/storage/roms/.system/rgb30/bin:$PATH

CUR_NAME=$(get_setting wifi.ssid)
CUR_PASS=$(get_setting wifi.key)

STATUS=$(cat "/sys/class/net/wlan0/operstate")

disconnect()
{
	echo "disconnect"
	wifictl disable
	$PRESENTER --message "WiFi disconnected" --timeout 2
	STATUS=down
}

connect()
{
	echo "connect"
	wifictl enable &
	DELAY=30
	$PRESENTER --message "Connecting to WiFi...\n\nPlease wait up to 30 seconds." --timeout $DELAY &
	PRESENTER_PID=$!

	for _ in $(seq 1 $DELAY); do
		STATUS=$(cat "/sys/class/net/wlan0/operstate")
		if [ "$STATUS" = "up" ]; then
			break
		fi
		sleep 1
	done

	kill "$PRESENTER_PID" 2>/dev/null

	if [ "$STATUS" = "up" ]; then
		$PRESENTER --message "WiFi connected successfully!" --timeout 2
	else
		$PRESENTER --message "WiFi connection failed" --timeout 3
	fi
}

{
if [ "$WIFI_NAME" != "$CUR_NAME" ] || [ "$WIFI_PASS" != "$CUR_PASS" ]; then
	echo "change"

	if [ "$STATUS" = "up" ]; then
		disconnect
	fi

	$PRESENTER --message "Updating WiFi credentials..." --timeout 2
	set_setting wifi.ssid "$WIFI_NAME"
	set_setting wifi.key "$WIFI_PASS"
fi

echo "toggle"
if [ "$STATUS" = "up" ]; then
	disconnect
else
	connect
fi
} > ./log.txt 2>&1
