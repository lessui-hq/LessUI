#!/bin/sh

if [ -z "$USERDATA_PATH" ]; then
	shellui message "Configuration error.\n\nUSERDATA_PATH not set." --confirm "Dismiss"
	exit 1
fi

# Confirm before resetting
if ! shellui message "Reset stick calibration?\n\nYou will need to recalibrate\nthe analog stick afterward." \
	--confirm "Reset" --cancel "Cancel"; then
	exit 0
fi

shellui progress "Resetting calibration..." --indeterminate

if ! rm -f "$USERDATA_PATH/mstick.bin" 2>/dev/null; then
	shellui message "Failed to reset calibration.\n\nCheck logs for details." --confirm "Dismiss"
	exit 1
fi

shellui message "Calibration reset!\n\nMove the stick in circles\nto recalibrate." --confirm "Done"
