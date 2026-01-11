#!/bin/sh

# Cross-platform loading screen removal tool
# Patches device firmware to disable boot loading screens

DIR="$(dirname "$0")"
cd "$DIR" || exit 1

# Platform-specific implementations
case "$PLATFORM" in
	miyoomini)
		# Confirm before modifying firmware
		if ! shui message "Remove boot loading screen?" \
			--subtext "This modifies device firmware.\nDo not power off during process." \
			--confirm "Remove" --cancel "Cancel"; then
			exit 0
		fi

		overclock.elf "$CPU_SPEED_GAME" # slow down, my282 didn't like overclock during this operation

		shui progress "Preparing tools..." --value 5

		# squashfs tools and liblzma.so sourced from toolchain buildroot
		cp -r miyoomini/bin /tmp
		cp -r miyoomini/lib /tmp

		export PATH=/tmp/bin:$PATH
		export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

		cd /tmp || exit 1

		rm -rf customer squashfs-root customer.modified

		shui progress "Reading firmware..." --value 15

		cp /dev/mtd6 customer

		shui progress "Extracting firmware..." --value 30

		if ! unsquashfs customer; then
			shui message "Failed to extract firmware." \
				--subtext "Your device is unchanged." --confirm "Dismiss"
			sync
			exit 1
		fi

		shui progress "Patching firmware..." --value 50

		sed -i '/^\/customer\/app\/sdldisplay/d' squashfs-root/main
		echo "patched main"

		shui progress "Repacking firmware..." --value 65

		if ! mksquashfs squashfs-root customer.mod -comp xz -b 131072 -xattrs -all-root; then
			shui message "Failed to repack firmware." \
				--subtext "Your device is unchanged." --confirm "Dismiss"
			sync
			exit 1
		fi

		shui progress "Writing firmware..." --value 85

		if ! dd if=customer.mod of=/dev/mtdblock6 bs=128K conv=fsync; then
			shui message "Failed to write firmware!" \
				--subtext "Device may need recovery." --confirm "Dismiss"
			sync
			exit 1
		fi

		shui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct before reboot
		mv "$DIR" "$DIR.disabled"
		sync

		shui message "Loading screen removed!" \
			--subtext "Device will reboot to apply changes." --confirm "Reboot"
		reboot
		;;

	my282)
		# Confirm before modifying firmware
		if ! shui message "Remove boot loading screen?" \
			--subtext "This modifies device firmware.\nDo not power off during process." \
			--confirm "Remove" --cancel "Cancel"; then
			exit 0
		fi

		overclock.elf performance 2 1200 384 1080 0

		shui progress "Preparing tools..." --value 5

		# same as miyoomini
		cp -r my282/bin /tmp
		cp -r my282/lib /tmp

		export PATH=/tmp/bin:$PATH
		export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

		cd /tmp || exit 1

		rm -rf rootfs squashfs-root rootfs.modified

		shui progress "Reading firmware..." --value 15

		mtd read rootfs /dev/mtd3

		shui progress "Extracting firmware..." --value 30

		if ! unsquashfs rootfs; then
			shui message "Failed to extract firmware." \
				--subtext "Your device is unchanged." --confirm "Dismiss"
			sync
			exit 1
		fi

		shui progress "Patching firmware..." --value 50

		sed -i '/^\/customer\/app\/sdldisplay/d' squashfs-root/customer/main
		echo "patched main"

		shui progress "Repacking firmware..." --value 65

		if ! mksquashfs squashfs-root rootfs.mod -comp xz -b 262144 -Xbcj arm; then
			shui message "Failed to repack firmware." \
				--subtext "Your device is unchanged." --confirm "Dismiss"
			sync
			exit 1
		fi

		shui progress "Writing firmware..." --value 85

		if ! mtd write rootfs.mod /dev/mtd3; then
			shui message "Failed to write firmware!" \
				--subtext "Device may need recovery." --confirm "Dismiss"
			sync
			exit 1
		fi

		shui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		sync

		shui message "Loading screen removed!" --confirm "Done"
		;;

	tg5040 | tg5050)
		# Confirm before modifying
		if ! shui message "Remove boot loading screen?" \
			--subtext "This modifies system files." \
			--confirm "Remove" --cancel "Cancel"; then
			exit 0
		fi

		shui progress "Patching boot script..." --indeterminate

		sed -i '/^\/usr\/sbin\/pic2fb \/etc\/splash.png/d' /etc/init.d/runtrimui

		shui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		sync

		shui message "Loading screen removed!" --confirm "Done"
		;;

	*)
		shui message "Loading screen removal is not\nsupported on $PLATFORM." --confirm "Dismiss"
		exit 1
		;;
esac
