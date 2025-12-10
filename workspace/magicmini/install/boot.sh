#!/bin/sh

# SYSTEM/usr/bin/autostart.sh

SDCARD_PATH=/storage/TF2
SYSTEM_FRAG=/.system/magicmini
UPDATE_FRAG=/LessUI.zip
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}
LOG_FILE="$SDCARD_PATH/lessui-install.log"

# Source shared update functions
. "$(dirname "$0")/install/update-functions.sh"

# is there an update available?
if [ -f $UPDATE_PATH ]; then
	if [ ! -d $SYSTEM_PATH ]; then
		ACTION=installing
		ACTION_NOUN="installation"
	else
		ACTION=updating
		ACTION_NOUN="update"
	fi

	# show action
	dd if=/usr/config/minui/$ACTION.bmp of=/dev/fb0 bs=1 skip=54
	echo 0,0 >/sys/class/graphics/fb0/pan

	log_info "Starting LessUI $ACTION_NOUN..."

	# Perform atomic update with automatic rollback
	atomic_system_update "$UPDATE_PATH" "$SDCARD_PATH" "$SYSTEM_PATH" "$LOG_FILE"

	# Run platform-specific install script (note: magicmini uses SYSTEM_PATH/bin, not SYSTEM_PATH/PLATFORM/bin)
	run_platform_install "$SYSTEM_PATH/bin/install.sh" "$LOG_FILE"
fi

LAUNCH_PATH=$SYSTEM_PATH/paks/MinUI.pak/launch.sh
if [ -f $LAUNCH_PATH ]; then
	$LAUNCH_PATH
fi

sync && shutdown now
