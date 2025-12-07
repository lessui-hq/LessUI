#!/bin/sh
# zero28 initialization

# Create standard directories
mkdir -p "$BIOS_PATH"
mkdir -p "$SDCARD_PATH/Roms"
mkdir -p "$SAVES_PATH"

# CPU setup
echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed
CPU_SPEED_PERF=1800000

cpu_restore() {
	echo $CPU_SPEED_PERF > $CPU_PATH
}
cpu_restore

# Backlight fix
bl_disable && bl_enable

# Start keymon
keymon.elf &
