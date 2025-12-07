#!/bin/sh
# m17 initialization

# Extra paths
export PATH="/usr/bin:$PATH"
export LD_LIBRARY_PATH="/usr/lib:$LD_LIBRARY_PATH"

# CPU/GPU governors (performance mode, no speed control)
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance > /sys/devices/platform/10091000.gpu/devfreq/10091000.gpu/governor

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
		while :; do
			sleep 5
		done
	fi
}

# Start keymon
keymon.elf &
