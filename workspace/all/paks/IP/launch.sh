#!/bin/sh
cd "$(dirname "$0")"

IP=$(ip -4 addr show dev wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
if [ -z "$IP" ]; then
	shellui message "WiFi IP Address\n\nNot connected to WiFi." --confirm "Done"
else
	shellui message "WiFi IP Address\n\n$IP" --confirm "Done"
fi
