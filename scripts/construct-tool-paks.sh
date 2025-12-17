#!/bin/bash
# Construct tool paks for a specific platform
#
# This script is called by the Makefile system target to construct
# tool paks from workspace/all/paks/Tools/ for a given platform.
#
# Usage: ./scripts/construct-tool-paks.sh <PLATFORM> [DEBUG]
#
# Arguments:
#   PLATFORM - Target platform (e.g., miyoomini, tg5040)
#   DEBUG    - If set to "1", include debug-only paks
#
# Each pak directory must contain pak.json with:
#   - platforms: array of platform names or ["all"] for universal paks
#   - debug: (optional) true to only include in debug builds

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Parse arguments
PLATFORM="${1:?Error: PLATFORM required as first argument}"
DEBUG="${2:-}"

PAKS_SOURCE="$PROJECT_ROOT/workspace/all/paks/Tools"
OUTPUT_BASE="$PROJECT_ROOT/build/Tools/$PLATFORM"

# Check for jq
if ! command -v jq &> /dev/null; then
    echo "Error: jq is required but not installed."
    exit 1
fi

# Process each pak directory
for pak_dir in "$PAKS_SOURCE"/*/; do
    [ -d "$pak_dir" ] || continue
    pak_name=$(basename "$pak_dir")

    # Skip if no pak.json
    [ -f "$pak_dir/pak.json" ] || continue

    # Check if debug-only pak
    is_debug_pak=$(jq -r '.debug // false' "$pak_dir/pak.json")
    if [ "$is_debug_pak" = "true" ] && [ -z "$DEBUG" ]; then
        continue
    fi

    # Check platform compatibility
    if ! jq -e ".platforms | index(\"$PLATFORM\") or index(\"all\")" "$pak_dir/pak.json" > /dev/null 2>&1; then
        continue
    fi

    echo "  Constructing ${pak_name}.pak for $PLATFORM"
    output_dir="$OUTPUT_BASE/${pak_name}.pak"
    mkdir -p "$output_dir"

    # Copy launch.sh
    if [ -f "$pak_dir/launch.sh" ]; then
        rsync -a "$pak_dir/launch.sh" "$output_dir/"
        chmod +x "$output_dir/launch.sh"
    fi

    # Copy metadata files
    [ -f "$pak_dir/pak.json" ] && rsync -a "$pak_dir/pak.json" "$output_dir/"
    [ -f "$pak_dir/settings.json" ] && rsync -a "$pak_dir/settings.json" "$output_dir/"

    # Copy resources
    if [ -d "$pak_dir/res" ]; then
        mkdir -p "$output_dir/res"
        for res_file in "$pak_dir/res"/*; do
            [ -f "$res_file" ] && rsync -a "$res_file" "$output_dir/res/"
        done
        # Platform-specific resources
        if [ -d "$pak_dir/res/$PLATFORM" ]; then
            rsync -a "$pak_dir/res/$PLATFORM/" "$output_dir/res/$PLATFORM/"
        fi
    fi

    # Copy platform-specific binaries
    if [ -d "$pak_dir/bin/$PLATFORM" ]; then
        mkdir -p "$output_dir/bin"
        rsync -a "$pak_dir/bin/$PLATFORM/" "$output_dir/bin/$PLATFORM/"
    fi

    # Copy executable scripts from bin/
    for script in "$pak_dir/bin"/*; do
        if [ -f "$script" ] && [ -x "$script" ]; then
            mkdir -p "$output_dir/bin"
            rsync -a "$script" "$output_dir/bin/"
        fi
    done

    # Copy platform-specific libs
    if [ -d "$pak_dir/lib/$PLATFORM" ]; then
        mkdir -p "$output_dir/lib"
        rsync -a "$pak_dir/lib/$PLATFORM/" "$output_dir/lib/$PLATFORM/"
    fi

    # Copy platform overlay directory
    if [ -d "$pak_dir/$PLATFORM" ]; then
        rsync -a "$pak_dir/$PLATFORM/" "$output_dir/" 2>/dev/null || true
    fi

    # Copy built ELF files
    for elf in "$pak_dir/build/$PLATFORM/"*.elf; do
        if [ -f "$elf" ]; then
            mkdir -p "$output_dir/bin"
            rsync -a "$elf" "$output_dir/bin/"
        fi
    done
done
