#!/bin/sh

# NOTE: becomes em_ui.sh

PLATFORM="m17"
SDCARD_PATH="/sdcard"
UPDATE_PATH="$SDCARD_PATH/LessUI.7z"
SYSTEM_PATH="$SDCARD_PATH/.system"
LOG_FILE="$SDCARD_PATH/lessui-install.log"

# Embedded logging (simplified for early boot environment)
# NOTE: Inlined because this script runs from internal storage during early boot,
# before SD card is mounted and .system is accessible. Cannot source shared log.sh.
# Keep format in sync with skeleton/SYSTEM/common/log.sh
log_write() {
	echo "[$1] $2" >>"$LOG_FILE"
}
log_info() { log_write "INFO" "$*"; }
log_error() { log_write "ERROR" "$*"; }

# install/update
if [ -f "$UPDATE_PATH" ]; then
	if [ ! -d $SYSTEM_PATH ]; then
		ACTION=installing
		ACTION_NOUN="installation"
	else
		ACTION=updating
		ACTION_NOUN="update"
	fi

	# initialize fb0
	cat /sys/class/graphics/fb0/modes >/sys/class/graphics/fb0/mode

	# extract the zip file appended to the end of this script to tmp
	CUT=$(($(grep -n '^BINARY' $0 | cut -d ':' -f 1 | tail -1) + 1))
	tail -n +$CUT "$0" | uudecode -o /tmp/data

	# unzip and display one of the two images it contains
	unzip -o /tmp/data -d /tmp
	dd if=/tmp/$ACTION of=/dev/fb0
	sync

	log_info "Starting LessUI $ACTION_NOUN..."
	if $SDCARD_PATH/bin/arm/7z x -y -o"$SDCARD_PATH" "$UPDATE_PATH" >>"$LOG_FILE" 2>&1; then
		log_info "Extraction complete"
	else
		EXIT_CODE=$?
		log_error "7z extraction failed with exit code $EXIT_CODE"
	fi
	rm -f "$UPDATE_PATH"
	sync

	# the updated system finishes the install/update
	if [ -f $SYSTEM_PATH/$PLATFORM/bin/install.sh ]; then
		log_info "Running install.sh..."
		if $SYSTEM_PATH/$PLATFORM/bin/install.sh >>"$LOG_FILE" 2>&1; then
			log_info "Installation complete"
		else
			EXIT_CODE=$?
			log_error "install.sh failed with exit code $EXIT_CODE"
		fi
	fi
	dd if=/dev/zero of=/dev/fb0
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/LessUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ]; do
	taskset 8 "$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card

exit 0
