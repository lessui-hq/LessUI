#!/bin/sh
# rgb30 initialization

# CPU setup
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
CPU_SPEED_PERF=1992000

cpu_restore() {
	echo $CPU_SPEED_PERF >$CPU_PATH
}
cpu_restore

# Clean up JELOS filesystem check litter
rm -f "$SDCARD_PATH"/FSCK*.REC

# SDL environment for sdl12-compat
export SDL_VIDEODRIVER=kmsdrm
export SDL_AUDIODRIVER=alsa

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
