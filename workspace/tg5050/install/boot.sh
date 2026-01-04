#!/bin/sh
# NOTE: becomes .tmp_update/tg5050.sh

PLATFORM="tg5050"
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/LessUI.7z"
SYSTEM_PATH="$SDCARD_PATH/.system"
LOG_FILE="$SDCARD_PATH/lessui-install.log"

# Source shared update functions
. "$(dirname "$0")/install/update-functions.sh"

# Remount for write access
mount -o remount,rw,async "$SDCARD_PATH"
mount -o remount,rw,async "/mnt/UDISK"

# Use schedutil governor (A523 dual-cluster)
echo schedutil >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null
echo schedutil >/sys/devices/system/cpu/cpufreq/policy4/scaling_governor 2>/dev/null

# install/update
if [ -f "$UPDATE_PATH" ]; then
	export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
	export PATH=/usr/trimui/bin:$PATH

	# Turn off LEDs
	echo 0 >/sys/class/led_anim/max_scale 2>/dev/null

	cd $(dirname "$0")/$PLATFORM
	if [ -d "$SYSTEM_PATH" ]; then
		ACTION=updating
		ACTION_NOUN="update"
	else
		ACTION=installing
		ACTION_NOUN="installation"
	fi
	./show.elf ./$ACTION.png

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

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/LessUI.pak/launch.sh"
if [ -f "$LAUNCH_PATH" ]; then
	exec "$LAUNCH_PATH"
fi
