# shellcheck shell=bash
# Sourced by generated launch.sh
# rg35xxplus post-env hook
# Model detection, HDMI export, system setup

systemctl disable ondemand
echo 0 > /sys/class/power_supply/axp2202-battery/work_led

# Detect model
RGXX_MODEL=$(strings /mnt/vendor/bin/dmenu.bin | grep ^RG)
export RGXX_MODEL

# Set device type based on model
case "$RGXX_MODEL" in
	RGcubexx)
		export DEVICE="cube"
		;;
	RG34xx*)
		export DEVICE="wide"
		;;
esac

# Export HDMI path for loop hook
export HDMI_EXPORT_PATH="/tmp/hdmi_export.sh"
