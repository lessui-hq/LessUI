#!/bin/sh

# Cross-platform boot logo flasher
# Uses platform-specific logic to flash custom boot logos to device firmware

DIR="$(dirname "$0")"
cd "$DIR"

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

# Platform-specific implementations
case "$PLATFORM" in
	miyoomini)
		$PRESENTER --message "Flashing boot logo..." --timeout 30 &
		PRESENTER_PID=$!

		{
			SUPPORTED_VERSION="202304280000"
			if [ "$MIYOO_VERSION" -gt "$SUPPORTED_VERSION" ]; then
				echo "Unknown firmware version. Aborted."
				exit 1
			fi

			./logoread.elf

			if [ -f ./logo.jpg ]; then
				cp ./logo.jpg ./image1.jpg
			else
				cp "$SYSTEM_PATH/dat/image1.jpg" ./
			fi

			if ! ./logomake.elf; then
				echo "Preparing bootlogo failed. Aborted."
				exit 1
			fi

			if ! ./logowrite.elf; then
				echo "Flashing bootlogo failed. Aborted."
				exit 1
			fi

			echo "Flashed bootlogo successfully. Tidying up."

			rm -f image1.jpg image2.jpg image3.jpg logo.img

			echo "Done."
		} > ./log.txt 2>&1

		kill "$PRESENTER_PID" 2>/dev/null

		if [ -f ./log.txt ] && grep -q "Done." ./log.txt; then
			$PRESENTER --message "Boot logo flashed successfully!" --timeout 3
		else
			$PRESENTER --message "Failed to flash boot logo. Check log.txt" --timeout 4
		fi
		;;

	trimuismart)
		{
			LOGO_PATH=./logo.bmp

			if [ ! -f $LOGO_PATH ]; then
				LOGO_PATH=$SYSTEM_PATH/dat/logo.bmp
			fi

			if [ ! -f $LOGO_PATH ]; then
				echo "No logo.bmp available. Aborted"
				exit 1
			fi

			dd if=$LOGO_PATH of=/dev/by-name/bootlogo bs=65536

			echo "Done."
		} > ./log.txt 2>&1

		if [ -f ./log.txt ] && grep -q "Done." ./log.txt; then
			$PRESENTER --message "Boot logo flashed successfully!" --timeout 3
		else
			$PRESENTER --message "Failed to flash boot logo. Check log.txt" --timeout 4
		fi
		;;

	my282)
		LOGO_NAME="bootlogo.bmp"

		overclock.elf performance 2 1200 384 1080 0

		LOGO_PATH=$DIR/$LOGO_NAME
		if [ ! -f $LOGO_PATH ]; then
			$PRESENTER --message "Missing bootlogo.bmp file!" --timeout 2
			exit 1
		fi

		cp /dev/mtdblock0 boot0

		VERSION=$(cat /usr/miyoo/version)
		OFFSET_PATH="res/offset-$VERSION"

		if [ ! -f "$OFFSET_PATH" ]; then
			$PRESENTER --message "Unsupported firmware version!" --timeout 2
			exit 1
		fi

		OFFSET=$(cat "$OFFSET_PATH")

		$PRESENTER --message "Updating boot logo..." --timeout 120 &
		PRESENTER_PID=$!

		gzip -k "$LOGO_PATH"
		LOGO_PATH=$LOGO_PATH.gz
		LOGO_SIZE=$(wc -c < "$LOGO_PATH")

		MAX_SIZE=62234
		if [ "$LOGO_SIZE" -gt "$MAX_SIZE" ]; then
			kill $PRESENTER_PID 2>/dev/null
			$PRESENTER --message "Logo too complex, simplify image!" --timeout 4
			exit 1
		fi

		# Workaround for missing conv=notrunc support
		OFFSET_PART=$((OFFSET+LOGO_SIZE))
		dd if=boot0 of=boot0-suffix bs=1 skip=$OFFSET_PART 2>/dev/null
		dd if=$LOGO_PATH of=boot0 bs=1 seek=$OFFSET 2>/dev/null
		dd if=boot0-suffix of=boot0 bs=1 seek=$OFFSET_PART 2>/dev/null

		mtd write "$DIR/boot0" boot
		kill $PRESENTER_PID 2>/dev/null

		rm -f $LOGO_PATH boot0 boot0-suffix

		$PRESENTER --message "Boot logo flashed successfully!" --timeout 3
		;;

	my355)
		cd "$DIR"
		./apply.sh > ./log.txt 2>&1 &
		APPLY_PID=$!

		# Show progress messages based on log output
		sleep 2
		if ps | grep -q $APPLY_PID; then
			$PRESENTER --message "Preparing environment..." --timeout 2
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			$PRESENTER --message "Extracting boot image..." --timeout 3
		fi
		sleep 4
		if ps | grep -q $APPLY_PID; then
			$PRESENTER --message "Unpacking resources..." --timeout 3
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			$PRESENTER --message "Replacing logo..." --timeout 3
		fi
		sleep 3
		if ps | grep -q $APPLY_PID; then
			$PRESENTER --message "Repacking boot image..." --timeout 4
		fi
		sleep 5
		if ps | grep -q $APPLY_PID; then
			$PRESENTER --message "Flashing to device..." --timeout 4
		fi

		wait $APPLY_PID
		;;

	tg5040)
		DIR=$(dirname "$0")
		cd "$DIR" || exit 1

		# Detect device variant (brick uses different bootlogo path)
		if [ "$DEVICE" = "brick" ]; then
			LOGO_PATH="$DIR/brick/bootlogo.bmp"
		else
			LOGO_PATH="$DIR/tg5040/bootlogo.bmp"
		fi

		if [ ! -f "$LOGO_PATH" ]; then
			$PRESENTER --message "No bootlogo.bmp file found!" --timeout 3
			exit 1
		fi

		$PRESENTER --message "Flashing boot logo..." --timeout 10 &
		PRESENTER_PID=$!

		{
			BOOT_PATH=/mnt/boot/
			mkdir -p "$BOOT_PATH"
			mount -t vfat /dev/mmcblk0p1 "$BOOT_PATH"
			cp "$LOGO_PATH" "$BOOT_PATH/bootlogo.bmp"
			sync
			umount "$BOOT_PATH"

			echo "Done."
		} > ./log.txt 2>&1

		kill "$PRESENTER_PID" 2>/dev/null

		if [ -f ./log.txt ] && grep -q "Done." ./log.txt; then
			$PRESENTER --message "Boot logo flashed! Rebooting..." --timeout 3
		else
			$PRESENTER --message "Failed to flash boot logo. Check log.txt" --timeout 4
		fi

		rm -f /tmp/minui_exec
		reboot
		;;

	zero28)
		BOOT_DEV=/dev/mmcblk0p1
		BOOT_PATH=/mnt/boot

		mkdir -p $BOOT_PATH
		mount -t vfat $BOOT_DEV $BOOT_PATH

		cp bootlogo.bmp $BOOT_PATH

		umount $BOOT_PATH
		rm -rf $BOOT_PATH

		$PRESENTER --message "Boot logo flashed! Rebooting..." --timeout 3

		reboot
		;;

	m17)
		{
			LOGO_PATH=$DIR/logo.bmp

			if [ ! -f $LOGO_PATH ]; then
				echo "No logo.bmp available. Aborted."
				exit 1
			fi

			# Read new bitmap size
			HEX=`dd if=$LOGO_PATH bs=1 skip=2 count=4 status=none | xxd -g4 -p`
			BYTE0=$(printf "%s" "$HEX" | dd bs=1 skip=0 count=2 2>/dev/null)
			BYTE1=$(printf "%s" "$HEX" | dd bs=1 skip=2 count=2 2>/dev/null)
			BYTE2=$(printf "%s" "$HEX" | dd bs=1 skip=4 count=2 2>/dev/null)
			BYTE3=$(printf "%s" "$HEX" | dd bs=1 skip=6 count=2 2>/dev/null)
			COUNT=$((0x${BYTE3}${BYTE2}${BYTE1}${BYTE0}))
			if [ $COUNT -gt 32768 ]; then
				echo "logo.bmp too large ($COUNT). Aborted."
				exit 1
			fi

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
						exit 1
					fi
				fi
			fi

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

			# Inject new logo
			echo "injecting $LOGO_PATH"
			dd conv=notrunc if=$LOGO_PATH of=/dev/block/by-name/boot bs=1 seek=$OFFSET

			sync
			echo "Done."
		} > ./log.txt 2>&1

		if [ -f ./log.txt ] && grep -q "Done." ./log.txt; then
			$PRESENTER --message "Boot logo flashed successfully!" --timeout 3
		else
			$PRESENTER --message "Failed to flash boot logo. Check log.txt" --timeout 4
		fi
		;;

	*)
		$PRESENTER --message "Bootlogo not supported on $PLATFORM" --timeout 3
		exit 1
		;;
esac

# Self-destruct (prevent re-running)
mv "$DIR" "$DIR.disabled"
