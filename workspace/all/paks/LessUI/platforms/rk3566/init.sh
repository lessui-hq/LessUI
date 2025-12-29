#!/bin/sh
# RK3566 platform initialization (LessOS)
# Supports: RGB30, RG353P/M/V/VS, and other RK3566 devices

# CPU setup
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
CPU_SPEED_PERF=1992000

cpu_restore() {
	echo "$CPU_SPEED_PERF" >"$CPU_PATH"
}
cpu_restore

# SDL environment
export SDL_AUDIODRIVER=alsa

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
