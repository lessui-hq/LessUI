#!/bin/sh

# Cross-platform loading screen removal tool
# Patches device firmware to disable boot loading screens

DIR="$(dirname "$0")"
cd "$DIR" || exit 1

# Platform-specific implementations
case "$PLATFORM" in
	miyoomini)
		# Confirm before modifying firmware
		if ! shellui message "Remove boot loading screen?\n\nThis modifies device firmware.\nDo not power off during process." \
			--confirm "Remove Loading" --cancel "Cancel"; then
			exit 0
		fi

		overclock.elf "$CPU_SPEED_GAME" # slow down, my282 didn't like overclock during this operation

		{
			shellui progress "Preparing tools..." --value 5

			# squashfs tools and liblzma.so sourced from toolchain buildroot
			cp -r miyoomini/bin /tmp
			cp -r miyoomini/lib /tmp

			export PATH=/tmp/bin:$PATH
			export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

			cd /tmp || exit 1

			rm -rf customer squashfs-root customer.modified

			shellui progress "Reading firmware..." --value 15

			cp /dev/mtd6 customer

			shellui progress "Extracting firmware..." --value 30

			unsquashfs customer
			if [ $? -ne 0 ]; then
				shellui message "Failed to extract firmware.\n\nYour device is unchanged." --confirm "Dismiss"
				sync
				exit 1
			fi

			shellui progress "Patching firmware..." --value 50

			sed -i '/^\/customer\/app\/sdldisplay/d' squashfs-root/main
			echo "patched main"

			shellui progress "Repacking firmware..." --value 65

			mksquashfs squashfs-root customer.mod -comp xz -b 131072 -xattrs -all-root
			if [ $? -ne 0 ]; then
				shellui message "Failed to repack firmware.\n\nYour device is unchanged." --confirm "Dismiss"
				sync
				exit 1
			fi

			shellui progress "Writing firmware..." --value 85

			dd if=customer.mod of=/dev/mtdblock6 bs=128K conv=fsync
			if [ $? -ne 0 ]; then
				shellui message "Failed to write firmware!\n\nDevice may need recovery." --confirm "Dismiss"
				sync
				exit 1
			fi

			shellui progress "Complete!" --value 100
			sleep 0.5

		} &> ./log.txt

		# Self-destruct before reboot
		mv "$DIR" "$DIR.disabled"
		sync

		shellui message "Loading screen removed!\n\nDevice will reboot to apply changes." --confirm "Reboot Now"
		reboot
		;;

	my282)
		# Confirm before modifying firmware
		if ! shellui message "Remove boot loading screen?\n\nThis modifies device firmware.\nDo not power off during process." \
			--confirm "Remove Loading" --cancel "Cancel"; then
			exit 0
		fi

		overclock.elf performance 2 1200 384 1080 0

		{
			shellui progress "Preparing tools..." --value 5

			# same as miyoomini
			cp -r my282/bin /tmp
			cp -r my282/lib /tmp

			export PATH=/tmp/bin:$PATH
			export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

			cd /tmp || exit 1

			rm -rf rootfs squashfs-root rootfs.modified

			shellui progress "Reading firmware..." --value 15

			mtd read rootfs /dev/mtd3

			shellui progress "Extracting firmware..." --value 30

			unsquashfs rootfs
			if [ $? -ne 0 ]; then
				shellui message "Failed to extract firmware.\n\nYour device is unchanged." --confirm "Dismiss"
				sync
				exit 1
			fi

			shellui progress "Patching firmware..." --value 50

			sed -i '/^\/customer\/app\/sdldisplay/d' squashfs-root/customer/main
			echo "patched main"

			shellui progress "Repacking firmware..." --value 65

			mksquashfs squashfs-root rootfs.mod -comp xz -b 262144 -Xbcj arm
			if [ $? -ne 0 ]; then
				shellui message "Failed to repack firmware.\n\nYour device is unchanged." --confirm "Dismiss"
				sync
				exit 1
			fi

			shellui progress "Writing firmware..." --value 85

			mtd write rootfs.mod /dev/mtd3
			if [ $? -ne 0 ]; then
				shellui message "Failed to write firmware!\n\nDevice may need recovery." --confirm "Dismiss"
				sync
				exit 1
			fi

			shellui progress "Complete!" --value 100
			sleep 0.5

		} &> ./log.txt

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		sync

		shellui message "Loading screen removed!" --confirm "Done"
		;;

	tg5040)
		# Confirm before modifying
		if ! shellui message "Remove boot loading screen?\n\nThis modifies system files." \
			--confirm "Remove Loading" --cancel "Cancel"; then
			exit 0
		fi

		shellui progress "Patching boot script..." --indeterminate

		sed -i '/^\/usr\/sbin\/pic2fb \/etc\/splash.png/d' /etc/init.d/runtrimui

		shellui progress "Complete!" --value 100
		sleep 0.5

		# Self-destruct
		mv "$DIR" "$DIR.disabled"
		sync

		shellui message "Loading screen removed!" --confirm "Done"
		;;

	*)
		shellui message "Loading screen removal is not\nsupported on $PLATFORM." --confirm "Dismiss"
		exit 1
		;;
esac
