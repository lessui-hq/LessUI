# trimuismart initialization

# Extra paths
export PATH="/usr/trimui/bin:$PATH"
export LD_LIBRARY_PATH="/usr/trimui/lib:$LD_LIBRARY_PATH"

# Button configuration
echo A,B,X,Y,L,R > /sys/module/gpio_keys_polled/parameters/button_config

# CPU setup
echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed
CPU_SPEED_PERF=1536000

cpu_restore() {
	echo $CPU_SPEED_PERF > $CPU_PATH
}
cpu_restore

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
		echo 60000 > $CPU_PATH
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
keymon.elf &
