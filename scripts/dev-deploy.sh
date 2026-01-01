#!/bin/bash
# Deploy built files directly to SD card for rapid development iteration
#
# Usage: ./scripts/dev-deploy.sh [options]
#
# Options:
#   --path PATH    Override SD card path (default: /Volumes/LESSUI_DEV)
#   --no-update    Skip .tmp_update (won't trigger update on device boot)
#   --platform X   Only sync specific platform (e.g., miyoomini)
#   --no-eject     Don't eject the SD card after sync
#
# Prerequisites:
#   - Run 'make all' or 'make setup && make common PLATFORM=<platform>' first
#   - SD card must be mounted at /Volumes/LESSUI_DEV
#
# Sync Strategy:
#   MIRROR (--delete): .system, .tmp_update, Tools, platform autoboot dirs
#   MERGE (no delete): Bios, Roms, Saves (preserve user content)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SD_CARD="/Volumes/LESSUI_DEV"  # Default, can be overridden
PAYLOAD_DIR="$PROJECT_ROOT/build/PAYLOAD"
BASE_DIR="$PROJECT_ROOT/build/BASE"

# Parse arguments
SYNC_UPDATE=true
PLATFORM_FILTER=""
DO_EJECT=true

while [[ $# -gt 0 ]]; do
    case $1 in
        --path)
            if [[ -z "${2:-}" || "$2" == --* ]]; then
                echo "Error: --path requires a value"
                exit 1
            fi
            SD_CARD="$2"
            shift 2
            ;;
        --no-update)
            SYNC_UPDATE=false
            shift
            ;;
        --platform)
            if [[ -z "${2:-}" || "$2" == --* ]]; then
                echo "Error: --platform requires a value"
                exit 1
            fi
            PLATFORM_FILTER="$2"
            shift 2
            ;;
        --no-eject)
            DO_EJECT=false
            shift
            ;;
        -h|--help)
            # Print header comments (stop at first non-comment line)
            sed -n '2,/^[^#]/p' "$0" | grep '^#' | cut -c3-
            exit 0
            ;;
        *)
            echo "Error: Unknown option: $1"
            echo "Usage: $0 [--path <path>] [--no-update] [--platform <name>] [--no-eject]"
            exit 1
            ;;
    esac
done

# Verify SD card is mounted
if [[ ! -d "$SD_CARD" ]]; then
    echo "Error: SD card not mounted at $SD_CARD"
    exit 1
fi

# Verify build exists
if [[ ! -d "$PAYLOAD_DIR/.system" ]]; then
    echo "Error: Build not found. Run 'make all' first."
    exit 1
fi

echo "Deploying to $SD_CARD..."
echo ""

# Rsync options for FAT32/exFAT filesystems
# Note: ._* files are created by macOS rsync on FAT - we just ignore them
RSYNC_OPTS=(-rt --no-p --no-o --no-g --modify-window=1 --force
            --exclude=.DS_Store --exclude="._*")

# Track sync statistics
TOTAL_FILES=0
TOTAL_DIRS=0

# Sync helper: $1=src $2=dst $3=name $4=--delete (optional)
sync_dir() {
    [[ -d "$1" ]] || return 0
    local mode="${4:+mirror}${4:-merge}"
    local output files dirs
    # Run rsync with itemize to count changes
    output=$(rsync "${RSYNC_OPTS[@]}" --itemize-changes ${4:+"$4"} "$1/" "$2/" 2>&1)
    files=$(echo "$output" | grep -c '^<f' || true)
    dirs=$(echo "$output" | grep -c '^cd' || true)
    TOTAL_FILES=$((TOTAL_FILES + files))
    TOTAL_DIRS=$((TOTAL_DIRS + dirs))
    if [[ $files -gt 0 || $dirs -gt 0 ]]; then
        printf "  %-30s %s (%d files)\n" "$3" "$mode" "$files"
    else
        printf "  %-30s %s (up to date)\n" "$3" "$mode"
    fi
}

sync_file() {
    [[ -f "$1" ]] || return 0
    local output
    output=$(rsync "${RSYNC_OPTS[@]}" --itemize-changes "$1" "$2" 2>&1)
    if [[ -n "$output" ]]; then
        TOTAL_FILES=$((TOTAL_FILES + 1))
        printf "  %-30s copied\n" "$3"
    else
        printf "  %-30s up to date\n" "$3"
    fi
}

# --- System directories (mirror) ---
echo "System:"

if [[ -n "$PLATFORM_FILTER" ]]; then
    # Validate platform exists
    if [[ ! -d "$PAYLOAD_DIR/.system/$PLATFORM_FILTER" ]]; then
        echo "Error: Platform '$PLATFORM_FILTER' not found in build"
        echo -n "Available:"
        for d in "$PAYLOAD_DIR/.system"/*/; do
            p=$(basename "$d")
            [[ "$p" =~ ^(res|common)$ ]] || echo -n " $p"
        done
        echo ""
        exit 1
    fi
    sync_dir "$PAYLOAD_DIR/.system/$PLATFORM_FILTER" "$SD_CARD/.system/$PLATFORM_FILTER" ".system/$PLATFORM_FILTER" --delete
    sync_dir "$PAYLOAD_DIR/.system/res" "$SD_CARD/.system/res" ".system/res"
    sync_dir "$PAYLOAD_DIR/.system/common" "$SD_CARD/.system/common" ".system/common"
    sync_file "$PAYLOAD_DIR/.system/version.txt" "$SD_CARD/.system/" ".system/version.txt"
else
    sync_dir "$PAYLOAD_DIR/.system" "$SD_CARD/.system" ".system" --delete
fi

if [[ "$SYNC_UPDATE" == true ]]; then
    sync_dir "$PAYLOAD_DIR/.tmp_update" "$SD_CARD/.tmp_update" ".tmp_update" --delete
else
    echo "  [.tmp_update] skipped (--no-update)"
fi

if [[ -n "$PLATFORM_FILTER" ]]; then
    sync_dir "$BASE_DIR/Tools/$PLATFORM_FILTER" "$SD_CARD/Tools/$PLATFORM_FILTER" "Tools/$PLATFORM_FILTER" --delete
else
    sync_dir "$BASE_DIR/Tools" "$SD_CARD/Tools" "Tools" --delete
fi

# --- Platform autoboot directories (mirror) ---
# These are top-level dirs in BASE that aren't special (Tools, Bios, Roms, etc.)
echo ""
echo "Autoboot:"
SKIP_DIRS="Tools|Bios|Roms|Saves"
for dir in "$BASE_DIR"/*/; do
    [[ -d "$dir" ]] || continue
    name=$(basename "$dir")
    # Skip known non-platform directories and files
    [[ "$name" =~ ^($SKIP_DIRS)$ ]] && continue
    # Apply platform filter if specified (exact match)
    if [[ -n "$PLATFORM_FILTER" && "$name" != *"$PLATFORM_FILTER"* ]]; then
        continue
    fi
    sync_dir "$dir" "$SD_CARD/$name" "$name" --delete
done

# --- User directories (merge - preserves user content) ---
echo ""
echo "User data:"
sync_dir "$BASE_DIR/Bios" "$SD_CARD/Bios" "Bios"
sync_dir "$BASE_DIR/Roms" "$SD_CARD/Roms" "Roms"
sync_dir "$BASE_DIR/Saves" "$SD_CARD/Saves" "Saves"

# --- Other files ---
echo ""
echo "Other:"
sync_file "$BASE_DIR/README.txt" "$SD_CARD/" "README.txt"
sync_file "$BASE_DIR/em_ui.sh" "$SD_CARD/" "em_ui.sh"

# Summary and eject
echo ""
if [[ $TOTAL_FILES -gt 0 ]]; then
    echo "Synced $TOTAL_FILES file(s) to $SD_CARD"
else
    echo "Everything up to date"
fi

if [[ "$DO_EJECT" == true ]]; then
    printf "Ejecting... "
    if diskutil eject "$SD_CARD" >/dev/null 2>&1; then
        echo "done"
    else
        echo "failed"
    fi
fi
