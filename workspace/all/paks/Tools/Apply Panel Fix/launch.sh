#!/bin/bash

DIR="$(dirname "$0")"
cd "$DIR"

# Confirm before modifying firmware
if ! shui message "Apply panel timing fix?" \
	--subtext "This modifies device firmware\nto improve display refresh rate." \
	--confirm "Apply" --cancel "Cancel"; then
	exit 0
fi

./apply.sh
