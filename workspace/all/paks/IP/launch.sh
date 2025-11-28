#!/bin/sh
cd $(dirname "$0")

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

IP=$(ip -4 addr show dev wlan0 | awk '/inet / {print $2}' | cut -d/ -f1)
if [ -z "$IP" ]; then
	$PRESENTER "WiFi IP Address:\n\nUnassigned" 4
else
	$PRESENTER "WiFi IP Address:\n\n$IP" 4
fi
