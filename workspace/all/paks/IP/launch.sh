#!/bin/sh
cd "$(dirname "$0")"

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

IP=$(ip -4 addr show dev wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
if [ -z "$IP" ]; then
	$PRESENTER --message "WiFi IP Address:\n\nUnassigned" --timeout 4
else
	$PRESENTER --message "WiFi IP Address:\n\n$IP" --timeout 4
fi
