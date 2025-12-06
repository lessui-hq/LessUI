#!/bin/bash

cd "$(dirname "$0")"

# Confirm before modifying
if ! shui message "Swap Menu and Select buttons?\n\nThis modifies device firmware\nto swap button mappings." \
	--confirm "Swap" --cancel "Cancel"; then
	exit 0
fi

bash ./apply.sh > ./log.txt 2>&1
