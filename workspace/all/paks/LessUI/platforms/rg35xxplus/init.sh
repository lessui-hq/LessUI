#!/bin/sh
# rg35xxplus initialization

# CPU governor
systemctl disable ondemand
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor
echo 0 >/sys/class/power_supply/axp2202-battery/work_led

# Detect model
RGXX_MODEL=$(strings /mnt/vendor/bin/dmenu.bin | grep ^RG)
export RGXX_MODEL

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="rg35xxplus"

case "$RGXX_MODEL" in
	RGcubexx)
		export LESSUI_VARIANT="square"
		export LESSUI_DEVICE="rgcubexx"
		;;
	RG34xx*)
		export LESSUI_VARIANT="wide"
		export LESSUI_DEVICE="rg34xx"
		;;
	RG28xx)
		export LESSUI_VARIANT="vga"
		export LESSUI_DEVICE="rg28xx"
		;;
	RG35xxH*)
		export LESSUI_VARIANT="vga"
		export LESSUI_DEVICE="rg35xxh"
		;;
	RG35xxSP*)
		export LESSUI_VARIANT="vga"
		export LESSUI_DEVICE="rg35xxsp"
		;;
	RG40xxH*)
		export LESSUI_VARIANT="vga"
		export LESSUI_DEVICE="rg40xxh"
		;;
	RG40xxV*)
		export LESSUI_VARIANT="vga"
		export LESSUI_DEVICE="rg40xxv"
		;;
	*)
		export LESSUI_VARIANT="vga"
		export LESSUI_DEVICE="rg35xxplus"
		;;
esac

# HDMI export path (used by loop hooks)
export HDMI_EXPORT_PATH="/tmp/hdmi_export.sh"

# HDMI loop hooks
pre_launcher_hook() {
	. "$HDMI_EXPORT_PATH"
}

pre_pak_hook() {
	. "$HDMI_EXPORT_PATH"
}

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
