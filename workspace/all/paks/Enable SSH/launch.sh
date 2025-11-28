#!/bin/bash
DIR="$(dirname "$0")"
cd "$DIR"

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

# must be connected to wifi
if [ "$(cat /sys/class/net/wlan0/operstate)" != "up" ]; then
	$PRESENTER --message "WiFi not connected.\n\nPlease connect to WiFi first." --timeout 4
	exit 0
fi

{
	# switch language from mandarin to english since we require a reboot anyway
	locale-gen "en_US.UTF-8"
	echo "LANG=en_US.UTF-8" > /etc/default/locale

	# install or update ssh server
	apt -y update && apt -y install --reinstall openssh-server
	echo "d /run/sshd 0755 root root" > /etc/tmpfiles.d/sshd.conf

	# enable login root:root
	echo "PermitRootLogin yes" >> /etc/ssh/sshd_config
	printf "root\nroot" | passwd root

	echo "Success"
} > ./log.txt 2>&1 &
INSTALL_PID=$!

# Show installing message while it runs
$PRESENTER --message "Installing SSH server...\n\nThis may take a few minutes.\nDevice will reboot when done." --timeout 200 &
PRESENTER_PID=$!

# Wait for installation to complete
wait "$INSTALL_PID"

# Kill the presenter
kill "$PRESENTER_PID" 2>/dev/null

if grep -q "Success" ./log.txt; then
	$PRESENTER --message "SSH enabled successfully!\n\nLogin: root / root\nDevice will reboot now." --timeout 5 &
	sleep 5
	mv "$DIR" "$DIR.disabled"
	reboot
else
	$PRESENTER --message "SSH installation failed.\nCheck log.txt for details." --timeout 5
fi
