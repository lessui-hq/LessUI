#!/bin/sh
# miyoomini initialization

# CPU governor
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# CPU speeds for minarch
export CPU_SPEED_MENU=504000
export CPU_SPEED_GAME=1296000
export CPU_SPEED_PERF=1488000

cpu_restore() {
	overclock.elf $CPU_SPEED_PERF
}
cpu_restore

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

# LCD luma/saturation adjustment
lumon.elf &

# Battery monitor (only when charging)
if $IS_PLUS; then
	CHARGING=$(/customer/app/axp_test | awk -F'[,: {}]+' '{print $7}')
	[ "$CHARGING" = "3" ] && batmon.elf
else
	CHARGING=$(cat /sys/devices/gpiochip0/gpio/gpio59/value)
	[ "$CHARGING" = "1" ] && batmon.elf
fi

# Start keymon
keymon.elf &

# Datetime restore
if [ -f "$DATETIME_PATH" ] && [ ! -f "$USERDATA_PATH/enable-rtc" ]; then
	DATETIME=$(cat "$DATETIME_PATH")
	date +'%F %T' -s "$DATETIME"
	DATETIME=$(date +'%s')
	date -u -s "@$DATETIME"
fi
