#!/bin/sh
# rg35xxplus initialization

# CPU governor
systemctl disable ondemand
echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
echo 0 > /sys/class/power_supply/axp2202-battery/work_led

# Detect model
RGXX_MODEL=$(strings /mnt/vendor/bin/dmenu.bin | grep ^RG)
export RGXX_MODEL

case "$RGXX_MODEL" in
	RGcubexx)
		export DEVICE="cube"
		;;
	RG34xx*)
		export DEVICE="wide"
		;;
esac

# HDMI export path (used by loop hooks)
export HDMI_EXPORT_PATH="/tmp/hdmi_export.sh"

# HDMI loop hooks
pre_minui_hook() {
	. "$HDMI_EXPORT_PATH"
}

pre_pak_hook() {
	. "$HDMI_EXPORT_PATH"
}

# Start keymon
keymon.elf &
