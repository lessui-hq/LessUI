#!/bin/sh
# NOTE: becomes .tmp_update/tg5040.sh

PLATFORM="tg5040"
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/LessUI.7z"
SYSTEM_PATH="$SDCARD_PATH/.system"
LOG_FILE="$SDCARD_PATH/lessui-install.log"

# Source shared update functions
. "$(dirname "$0")/install/update-functions.sh"

# for Brick
mount -o remount,rw,async "$SDCARD_PATH"
mount -o remount,rw,async "/mnt/UDISK"

echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed
CPU_SPEED_PERF=2000000
echo $CPU_SPEED_PERF >$CPU_PATH

# install/update
if [ -f "$UPDATE_PATH" ]; then
	export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
	export PATH=/usr/trimui/bin:$PATH

	TRIMUI_MODEL=$(strings /usr/trimui/bin/MainUI | grep ^Trimui)
	if [ "$TRIMUI_MODEL" = "Trimui Smart Pro" ]; then
		DEVICE="smartpro"
	elif [ "$TRIMUI_MODEL" = "Trimui Brick" ]; then
		DEVICE="brick"
	fi

	# leds_off
	echo 0 >/sys/class/led_anim/max_scale
	if [ "$DEVICE" = "brick" ]; then
		echo 0 >/sys/class/led_anim/max_scale_lr
		echo 0 >/sys/class/led_anim/max_scale_f1f2
	fi

	cd $(dirname "$0")/$PLATFORM
	if [ -d "$SYSTEM_PATH" ]; then
		ACTION=updating
		ACTION_NOUN="update"
	else
		ACTION=installing
		ACTION_NOUN="installation"
	fi
	./show.elf ./$DEVICE/$ACTION.png

	log_info "Starting LessUI $ACTION_NOUN..."

	# Perform atomic update with automatic rollback
	atomic_system_update "$UPDATE_PATH" "$SDCARD_PATH" "$SYSTEM_PATH" "$LOG_FILE"
	sync

	# Run platform-specific install script
	run_platform_install "$SYSTEM_PATH/$PLATFORM/bin/install.sh" "$LOG_FILE"

	if [ "$ACTION" = "installing" ]; then
		log_info "Rebooting..."
		reboot
	fi
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
if [ -f "$LAUNCH_PATH" ]; then
	exec "$LAUNCH_PATH"
fi
