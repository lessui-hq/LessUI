#!/bin/sh

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

if [ -z "$USERDATA_PATH" ]; then
	$PRESENTER --message "Error: USERDATA_PATH not set" --timeout 3
	exit 1
fi

if ! rm -f "$USERDATA_PATH/mstick.bin" 2>/dev/null; then
	$PRESENTER --message "Error: Failed to reset calibration" --timeout 3
	exit 1
fi

$PRESENTER --message "Stick calibration reset.\n\nMove stick to recalibrate." --timeout 3
