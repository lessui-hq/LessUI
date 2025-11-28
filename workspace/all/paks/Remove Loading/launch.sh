#!/bin/sh

# Cross-platform loading screen removal tool
# Patches device firmware to disable boot loading screens

DIR="$(dirname "$0")"
cd "$DIR" || exit 1

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

# Platform-specific implementations
case "$PLATFORM" in
	miyoomini)
		overclock.elf "$CPU_SPEED_GAME" # slow down, my282 didn't like overclock during this operation

		# Show progress
		$PRESENTER --message "Patching firmware...\n\nPlease wait up to 2 minutes.\nDo not power off!" --timeout 120 &
		PRESENTER_PID=$!

		{
			# squashfs tools and liblzma.so sourced from toolchain buildroot
			cp -r miyoomini/bin /tmp
			cp -r miyoomini/lib /tmp

			export PATH=/tmp/bin:$PATH
			export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

			cd /tmp || exit 1

			rm -rf customer squashfs-root customer.modified

			cp /dev/mtd6 customer

			unsquashfs customer
			if [ $? -ne 0 ]; then
				kill "$PRESENTER_PID" 2>/dev/null
				$PRESENTER --message "Failed to extract firmware.\nYour device is safe." --timeout 4
				sync
				exit 1
			fi

			sed -i '/^\/customer\/app\/sdldisplay/d' squashfs-root/main
			echo "patched main"

			mksquashfs squashfs-root customer.mod -comp xz -b 131072 -xattrs -all-root
			if [ $? -ne 0 ]; then
				kill "$PRESENTER_PID" 2>/dev/null
				$PRESENTER --message "Failed to repack firmware.\nYour device is safe." --timeout 4
				sync
				exit 1
			fi

			dd if=customer.mod of=/dev/mtdblock6 bs=128K conv=fsync
			if [ $? -ne 0 ]; then
				kill "$PRESENTER_PID" 2>/dev/null
				$PRESENTER --message "Failed to write firmware!\nDevice may need recovery." --timeout 5
				sync
				exit 1
			fi

		} &> ./log.txt

		kill "$PRESENTER_PID" 2>/dev/null
		$PRESENTER --message "Loading screen removed!\n\nRebooting in 2 seconds..." --timeout 2
		mv "$DIR" "$DIR.disabled"
		sync
		reboot
		;;

	my282)
		overclock.elf performance 2 1200 384 1080 0

		# Show progress
		$PRESENTER --message "Patching firmware...\n\nPlease wait up to 2 minutes.\nDo not power off!" --timeout 120 &
		PRESENTER_PID=$!

		{
			# same as miyoomini
			cp -r my282/bin /tmp
			cp -r my282/lib /tmp

			export PATH=/tmp/bin:$PATH
			export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

			cd /tmp || exit 1

			rm -rf rootfs squashfs-root rootfs.modified

			mtd read rootfs /dev/mtd3

			unsquashfs rootfs
			if [ $? -ne 0 ]; then
				kill "$PRESENTER_PID" 2>/dev/null
				$PRESENTER --message "Failed to extract firmware.\nYour device is safe." --timeout 4
				sync
				exit 1
			fi

			sed -i '/^\/customer\/app\/sdldisplay/d' squashfs-root/customer/main
			echo "patched main"

			mksquashfs squashfs-root rootfs.mod -comp xz -b 262144 -Xbcj arm
			if [ $? -ne 0 ]; then
				kill "$PRESENTER_PID" 2>/dev/null
				$PRESENTER --message "Failed to repack firmware.\nYour device is safe." --timeout 4
				sync
				exit 1
			fi

			mtd write rootfs.mod /dev/mtd3
			if [ $? -ne 0 ]; then
				kill "$PRESENTER_PID" 2>/dev/null
				$PRESENTER --message "Failed to write firmware!\nDevice may need recovery." --timeout 5
				sync
				exit 1
			fi

		} &> ./log.txt

		kill "$PRESENTER_PID" 2>/dev/null
		$PRESENTER --message "Loading screen removed successfully!" --timeout 3
		mv "$DIR" "$DIR.disabled"
		sync
		;;

	tg5040)
		sed -i '/^\/usr\/sbin\/pic2fb \/etc\/splash.png/d' /etc/init.d/runtrimui
		$PRESENTER --message "Boot loading screen removed!" --timeout 3

		mv "$DIR" "$DIR.disabled"
		sync
		;;

	*)
		$PRESENTER --message "Remove Loading not supported on $PLATFORM" --timeout 3
		exit 1
		;;
esac
