#!/bin/sh
# shellcheck shell=bash
# Sourced by generated launch.sh
# rg35xxplus late-init hook
# Runs after auto.sh - init.elf and hdmimon need to run after user customization

init.elf
hdmimon.sh &

# Wait for HDMI if connected
HDMI_STATE_PATH="/sys/class/switch/hdmi/cable.0/state"
HAS_HDMI=$(cat "$HDMI_STATE_PATH")
if [ "$HAS_HDMI" = "1" ]; then
	sleep 3
fi
