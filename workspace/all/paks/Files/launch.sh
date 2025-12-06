#!/bin/sh

cd "$(dirname "$0")"

# Set HOME to SD card path for all file managers
HOME="$SDCARD_PATH"

case "$PLATFORM" in
	rg35xxplus)
		# Use system file manager on rg35xxplus
		DIR="/mnt/vendor/bin/fileM"
		if [ ! -d "$DIR" ]; then
			shui message "File manager not found." \
				--subtext "Update stock firmware from Anbernic." --confirm "Dismiss"
			exit 1
		fi
		if [ ! -f "$DIR/dinguxCommand_en.dge" ]; then
			shui message "File manager binary missing\nor corrupt." --confirm "Dismiss"
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
			shui message "File manager not available\nfor $PLATFORM." --confirm "Dismiss"
			exit 1
		fi
		"$BINARY"
		;;
	*)
		# Use DinguxCommander on all other platforms
		BINARY="./bin/$PLATFORM/DinguxCommander"
		if [ ! -f "$BINARY" ]; then
			shui message "File manager not available\nfor $PLATFORM." --confirm "Dismiss"
			exit 1
		fi
		"$BINARY"
		;;
esac
