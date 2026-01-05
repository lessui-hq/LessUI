#!/bin/sh

# Cross-platform boot logo flasher
# Uses platform-specific logic to flash custom boot logos to device firmware

DIR="$(dirname "$0")"
cd "$DIR"

# Platform-specific implementations
case "$PLATFORM" in
	miyoomini)
		# Check for custom logo
		if [ ! -f ./logo.jpg ]; then
			shui message "No custom logo found." \
				--subtext "Place logo.jpg in the pak folder\nto use your own boot logo." --confirm "Continue"
		fi

		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This modifies device firmware." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Checking firmware..." --value 0

		SUPPORTED_VERSION="202304280000"
		if [ "$MIYOO_VERSION" -gt "$SUPPORTED_VERSION" ]; then
			shui message "Unsupported firmware version." \
				--subtext "Your device is unchanged." --confirm "Dismiss"
			exit 1
		fi

		shui progress "Reading current logo..." --value 20
		./bin/logoread.elf

		if [ -f ./logo.jpg ]; then
			cp ./logo.jpg ./image1.jpg
		else
			cp "$SYSTEM_PATH/dat/image1.jpg" ./
		fi

		shui progress "Preparing new logo..." --value 40

		if ! ./bin/logomake.elf; then
			shui message "Failed to prepare boot logo." \
				--subtext "Your device is unchanged." --confirm "Dismiss"
			exit 1
		fi

		shui progress "Flashing to device..." --value 70

		if ! ./bin/logowrite.elf; then
			shui message "Failed to flash boot logo." \
				--subtext "Check logs for details." --confirm "Dismiss"
			exit 1
		fi

		shui progress "Cleaning up..." --value 90

		rm -f image1.jpg image2.jpg image3.jpg logo.img

		shui progress "Complete!" --value 100
		sleep 0.5

		shui message "Boot logo flashed!" --confirm "Done"

		# Self-destruct (prevent re-running)
		mv "$DIR" "$DIR.disabled"
		;;

	trimuismart)
		LOGO_PATH=./logo.bmp

		if [ ! -f $LOGO_PATH ]; then
			LOGO_PATH=$SYSTEM_PATH/dat/logo.bmp
		fi

		if [ ! -f $LOGO_PATH ]; then
			shui message "No logo.bmp available." \
				--subtext "Place logo.bmp in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This modifies device firmware." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Flashing boot logo..." --indeterminate

		if dd if=$LOGO_PATH of=/dev/by-name/bootlogo bs=65536; then
			shui message "Boot logo flashed!" --confirm "Done"
		else
			shui message "Failed to flash boot logo." \
				--subtext "Check logs for details." --confirm "Dismiss"
		fi

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	my282)
		LOGO_NAME="bootlogo.bmp"
		LOGO_PATH=$DIR/$LOGO_NAME

		if [ ! -f $LOGO_PATH ]; then
			shui message "Missing bootlogo.bmp file!" \
				--subtext "Place bootlogo.bmp in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This modifies device firmware." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		overclock.elf performance 2 1200 384 1080 0

		cp /dev/mtdblock0 boot0

		VERSION=$(cat /usr/miyoo/version)
		OFFSET_PATH="res/offset-$VERSION"

		if [ ! -f "$OFFSET_PATH" ]; then
			shui message "Unsupported firmware version." \
				--subtext "Your device is unchanged." --confirm "Dismiss"
			exit 1
		fi

		OFFSET=$(cat "$OFFSET_PATH")

		shui progress "Compressing logo..." --value 20

		gzip -k "$LOGO_PATH"
		LOGO_PATH=$LOGO_PATH.gz
		LOGO_SIZE=$(wc -c <"$LOGO_PATH")

		MAX_SIZE=62234
		if [ "$LOGO_SIZE" -gt "$MAX_SIZE" ]; then
			shui message "Logo image too complex." \
				--subtext "Simplify the image and try again." --confirm "Dismiss"
			rm -f "$LOGO_PATH"
			exit 1
		fi

		shui progress "Preparing firmware..." --value 50

		# Workaround for missing conv=notrunc support
		OFFSET_PART=$((OFFSET + LOGO_SIZE))
		dd if=boot0 of=boot0-suffix bs=1 skip=$OFFSET_PART 2>/dev/null
		dd if=$LOGO_PATH of=boot0 bs=1 seek=$OFFSET 2>/dev/null
		dd if=boot0-suffix of=boot0 bs=1 seek=$OFFSET_PART 2>/dev/null

		shui progress "Flashing to device..." --value 80

		mtd write "$DIR/boot0" boot

		shui progress "Complete!" --value 100
		sleep 0.5

		rm -f $LOGO_PATH boot0 boot0-suffix

		shui message "Boot logo flashed!" --confirm "Done"

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	my355)
		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This modifies device firmware\nand will reboot when done." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		cd "$DIR"
		./apply.sh &
		APPLY_PID=$!

		# Show progress messages based on timing
		shui progress "Preparing environment..." --value 10
		sleep 2
		if ps | grep -q $APPLY_PID; then
			shui progress "Extracting boot image..." --value 30
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			shui progress "Unpacking resources..." --value 50
		fi
		sleep 4
		if ps | grep -q $APPLY_PID; then
			shui progress "Replacing logo..." --value 65
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			shui progress "Repacking boot image..." --value 80
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			shui progress "Flashing to device..." --value 95
		fi
		sleep 5

		wait $APPLY_PID

		shui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	tg5040)
		# Detect device variant (brick uses different bootlogo path)
		if [ "$LESSUI_DEVICE" = "brick" ]; then
			LOGO_PATH="$DIR/brick/bootlogo.bmp"
		else
			LOGO_PATH="$DIR/tg5040/bootlogo.bmp"
		fi

		if [ ! -f "$LOGO_PATH" ]; then
			shui message "No bootlogo.bmp file found!" \
				--subtext "Place it in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This will copy the logo and reboot." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Mounting boot partition..." --value 20

		BOOT_PATH=/mnt/boot/
		mkdir -p "$BOOT_PATH"
		if ! mount -t vfat /dev/mmcblk0p1 "$BOOT_PATH"; then
			shui message "Failed to mount boot partition." --confirm "Dismiss"
			exit 1
		fi

		shui progress "Copying boot logo..." --value 60

		if ! cp "$LOGO_PATH" "$BOOT_PATH/bootlogo.bmp"; then
			umount "$BOOT_PATH" 2>/dev/null
			shui message "Failed to copy boot logo." --confirm "Dismiss"
			exit 1
		fi
		sync

		shui progress "Unmounting..." --value 90

		umount "$BOOT_PATH"

		shui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct before reboot
		mv "$DIR" "$DIR.disabled"
		rm -f /tmp/launcher_exec
		shui message "Boot logo flashed!" --confirm "Reboot"
		reboot
		;;

	tg5050)
		# tg5050 - single device, no variants
		LOGO_PATH="$DIR/tg5050/bootlogo.bmp"

		if [ ! -f "$LOGO_PATH" ]; then
			shui message "No bootlogo.bmp file found!" \
				--subtext "Place it in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This will copy the logo and reboot." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Mounting boot partition..." --value 20

		BOOT_PATH=/mnt/boot/
		mkdir -p "$BOOT_PATH"
		if ! mount -t vfat /dev/mmcblk0p1 "$BOOT_PATH"; then
			shui message "Failed to mount boot partition." --confirm "Dismiss"
			exit 1
		fi

		shui progress "Copying boot logo..." --value 60

		if ! cp "$LOGO_PATH" "$BOOT_PATH/bootlogo.bmp"; then
			umount "$BOOT_PATH" 2>/dev/null
			shui message "Failed to copy boot logo." --confirm "Dismiss"
			exit 1
		fi
		sync

		shui progress "Unmounting..." --value 90

		umount "$BOOT_PATH"

		shui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct before reboot
		mv "$DIR" "$DIR.disabled"
		rm -f /tmp/launcher_exec
		shui message "Boot logo flashed!" --confirm "Reboot"
		reboot
		;;

	zero28)
		BOOT_DEV=/dev/mmcblk0p1
		BOOT_PATH=/mnt/boot

		if [ ! -f bootlogo.bmp ]; then
			shui message "No bootlogo.bmp file found!" \
				--subtext "Place it in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This will copy the logo and reboot." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Mounting boot partition..." --value 20

		mkdir -p $BOOT_PATH
		mount -t vfat $BOOT_DEV $BOOT_PATH

		shui progress "Copying boot logo..." --value 60

		cp bootlogo.bmp $BOOT_PATH

		shui progress "Unmounting..." --value 90

		umount $BOOT_PATH
		rm -rf $BOOT_PATH

		shui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct before reboot
		mv "$DIR" "$DIR.disabled"

		shui message "Boot logo flashed!" --confirm "Reboot"
		reboot
		;;

	m17)
		LOGO_PATH=$DIR/logo.bmp

		if [ ! -f $LOGO_PATH ]; then
			shui message "No logo.bmp file found!" \
				--subtext "Place it in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shui message "Flash boot logo to device?" \
			--subtext "This modifies device firmware." \
			--confirm "Flash" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Reading logo size..." --value 10

		# Read new bitmap size
		HEX=$(dd if=$LOGO_PATH bs=1 skip=2 count=4 status=none | xxd -g4 -p)
		BYTE0=$(printf "%s" "$HEX" | dd bs=1 skip=0 count=2 2>/dev/null)
		BYTE1=$(printf "%s" "$HEX" | dd bs=1 skip=2 count=2 2>/dev/null)
		BYTE2=$(printf "%s" "$HEX" | dd bs=1 skip=4 count=2 2>/dev/null)
		BYTE3=$(printf "%s" "$HEX" | dd bs=1 skip=6 count=2 2>/dev/null)
		COUNT=$((0x${BYTE3}${BYTE2}${BYTE1}${BYTE0}))
		if [ $COUNT -gt 32768 ]; then
			shui message "Logo file too large." \
				--subtext "Maximum size is 32KB." --confirm "Dismiss"
			exit 1
		fi

		shui progress "Detecting boot partition..." --value 25

		# Detect boot partition revision
		OFFSET=4044800 # rev A
		SIGNATURE=$(dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none)

		if [ "$SIGNATURE" = "BM" ]; then
			echo "Rev A"
		else
			OFFSET=4045312 # rev B
			SIGNATURE=$(dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none)
			if [ "$SIGNATURE" = "BM" ]; then
				echo "Rev B"
			else
				OFFSET=4046848 # rev C
				SIGNATURE=$(dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none)
				if [ "$SIGNATURE" = "BM" ]; then
					echo "Rev C"
				else
					shui message "Unknown boot partition format." \
						--subtext "Your device is unchanged." --confirm "Dismiss"
					exit 1
				fi
			fi
		fi

		shui progress "Creating backup..." --value 50

		# Create backup
		DT=$(date +'%Y%m%d%H%M%S')
		HEX=$(dd if=/dev/block/by-name/boot bs=1 skip=$(($OFFSET + 2)) count=4 status=none | xxd -g4 -p)
		BYTE0=$(printf "%s" "$HEX" | dd bs=1 skip=0 count=2 2>/dev/null)
		BYTE1=$(printf "%s" "$HEX" | dd bs=1 skip=2 count=2 2>/dev/null)
		BYTE2=$(printf "%s" "$HEX" | dd bs=1 skip=4 count=2 2>/dev/null)
		BYTE3=$(printf "%s" "$HEX" | dd bs=1 skip=6 count=2 2>/dev/null)
		COUNT=$((0x${BYTE3}${BYTE2}${BYTE1}${BYTE0}))
		echo "copying $COUNT bytes from $OFFSET to backup-$DT.bmp"
		dd if=/dev/block/by-name/boot of=./backup-$DT.bmp bs=1 skip=$OFFSET count=$COUNT

		shui progress "Flashing new logo..." --value 75

		# Inject new logo
		echo "injecting $LOGO_PATH"
		if ! dd conv=notrunc if=$LOGO_PATH of=/dev/block/by-name/boot bs=1 seek=$OFFSET; then
			shui message "Failed to flash boot logo." \
				--subtext "Check logs for details." --confirm "Dismiss"
			exit 1
		fi

		sync

		shui progress "Complete!" --value 100
		sleep 0.5

		shui message "Boot logo flashed!" \
			--subtext "A backup was saved to the pak folder." --confirm "Done"

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	*)
		shui message "Boot logo flashing is not supported on $PLATFORM." --confirm "Dismiss"
		exit 1
		;;
esac
