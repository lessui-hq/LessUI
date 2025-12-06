# shellcheck shell=bash
# Sourced by generated launch.sh
# my355 daemons hook
# Start keymon

keymon.elf > "$LOGS_PATH/keymon.log" 2>&1 &
