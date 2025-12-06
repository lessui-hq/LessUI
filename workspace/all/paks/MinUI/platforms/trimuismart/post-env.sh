# shellcheck shell=bash
# Sourced by generated launch.sh
# trimuismart post-env hook
# Button config, LED off, kill daemons

# Configure button mapping
echo A,B,X,Y,L,R > /sys/module/gpio_keys_polled/parameters/button_config

# Turn off LEDs
leds_off

# Kill stock daemons
killall -9 MtpDaemon
killall -9 wpa_supplicant
ifconfig wlan0 down
