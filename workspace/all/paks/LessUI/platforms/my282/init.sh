#!/bin/sh
# my282 initialization

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="my282"
export LESSUI_VARIANT="vga"
export LESSUI_DEVICE="my282"

# LED off
echo 0 >/sys/class/leds/led1/brightness

# CPU governor (speed controlled by frontend)
overclock.elf userspace 2 1344 384 1080 0

# Clean up tee and update log
killall -9 tee
rm -f "$SDCARD_PATH/update.log"

# WiFi kill loop (background)
while :; do
	killall -9 wpa_supplicant
	killall -9 MtpDaemon
	ifconfig wlan0 down
	sleep 2
done &

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
