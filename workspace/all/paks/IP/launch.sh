#!/bin/sh
cd "$(dirname "$0")"

IP=$(ip -4 addr show dev wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
if [ -z "$IP" ]; then
	shui message "WiFi IP Address" \
		--subtext "Not connected to WiFi." --confirm "Done"
else
	shui message "WiFi IP Address" \
		--subtext "$IP" --confirm "Done"
fi
