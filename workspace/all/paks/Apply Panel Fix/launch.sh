#!/bin/bash

DIR="$(dirname "$0")"
cd "$DIR"

# Confirm before modifying firmware
if ! shellui message "Apply panel timing fix?\n\nThis modifies device firmware\nto improve display refresh rate." \
	--confirm "Apply Fix" --cancel "Cancel"; then
	exit 0
fi

bash ./apply.sh > ./log.txt 2>&1
