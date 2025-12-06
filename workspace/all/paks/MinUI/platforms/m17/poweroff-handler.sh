# shellcheck shell=bash
# Sourced by generated launch.sh
# m17 poweroff handler
# Physical powerswitch low power mode

poweroff_handler() {
	rm -f "/tmp/poweroff"
	killall keymon.elf
	shutdown
	# TODO: figure out how to control LED?
	while :; do
		sleep 5
	done
}
