#!/bin/sh

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

HOME="$USERDATA_PATH/splore"
mkdir -p "$HOME"

# cleanup beta puke
if [ -d "$USERDATA_PATH/carts" ]; then
	cd "$USERDATA_PATH" || exit 1
	mv activity_log.txt "$HOME" 2>/dev/null
	mv backup "$HOME" 2>/dev/null
	mv bbs "$HOME" 2>/dev/null
	mv carts "$HOME" 2>/dev/null
	mv cdata "$HOME" 2>/dev/null
	mv config.txt "$HOME" 2>/dev/null
	mv cstore "$HOME" 2>/dev/null
	mv log.txt "$HOME" 2>/dev/null
	mv plates "$HOME" 2>/dev/null
	mv sdl_controllers.txt "$HOME" 2>/dev/null
fi

DIR="$(dirname "$0")"
cd "$DIR" || exit 1

if [ ! -d ./pico-8 ]; then
	# Find PICO-8 zip file
	PICO8_ZIP=""
	for zip in ./pico-8*raspi.zip; do
		if [ -f "$zip" ]; then
			PICO8_ZIP="$zip"
			break
		fi
	done

	if [ -n "$PICO8_ZIP" ]; then
		$PRESENTER --message "Extracting PICO-8..." --timeout 60 &
		PRESENTER_PID=$!

		if ! unzip -o "$PICO8_ZIP" -d ./ >> ./log.txt 2>&1; then
			kill "$PRESENTER_PID" 2>/dev/null
			$PRESENTER --message "Failed to extract PICO-8.\n\nCheck log.txt for details." --timeout 4
			exit 1
		fi

		cp ./sdl_controllers.txt ./pico-8
		kill "$PRESENTER_PID" 2>/dev/null
	else
		$PRESENTER --message "PICO-8 zip file not found!\n\nPlace pico-8*raspi.zip in:\n$DIR" --timeout 4
		exit 1
	fi
fi

# Verify PICO-8 directory and binary exist
if [ ! -d ./pico-8 ]; then
	$PRESENTER --message "PICO-8 directory missing!\n\nExtraction may have failed." --timeout 4
	exit 1
fi

if [ ! -f ./pico-8/pico8_64 ]; then
	$PRESENTER --message "PICO-8 binary not found!\n\nCheck pico-8 directory." --timeout 4
	exit 1
fi

# ensure correct sdl controller file is in place
cmp -s "$DIR/sdl_controllers.txt" "$DIR/pico-8/sdl_controllers.txt"
if [ "$?" -eq 1 ]; then
	cp ./sdl_controllers.txt ./pico-8
fi

cd ./pico-8 && ./pico8_64 -splore -pixel_perfect 1 -joystick 0
