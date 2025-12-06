# shellcheck shell=bash
# Sourced by generated launch.sh
# miyoomini post-env hook
# Device detection, audioserver, lumon, batmon

# Detect Mini Plus
if [ -f /customer/app/axp_test ]; then
	IS_PLUS=true
else
	IS_PLUS=false
fi
export IS_PLUS

# Detect model
MY_MODEL=$(strings -n 5 /customer/app/MainUI | grep MY)
export MY_MODEL

# Detect firmware version
MIYOO_VERSION=$(/etc/fw_printenv miyoo_version)
export MIYOO_VERSION=${MIYOO_VERSION#miyoo_version=}

# Export CPU speed variables for minarch
export CPU_SPEED_MENU
export CPU_SPEED_GAME
export CPU_SPEED_PERF

# Clean up update log
rm -f "$SDCARD_PATH/update.log"

# Start audioserver
if $IS_PLUS; then
	/customer/app/audioserver -60 &
	export LD_PRELOAD=/customer/lib/libpadsp.so
else
	if [ -f /customer/lib/libpadsp.so ]; then
		LD_PRELOAD=as_preload.so audioserver.mod &
		export LD_PRELOAD=libpadsp.so
	fi
fi

# LCD luma and saturation adjustment
lumon.elf &

# Battery monitor (only when charging)
if $IS_PLUS; then
	CHARGING=$(/customer/app/axp_test | awk -F'[,: {}]+' '{print $7}')
	if [ "$CHARGING" = "3" ]; then
		batmon.elf
	fi
else
	CHARGING=$(cat /sys/devices/gpiochip0/gpio/gpio59/value)
	if [ "$CHARGING" = "1" ]; then
		batmon.elf
	fi
fi
