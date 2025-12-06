# shellcheck shell=bash
# Sourced by generated launch.sh
# my282 post-env hook
# LED off, reclock function, wifi kill loop

# LED off
echo 0 > /sys/class/leds/led1/brightness

# Define reclock function for CPU speed control
reclock() {
	overclock.elf userspace 2 1344 384 1080 0
}

# Initial reclock
reclock

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
