#!/bin/sh
# LessOS Bootstrap for LessUI
# Sets up environment from LessOS vars and calls standard platform launcher
#
# LessOS provides these environment variables before calling this script:
#   LESSOS_PLATFORM  - SoC/platform family (e.g., "RK3566")
#   LESSOS_DEVICE    - Specific device model (e.g., "Powkiddy RGB30")
#   LESSOS_ARCH      - CPU architecture (e.g., "aarch64")
#   LESSOS_VERSION   - OS version (e.g., "20241215")
#   LESSOS_DIR       - Path to lessos directory (e.g., "/storage/lessos")
#   LESSOS_STORAGE   - Writable storage path (e.g., "/storage")
#   DISPLAY_WIDTH    - Framebuffer width
#   DISPLAY_HEIGHT   - Framebuffer height
#   DISPLAY_ROTATION - Display rotation (0-3)

if [ -z "$LESSOS_PLATFORM" ]; then
	echo "Error: LESSOS_PLATFORM not set - not running under LessOS" >&2
	exit 1
fi

if [ -z "$LESSOS_STORAGE" ]; then
	echo "Error: LESSOS_STORAGE not set" >&2
	exit 1
fi

# Export core variables from LessOS
# Normalize platform to lowercase (Docker/filesystem requirement)
PLATFORM="$(echo "$LESSOS_PLATFORM" | tr '[:upper:]' '[:lower:]')"
export PLATFORM
export DEVICE="$LESSOS_DEVICE"
export SDCARD_PATH="$LESSOS_STORAGE"

# Validate platform launcher exists
LAUNCHER_PATH="$SDCARD_PATH/.system/$PLATFORM/paks/LessUI.pak/launch.sh"
if [ ! -f "$LAUNCHER_PATH" ]; then
	echo "Error: Unsupported platform '$PLATFORM' (from LESSOS_PLATFORM='$LESSOS_PLATFORM')" >&2
	echo "Expected launcher at: $LAUNCHER_PATH" >&2
	exit 1
fi

# Hand off to standard platform launcher
exec "$LAUNCHER_PATH"
# If exec returns, it failed
echo "Error: Failed to execute launcher for platform '$PLATFORM'" >&2
exit 1
