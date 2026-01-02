#!/bin/sh
# trimuismart initialization

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="trimuismart"
export LESSUI_VARIANT="qvga"
export LESSUI_DEVICE="trimuismart"

# Extra paths (appended so system paths have priority)
export PATH="$PATH:/usr/trimui/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/trimui/lib"

# Button configuration
echo A,B,X,Y,L,R >/sys/module/gpio_keys_polled/parameters/button_config

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed

# LED off
leds_off

# Kill stock daemons
killall -9 MtpDaemon
killall -9 wpa_supplicant
ifconfig wlan0 down

# Datetime restore
if [ -f "$DATETIME_PATH" ]; then
	DATETIME=$(cat "$DATETIME_PATH")
	date +'%F %T' -s "$DATETIME"
	DATETIME=$(date +'%s')
	date -u -s "@$DATETIME"
fi

# Poweroff handler (called from loop_tail_hook)
loop_tail_hook() {
	if [ -f "/tmp/poweroff" ]; then
		rm -f "/tmp/poweroff"
		killall keymon.elf
		shutdown
		echo 60000 >$CPU_PATH
		LED_ON=true
		while :; do
			if $LED_ON; then
				LED_ON=false
				leds_off
			else
				LED_ON=true
				leds_on
			fi
			sleep 0.5
		done
	fi
}

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
