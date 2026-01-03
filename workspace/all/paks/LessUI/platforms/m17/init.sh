#!/bin/sh
# m17 initialization

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="m17"
export LESSUI_VARIANT="wide"
export LESSUI_DEVICE="m17"

# Extra paths (appended so system paths have priority)
export PATH="$PATH:/usr/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/lib"

# CPU/GPU governors (performance mode, no speed control)
echo performance >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance >/sys/devices/platform/10091000.gpu/devfreq/10091000.gpu/governor

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

# Start ADB daemon
start_adbd.sh

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
