#!/bin/sh
# NOTE: becomes .tmp_update/my282.sh

PLATFORM="my282"
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/LessUI.7z"
SYSTEM_PATH="$SDCARD_PATH/.system"
LOG_FILE="$SDCARD_PATH/lessui-install.log"

# Source shared update functions
. "$(dirname "$0")/install/update-functions.sh"

CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance >"$CPU_PATH"

# install/update
if [ -f "$UPDATE_PATH" ]; then
	cd $(dirname "$0")/$PLATFORM

	if [ -d "$SYSTEM_PATH" ]; then
		ACTION="update"
		./show.elf ./updating.png
	else
		ACTION="installation"
		./show.elf ./installing.png
	fi

	log_info "Starting LessUI $ACTION..."

	# Perform atomic update with automatic rollback
	atomic_system_update "$UPDATE_PATH" "$SDCARD_PATH" "$SYSTEM_PATH" "$LOG_FILE"

	# Run platform-specific install script
	run_platform_install "$SYSTEM_PATH/$PLATFORM/bin/install.sh" "$LOG_FILE"
fi

# or launch (and keep launched)
LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/LessUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ]; do
	"$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card
