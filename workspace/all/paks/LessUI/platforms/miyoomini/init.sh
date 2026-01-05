#!/bin/sh
# miyoomini initialization

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# Detect hardware features
# HAS_PMIC: AXP223 power management IC (affects audio, battery, volume control)
if [ -f /customer/app/axp_test ]; then
	HAS_PMIC=true
else
	HAS_PMIC=false
fi
export HAS_PMIC

# Detect model from firmware
MY_MODEL=$(strings -n 5 /customer/app/MainUI | grep MY)
export MY_MODEL

# Detect 560p screen mode
if [ -f /sys/class/graphics/fb0/modes ] && grep -q "752x560" /sys/class/graphics/fb0/modes; then
	HAS_560P=true
else
	HAS_560P=false
fi
export HAS_560P

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="miyoomini"

# Device identification based on model and hardware
case "$MY_MODEL" in
	MY285)
		# Miyoo Mini Flip - clamshell form factor, always 560p, has PMIC
		export LESSUI_DEVICE="miyoominiflip"
		export LESSUI_VARIANT="560p"
		;;
	*)
		# Standard Mini or Plus - detect based on hardware
		if $HAS_560P; then
			export LESSUI_VARIANT="560p"
			if $HAS_PMIC; then
				export LESSUI_DEVICE="miyoominiplus560p"
			else
				export LESSUI_DEVICE="miyoomini560p"
			fi
		else
			export LESSUI_VARIANT="vga"
			if $HAS_PMIC; then
				export LESSUI_DEVICE="miyoominiplus"
			else
				export LESSUI_DEVICE="miyoomini"
			fi
		fi
		;;
esac

# Detect firmware version
MIYOO_VERSION=$(/etc/fw_printenv miyoo_version)
export MIYOO_VERSION=${MIYOO_VERSION#miyoo_version=}

# Clean up update log
rm -f "$SDCARD_PATH/update.log"

# Start audioserver (PMIC devices use system audioserver, others use modified version)
if $HAS_PMIC; then
	/customer/app/audioserver -60 &
	export LD_PRELOAD=/customer/lib/libpadsp.so
else
	if [ -f /customer/lib/libpadsp.so ]; then
		LD_PRELOAD=as_preload.so audioserver.mod &
		export LD_PRELOAD=libpadsp.so
	fi
fi

# LCD luma/saturation adjustment
lumon.elf &

# Battery monitor (only when charging) - PMIC uses axp_test, others use GPIO
if $HAS_PMIC; then
	CHARGING=$(/customer/app/axp_test | awk -F'[,: {}]+' '{print $7}')
	[ "$CHARGING" = "3" ] && batmon.elf
else
	CHARGING=$(cat /sys/devices/gpiochip0/gpio/gpio59/value)
	[ "$CHARGING" = "1" ] && batmon.elf
fi

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &

# Datetime restore
if [ -f "$DATETIME_PATH" ] && [ ! -f "$USERDATA_PATH/enable-rtc" ]; then
	DATETIME=$(cat "$DATETIME_PATH")
	date +'%F %T' -s "$DATETIME"
	DATETIME=$(date +'%s')
	date -u -s "@$DATETIME"
fi
