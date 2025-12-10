#!/bin/sh
# my282 initialization

# LED off
echo 0 >/sys/class/leds/led1/brightness

# CPU speed control via reclock
cpu_restore() {
	overclock.elf userspace 2 1344 384 1080 0
}
cpu_restore

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
keymon.elf &
