#!/bin/sh

if [ -z "$USERDATA_PATH" ]; then
	shui message "Configuration error." \
		--subtext "USERDATA_PATH not set." --confirm "Dismiss"
	exit 1
fi

# Confirm before resetting
if ! shui message "Reset stick calibration?" \
	--subtext "You will need to recalibrate\nthe analog stick afterward." \
	--confirm "Reset" --cancel "Cancel"; then
	exit 0
fi

shui progress "Resetting calibration..." --indeterminate

if ! rm -f "$USERDATA_PATH/mstick.bin" 2>/dev/null; then
	shui message "Failed to reset calibration." \
		--subtext "Check logs for details." --confirm "Dismiss"
	exit 1
fi

shui message "Calibration reset!" \
	--subtext "Move the stick in circles to recalibrate." --confirm "Done"
