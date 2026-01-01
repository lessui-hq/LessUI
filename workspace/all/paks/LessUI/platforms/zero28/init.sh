#!/bin/sh
# zero28 initialization

# Create standard directories
mkdir -p "$BIOS_PATH"
mkdir -p "$SDCARD_PATH/Roms"
mkdir -p "$SAVES_PATH"

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# Backlight fix
bl_disable && bl_enable

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
