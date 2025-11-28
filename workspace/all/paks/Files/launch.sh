#!/bin/sh

cd "$(dirname "$0")"

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

# Set HOME to SD card path for all file managers
HOME="$SDCARD_PATH"

case "$PLATFORM" in
	rg35xxplus)
		# Use system file manager on rg35xxplus
		DIR="/mnt/vendor/bin/fileM"
		if [ ! -d "$DIR" ]; then
			$PRESENTER --message "File manager not found.\n\nUpdate stock firmware from Anbernic." --timeout 5
			exit 1
		fi
		if [ ! -f "$DIR/dinguxCommand_en.dge" ]; then
			$PRESENTER --message "File manager binary missing or corrupt." --timeout 5
			exit 1
		fi
		cd "$DIR" || exit 1
		syncsettings.elf &
		./dinguxCommand_en.dge
		;;
	magicmini)
		# Use 351Files on magicmini
		BINARY="./bin/$PLATFORM/351Files"
		if [ ! -f "$BINARY" ]; then
			$PRESENTER --message "File manager not available for $PLATFORM" --timeout 3
			exit 1
		fi
		"$BINARY"
		;;
	*)
		# Use DinguxCommander on all other platforms
		BINARY="./bin/$PLATFORM/DinguxCommander"
		if [ ! -f "$BINARY" ]; then
			$PRESENTER --message "File manager not available for $PLATFORM" --timeout 3
			exit 1
		fi
		"$BINARY"
		;;
esac
