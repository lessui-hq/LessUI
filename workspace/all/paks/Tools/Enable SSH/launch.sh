#!/bin/bash
DIR="$(dirname "$0")"
cd "$DIR"

# Must be connected to wifi
if [ "$(cat /sys/class/net/wlan0/operstate)" != "up" ]; then
	shui message "WiFi not connected." \
		--subtext "Please connect to WiFi first." --confirm "Dismiss"
	exit 0
fi

# Confirm before installing
if ! shui message "Install SSH server?" \
	--subtext "Downloads packages and reboots when complete.\nLogin: root / root" \
	--confirm "Install" --cancel "Cancel"; then
	exit 0
fi

{
	shui progress "Updating package list..." --value 10

	# switch language from mandarin to english since we require a reboot anyway
	locale-gen "en_US.UTF-8"
	echo "LANG=en_US.UTF-8" >/etc/default/locale

	shui progress "Downloading SSH server..." --value 30

	# install or update ssh server
	apt -y update

	shui progress "Installing SSH server..." --value 50

	apt -y install --reinstall openssh-server

	shui progress "Configuring SSH..." --value 80

	echo "d /run/sshd 0755 root root" >/etc/tmpfiles.d/sshd.conf

	# enable login root:root
	echo "PermitRootLogin yes" >>/etc/ssh/sshd_config
	printf "root\nroot" | passwd root

	shui progress "Complete!" --value 100
	sleep 0.5

	echo "Success"
} >./log.txt 2>&1

if grep -q "Success" ./log.txt; then
	# Self-destruct before reboot
	mv "$DIR" "$DIR.disabled"

	shui message "SSH installed successfully!" \
		--subtext "Login: root / root\nDevice will reboot to apply changes." --confirm "Reboot"
	reboot
else
	shui message "SSH installation failed." \
		--subtext "Check log.txt for details." --confirm "Dismiss"
fi
