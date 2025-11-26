#!/bin/bash
# Deploy built files directly to SD card for rapid development iteration
#
# Usage: ./scripts/deploy-dev.sh [options]
#
# Options:
#   --no-update    Skip .tmp_update (won't trigger update on device boot)
#   --platform X   Only sync specific platform from .system (e.g., miyoomini)
#
# Prerequisites:
#   - Run 'make all' or 'make setup && make common PLATFORM=<platform>' first
#   - SD card must be mounted at /Volumes/LESSUI_DEV
#
# What this does:
#   Copies build/PAYLOAD/.system and build/PAYLOAD/.tmp_update directly to
#   the SD card, bypassing the zip/unzip cycle. Much faster than the full
#   workflow of: make -> copy zip -> load SD -> wait for device to unpack

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SD_CARD="/Volumes/LESSUI_DEV"
PAYLOAD_DIR="$PROJECT_ROOT/build/PAYLOAD"

# Parse arguments
SYNC_UPDATE=true
PLATFORM_FILTER=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-update)
            SYNC_UPDATE=false
            shift
            ;;
        --platform)
            PLATFORM_FILTER="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--no-update] [--platform <name>]"
            exit 1
            ;;
    esac
done

# Verify SD card is mounted
if [ ! -d "$SD_CARD" ]; then
    echo "Error: SD card not mounted at $SD_CARD"
    echo "Insert the SD card and wait for it to mount."
    exit 1
fi

# Verify build exists
if [ ! -d "$PAYLOAD_DIR/.system" ]; then
    echo "Error: Build not found at $PAYLOAD_DIR/.system"
    echo "Run 'make all' or 'make setup && make common PLATFORM=<platform>' first."
    exit 1
fi

echo "Deploying to $SD_CARD..."

# Common rsync options:
#   -a: archive mode (preserves permissions, etc.)
#   -v: verbose
#   --exclude: skip macOS metadata files
RSYNC_OPTS="-av --exclude=.DS_Store --exclude=._*"

# Sync .system directory
if [ -n "$PLATFORM_FILTER" ]; then
    # Only sync specific platform + shared files
    echo "  Syncing .system/$PLATFORM_FILTER..."
    rsync $RSYNC_OPTS --delete "$PAYLOAD_DIR/.system/$PLATFORM_FILTER/" "$SD_CARD/.system/$PLATFORM_FILTER/"

    # Also sync shared resources
    echo "  Syncing shared resources..."
    rsync $RSYNC_OPTS "$PAYLOAD_DIR/.system/res/" "$SD_CARD/.system/res/"
    rsync $RSYNC_OPTS "$PAYLOAD_DIR/.system/common/" "$SD_CARD/.system/common/"
    rsync $RSYNC_OPTS "$PAYLOAD_DIR/.system/cores/" "$SD_CARD/.system/cores/"
    [ -f "$PAYLOAD_DIR/.system/version.txt" ] && cp "$PAYLOAD_DIR/.system/version.txt" "$SD_CARD/.system/"
else
    # Sync entire .system directory
    echo "  Syncing .system/..."
    rsync $RSYNC_OPTS --delete "$PAYLOAD_DIR/.system/" "$SD_CARD/.system/"
fi

# Optionally sync .tmp_update (triggers update on boot)
if [ "$SYNC_UPDATE" = true ]; then
    echo "  Syncing .tmp_update/..."
    rsync $RSYNC_OPTS --delete "$PAYLOAD_DIR/.tmp_update/" "$SD_CARD/.tmp_update/"
else
    echo "  Skipping .tmp_update (--no-update specified)"
fi

# Eject the SD card
echo ""
echo "Ejecting SD card..."
diskutil eject "$SD_CARD" && echo "Done! SD card ejected." || echo "Warning: Failed to eject. You may need to eject manually."
