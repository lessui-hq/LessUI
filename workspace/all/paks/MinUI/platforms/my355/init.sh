#!/bin/sh
# my355 initialization

# Extra paths
export PATH="/usr/miyoo/bin:/usr/miyoo/sbin:$PATH"
export LD_LIBRARY_PATH="/usr/miyoo/lib:$LD_LIBRARY_PATH"

# CPU setup
echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
CPU_SPEED_PERF=1992000

cpu_restore() {
	echo $CPU_SPEED_PERF > $CPU_PATH
}
cpu_restore

# Headphone jack GPIO
echo 150 > /sys/class/gpio/export
printf "%s" in > /sys/class/gpio/gpio150/direction

# Motor GPIO
echo 20 > /sys/class/gpio/export
printf "%s" out > /sys/class/gpio/gpio20/direction
printf "%s" 0 > /sys/class/gpio/gpio20/value

# Keyboard joystick type
echo 0 > /sys/class/miyooio_chr_dev/joy_type

# LED off
echo 0 > /sys/class/leds/work/brightness

# Start input daemon
mkdir -p /tmp/miyoo_inputd
miyoo_inputd &

# Disable system-level lid handling
mv /dev/input/event1 /dev/input/event1.disabled

# Start keymon
keymon.elf > "$LOGS_PATH/keymon.log" 2>&1 &
