# shellcheck shell=bash
# Sourced by generated launch.sh
# trimuismart poweroff handler
# Physical powerswitch low power mode

poweroff_handler() {
	rm -f "/tmp/poweroff"
	killall keymon.elf
	shutdown
	echo 60000 > $CPU_PATH
	LED_ON=true
	while :; do
		if $LED_ON; then
			LED_ON=false
			leds_off
		else
			LED_ON=true
			leds_on
		fi
		sleep 0.5
	done
}
