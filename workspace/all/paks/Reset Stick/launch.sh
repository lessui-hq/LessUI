#!/bin/sh

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

rm -f "$USERDATA_PATH/mstick.bin"

$PRESENTER "Stick calibration reset.\n\nMove stick to recalibrate." 3
