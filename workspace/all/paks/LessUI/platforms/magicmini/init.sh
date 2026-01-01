#!/bin/sh
# magicmini initialization

# CPU/GPU/DMC governors (CPU speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance >/sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/governor
echo performance >/sys/devices/platform/dmc/devfreq/dmc/governor

# Audio setup
amixer cset name='Playback Path' SPK

# Clear framebuffer
cat /dev/zero >/dev/fb0

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
