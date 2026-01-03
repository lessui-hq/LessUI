#!/bin/sh
# LessOS Bootstrap for LessUI
# Mirrors stock platform hijack scripts (miyoomini.sh, etc.)
#
# LessOS provides these environment variables before calling this script:
#   LESSOS_PLATFORM  - SoC/platform family (e.g., "RK3566")
#   LESSOS_DEVICE    - Specific device model (e.g., "Powkiddy RGB30")
#   LESSOS_STORAGE   - Writable storage path (e.g., "/storage")

SDCARD_PATH="${LESSOS_STORAGE:-/storage}"
SCRIPT_DIR="$(dirname "$0")"

# Copy .tmp_update to SD card (like miyoomini.sh does)
# This ensures the updater and platform boot scripts are in place
if [ -d "$SCRIPT_DIR/.tmp_update" ]; then
	cp -rf "$SCRIPT_DIR/.tmp_update" "$SDCARD_PATH/"
	sync
fi

# Run the updater (handles platform detection, updates, and launching)
"$SDCARD_PATH/.tmp_update/updater"

# Fallback if updater exits unexpectedly
reboot
