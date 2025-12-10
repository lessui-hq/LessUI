#!/bin/sh
# magicmini initialization

# CPU/GPU/DMC governors
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance >/sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/governor
echo performance >/sys/devices/platform/dmc/devfreq/dmc/governor

CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
CPU_SPEED_PERF=1608000

cpu_restore() {
	echo $CPU_SPEED_PERF >$CPU_PATH
}
cpu_restore

# SDL audio
export SDL_AUDIODRIVER=alsa
amixer cset name='Playback Path' SPK

# Clear framebuffer
cat /dev/zero >/dev/fb0

# Start keymon
keymon.elf &
