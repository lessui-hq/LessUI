# shellcheck shell=bash
# Sourced by generated launch.sh
# my355 post-env hook
# GPIO setup, input daemon, lid disable

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
