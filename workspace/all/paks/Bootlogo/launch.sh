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
			shellui message "No custom logo found.\n\nPlace logo.jpg in the pak folder\nto use your own boot logo." --confirm "Use Default"
		fi

		# Confirm before flashing
		if ! shellui message "Flash boot logo to device?\n\nThis modifies device firmware." \
			--confirm "Flash Logo" --cancel "Cancel"; then
			exit 0
		fi

		{
			shellui progress "Checking firmware..." --value 0

			SUPPORTED_VERSION="202304280000"
			if [ "$MIYOO_VERSION" -gt "$SUPPORTED_VERSION" ]; then
				echo "Unknown firmware version. Aborted."
				shellui message "Unsupported firmware version.\n\nYour device is unchanged." --confirm "Dismiss"
				exit 1
			fi

			shellui progress "Reading current logo..." --value 20
			./logoread.elf

			if [ -f ./logo.jpg ]; then
				cp ./logo.jpg ./image1.jpg
			else
				cp "$SYSTEM_PATH/dat/image1.jpg" ./
			fi

			shellui progress "Preparing new logo..." --value 40

			if ! ./logomake.elf; then
				echo "Preparing bootlogo failed. Aborted."
				shellui message "Failed to prepare boot logo.\n\nYour device is unchanged." --confirm "Dismiss"
				exit 1
			fi

			shellui progress "Flashing to device..." --value 70

			if ! ./logowrite.elf; then
				echo "Flashing bootlogo failed. Aborted."
				shellui message "Failed to flash boot logo.\n\nCheck logs for details." --confirm "Dismiss"
				exit 1
			fi

			shellui progress "Cleaning up..." --value 90

			rm -f image1.jpg image2.jpg image3.jpg logo.img

			shellui progress "Complete!" --value 100
			sleep 0.5

			echo "Done."
		} > ./log.txt 2>&1

		shellui message "Boot logo flashed!" --confirm "Done"

		# Self-destruct (prevent re-running)
		mv "$DIR" "$DIR.disabled"
		;;

	trimuismart)
		LOGO_PATH=./logo.bmp

		if [ ! -f $LOGO_PATH ]; then
			LOGO_PATH=$SYSTEM_PATH/dat/logo.bmp
		fi

		if [ ! -f $LOGO_PATH ]; then
			shellui message "No logo.bmp available.\n\nPlace logo.bmp in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shellui message "Flash boot logo to device?\n\nThis modifies device firmware." \
			--confirm "Flash Logo" --cancel "Cancel"; then
			exit 0
		fi

		{
			shellui progress "Flashing boot logo..." --indeterminate
			dd if=$LOGO_PATH of=/dev/by-name/bootlogo bs=65536
			echo "Done."
		} > ./log.txt 2>&1

		if [ -f ./log.txt ] && grep -q "Done." ./log.txt; then
			shellui message "Boot logo flashed!" --confirm "Done"
		else
			shellui message "Failed to flash boot logo.\n\nCheck log.txt for details." --confirm "Dismiss"
		fi

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	my282)
		LOGO_NAME="bootlogo.bmp"
		LOGO_PATH=$DIR/$LOGO_NAME

		if [ ! -f $LOGO_PATH ]; then
			shellui message "Missing bootlogo.bmp file!\n\nPlace bootlogo.bmp in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shellui message "Flash boot logo to device?\n\nThis modifies device firmware." \
			--confirm "Flash Logo" --cancel "Cancel"; then
			exit 0
		fi

		overclock.elf performance 2 1200 384 1080 0

		cp /dev/mtdblock0 boot0

		VERSION=$(cat /usr/miyoo/version)
		OFFSET_PATH="res/offset-$VERSION"

		if [ ! -f "$OFFSET_PATH" ]; then
			shellui message "Unsupported firmware version.\n\nYour device is unchanged." --confirm "Dismiss"
			exit 1
		fi

		OFFSET=$(cat "$OFFSET_PATH")

		shellui progress "Compressing logo..." --value 20

		gzip -k "$LOGO_PATH"
		LOGO_PATH=$LOGO_PATH.gz
		LOGO_SIZE=$(wc -c < "$LOGO_PATH")

		MAX_SIZE=62234
		if [ "$LOGO_SIZE" -gt "$MAX_SIZE" ]; then
			shellui message "Logo image too complex.\n\nSimplify the image and try again." --confirm "Dismiss"
			rm -f "$LOGO_PATH"
			exit 1
		fi

		shellui progress "Preparing firmware..." --value 50

		# Workaround for missing conv=notrunc support
		OFFSET_PART=$((OFFSET+LOGO_SIZE))
		dd if=boot0 of=boot0-suffix bs=1 skip=$OFFSET_PART 2>/dev/null
		dd if=$LOGO_PATH of=boot0 bs=1 seek=$OFFSET 2>/dev/null
		dd if=boot0-suffix of=boot0 bs=1 seek=$OFFSET_PART 2>/dev/null

		shellui progress "Flashing to device..." --value 80

		mtd write "$DIR/boot0" boot

		shellui progress "Complete!" --value 100
		sleep 0.5

		rm -f $LOGO_PATH boot0 boot0-suffix

		shellui message "Boot logo flashed!" --confirm "Done"

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	my355)
		# Confirm before flashing
		if ! shellui message "Flash boot logo to device?\n\nThis modifies device firmware\nand will reboot when done." \
			--confirm "Flash Logo" --cancel "Cancel"; then
			exit 0
		fi

		cd "$DIR"
		./apply.sh > ./log.txt 2>&1 &
		APPLY_PID=$!

		# Show progress messages based on timing
		shellui progress "Preparing environment..." --value 10
		sleep 2
		if ps | grep -q $APPLY_PID; then
			shellui progress "Extracting boot image..." --value 30
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			shellui progress "Unpacking resources..." --value 50
		fi
		sleep 4
		if ps | grep -q $APPLY_PID; then
			shellui progress "Replacing logo..." --value 65
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			shellui progress "Repacking boot image..." --value 80
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			shellui progress "Flashing to device..." --value 95
		fi
		sleep 5

		wait $APPLY_PID

		shellui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	tg5040)
		# Detect device variant (brick uses different bootlogo path)
		if [ "$DEVICE" = "brick" ]; then
			LOGO_PATH="$DIR/brick/bootlogo.bmp"
		else
			LOGO_PATH="$DIR/tg5040/bootlogo.bmp"
		fi

		if [ ! -f "$LOGO_PATH" ]; then
			shellui message "No bootlogo.bmp file found!\n\nPlace it in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shellui message "Flash boot logo to device?\n\nThis will copy the logo and reboot." \
			--confirm "Flash Logo" --cancel "Cancel"; then
			exit 0
		fi

		{
			shellui progress "Mounting boot partition..." --value 20

			BOOT_PATH=/mnt/boot/
			mkdir -p "$BOOT_PATH"
			mount -t vfat /dev/mmcblk0p1 "$BOOT_PATH"

			shellui progress "Copying boot logo..." --value 60

			cp "$LOGO_PATH" "$BOOT_PATH/bootlogo.bmp"
			sync

			shellui progress "Unmounting..." --value 90

			umount "$BOOT_PATH"

			shellui progress "Complete!" --value 100
			sleep 0.5

			echo "Done."
		} > ./log.txt 2>&1

		if [ -f ./log.txt ] && grep -q "Done." ./log.txt; then
			# Self-destruct before reboot
			mv "$DIR" "$DIR.disabled"
			rm -f /tmp/minui_exec
			shellui message "Boot logo flashed!" --confirm "Reboot Now"
			reboot
		else
			shellui message "Failed to flash boot logo.\n\nCheck log.txt for details." --confirm "Dismiss"
		fi
		;;

	zero28)
		BOOT_DEV=/dev/mmcblk0p1
		BOOT_PATH=/mnt/boot

		if [ ! -f bootlogo.bmp ]; then
			shellui message "No bootlogo.bmp file found!\n\nPlace it in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shellui message "Flash boot logo to device?\n\nThis will copy the logo and reboot." \
			--confirm "Flash Logo" --cancel "Cancel"; then
			exit 0
		fi

		shellui progress "Mounting boot partition..." --value 20

		mkdir -p $BOOT_PATH
		mount -t vfat $BOOT_DEV $BOOT_PATH

		shellui progress "Copying boot logo..." --value 60

		cp bootlogo.bmp $BOOT_PATH

		shellui progress "Unmounting..." --value 90

		umount $BOOT_PATH
		rm -rf $BOOT_PATH

		shellui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct before reboot
		mv "$DIR" "$DIR.disabled"

		shellui message "Boot logo flashed!" --confirm "Reboot Now"
		reboot
		;;

	m17)
		LOGO_PATH=$DIR/logo.bmp

		if [ ! -f $LOGO_PATH ]; then
			shellui message "No logo.bmp file found!\n\nPlace it in the pak folder." --confirm "Dismiss"
			exit 1
		fi

		# Confirm before flashing
		if ! shellui message "Flash boot logo to device?\n\nThis modifies device firmware." \
			--confirm "Flash Logo" --cancel "Cancel"; then
			exit 0
		fi

		{
			shellui progress "Reading logo size..." --value 10

			# Read new bitmap size
			HEX=`dd if=$LOGO_PATH bs=1 skip=2 count=4 status=none | xxd -g4 -p`
			BYTE0=$(printf "%s" "$HEX" | dd bs=1 skip=0 count=2 2>/dev/null)
			BYTE1=$(printf "%s" "$HEX" | dd bs=1 skip=2 count=2 2>/dev/null)
			BYTE2=$(printf "%s" "$HEX" | dd bs=1 skip=4 count=2 2>/dev/null)
			BYTE3=$(printf "%s" "$HEX" | dd bs=1 skip=6 count=2 2>/dev/null)
			COUNT=$((0x${BYTE3}${BYTE2}${BYTE1}${BYTE0}))
			if [ $COUNT -gt 32768 ]; then
				echo "logo.bmp too large ($COUNT). Aborted."
				shellui message "Logo file too large.\n\nMaximum size is 32KB." --confirm "Dismiss"
				exit 1
			fi

			shellui progress "Detecting boot partition..." --value 25

			# Detect boot partition revision
			OFFSET=4044800 # rev A
			SIGNATURE=`dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none`

			if [ "$SIGNATURE" = "BM" ]; then
				echo "Rev A"
			else
				OFFSET=4045312 # rev B
				SIGNATURE=`dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none`
				if [ "$SIGNATURE" = "BM" ]; then
					echo "Rev B"
				else
					OFFSET=4046848 # rev C
					SIGNATURE=`dd if=/dev/block/by-name/boot bs=1 skip=$OFFSET count=2 status=none`
					if [ "$SIGNATURE" = "BM" ]; then
						echo "Rev C"
					else
						echo "Rev unknown. Aborted."
						shellui message "Unknown boot partition format.\n\nYour device is unchanged." --confirm "Dismiss"
						exit 1
					fi
				fi
			fi

			shellui progress "Creating backup..." --value 50

			# Create backup
			DT=`date +'%Y%m%d%H%M%S'`
			HEX=`dd if=/dev/block/by-name/boot bs=1 skip=$(($OFFSET+2)) count=4 status=none | xxd -g4 -p`
			BYTE0=$(printf "%s" "$HEX" | dd bs=1 skip=0 count=2 2>/dev/null)
			BYTE1=$(printf "%s" "$HEX" | dd bs=1 skip=2 count=2 2>/dev/null)
			BYTE2=$(printf "%s" "$HEX" | dd bs=1 skip=4 count=2 2>/dev/null)
			BYTE3=$(printf "%s" "$HEX" | dd bs=1 skip=6 count=2 2>/dev/null)
			COUNT=$((0x${BYTE3}${BYTE2}${BYTE1}${BYTE0}))
			echo "copying $COUNT bytes from $OFFSET to backup-$DT.bmp"
			dd if=/dev/block/by-name/boot of=./backup-$DT.bmp bs=1 skip=$OFFSET count=$COUNT

			shellui progress "Flashing new logo..." --value 75

			# Inject new logo
			echo "injecting $LOGO_PATH"
			dd conv=notrunc if=$LOGO_PATH of=/dev/block/by-name/boot bs=1 seek=$OFFSET

			sync

			shellui progress "Complete!" --value 100
			sleep 0.5

			echo "Done."
		} > ./log.txt 2>&1

		if [ -f ./log.txt ] && grep -q "Done." ./log.txt; then
			shellui message "Boot logo flashed!\n\nA backup was saved to the pak folder." --confirm "Done"
		else
			shellui message "Failed to flash boot logo.\n\nCheck log.txt for details." --confirm "Dismiss"
		fi

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		;;

	*)
		shellui message "Boot logo flashing is not\nsupported on $PLATFORM." --confirm "Dismiss"
		exit 1
		;;
esac
