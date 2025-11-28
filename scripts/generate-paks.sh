#!/bin/bash
# Generate platform-specific paks from templates
#
# This script generates all .pak directories for all platforms from canonical templates.
# - Emulator paks: skeleton/TEMPLATES/minarch-paks/
# - Tool paks: workspace/all/paks/
#
# Usage:
#   ./scripts/generate-paks.sh                    # Generate all paks for all platforms
#   ./scripts/generate-paks.sh miyoomini          # Generate paks for specific platform
#   ./scripts/generate-paks.sh miyoomini GB GBA   # Generate specific paks for platform

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEMPLATE_DIR="$PROJECT_ROOT/workspace/all/paks/Emus"
DIRECT_PAKS_DIR="$PROJECT_ROOT/skeleton/TEMPLATES/paks"
BUILD_DIR="$PROJECT_ROOT/build"

# Note: Tool paks (workspace/all/paks/) are handled by the Makefile system target,
# not this script. This script only handles emulator paks and direct paks.

# Check if jq is available
if ! command -v jq &> /dev/null; then
    echo "Error: jq is required but not installed."
    echo "Install with: brew install jq"
    exit 1
fi

# Load platform metadata
PLATFORMS_JSON="$TEMPLATE_DIR/platforms.json"
CORES_JSON="$TEMPLATE_DIR/cores.json"

if [ ! -f "$PLATFORMS_JSON" ]; then
    echo "Error: platforms.json not found at $PLATFORMS_JSON"
    exit 1
fi

if [ ! -f "$CORES_JSON" ]; then
    echo "Error: cores.json not found at $CORES_JSON"
    exit 1
fi

# Parse arguments
TARGET_PLATFORM="${1:-all}"
shift || true
TARGET_CORES=("$@")

# Get all platforms
ALL_PLATFORMS=$(jq -r '.platforms | keys[]' "$PLATFORMS_JSON")

# Determine which platforms to generate
if [ "$TARGET_PLATFORM" = "all" ]; then
    PLATFORMS_TO_GENERATE="$ALL_PLATFORMS"
else
    PLATFORMS_TO_GENERATE="$TARGET_PLATFORM"
fi

# Parallel job count (use nproc if available, fallback to 4)
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Export variables for parallel subprocesses
export TEMPLATE_DIR BUILD_DIR PLATFORMS_JSON CORES_JSON

# Function to generate a single pak (called in parallel)
# Arguments: platform core
generate_pak() {
    local platform=$1
    local core=$2

    # Get metadata (inline jq calls for subprocess isolation)
    local nice_prefix=$(jq -r ".platforms.\"$platform\".nice_prefix" "$PLATFORMS_JSON")
    local emu_exe=$(jq -r ".stock_cores.\"$core\".emu_exe" "$CORES_JSON")

    # Create output directory
    local output_dir="$BUILD_DIR/SYSTEM/$platform/paks/Emus/${core}.pak"
    mkdir -p "$output_dir"

    # Generate launch.sh from template
    local launch_template="$TEMPLATE_DIR/launch.sh.template"
    if [ -f "$launch_template" ]; then
        sed -e "s|{{EMU_EXE}}|$emu_exe|g" \
            -e "s|{{NICE_PREFIX}}|$nice_prefix|g" \
            "$launch_template" > "$output_dir/launch.sh"
        chmod +x "$output_dir/launch.sh"
    fi

    # Copy config files - base first, then platform-specific overwrites
    local cfg_base_dir="$TEMPLATE_DIR/configs/base/${core}"
    local cfg_platform_dir="$TEMPLATE_DIR/configs/${platform}/${core}"

    if [ -d "$cfg_base_dir" ]; then
        cp "$cfg_base_dir"/*.cfg "$output_dir/" 2>/dev/null || true
    fi
    if [ -d "$cfg_platform_dir" ]; then
        cp "$cfg_platform_dir"/*.cfg "$output_dir/" 2>/dev/null || true
    fi

}
export -f generate_pak

# Function to check if core is compatible with platform architecture
is_core_compatible() {
    local platform=$1
    local core=$2

    local platform_arch=$(jq -r ".platforms.\"$platform\".arch" "$PLATFORMS_JSON")
    local arm64_only=$(jq -r ".stock_cores.\"$core\".arm64_only // false" "$CORES_JSON")

    if [ "$platform_arch" = "arm32" ] && [ "$arm64_only" = "true" ]; then
        return 1
    fi
    return 0
}

# Function to check if core is in target list
core_in_target_list() {
    local core=$1
    if [ ${#TARGET_CORES[@]} -eq 0 ]; then
        return 0
    fi
    local target
    for target in "${TARGET_CORES[@]}"; do
        [ "$target" = "$core" ] && return 0
    done
    return 1
}

# Main generation
echo "Generating emulator paks..."

# Build list of all platform/core pairs to generate
WORK_LIST=""
for platform in $PLATFORMS_TO_GENERATE; do
    CORES=$(jq -r '.stock_cores | keys[]' "$CORES_JSON")

    for core in $CORES; do
        # Filter by target cores if specified
        if ! core_in_target_list "$core"; then
            continue
        fi

        # Check architecture compatibility
        if ! is_core_compatible "$platform" "$core"; then
            continue
        fi

        WORK_LIST="$WORK_LIST$platform $core"$'\n'
    done
done

# Generate paks in parallel using xargs
PAK_COUNT=$(echo "$WORK_LIST" | grep -c . || echo 0)
echo "$WORK_LIST" | xargs -P "$PARALLEL_JOBS" -L 1 bash -c 'generate_pak "$@"' _

# Copy direct paks (sequential - few files)
for platform in $PLATFORMS_TO_GENERATE; do
    if [ -d "$DIRECT_PAKS_DIR" ]; then
        for pak in "$DIRECT_PAKS_DIR"/*.pak; do
            if [ -d "$pak" ]; then
                cp -r "$pak" "$BUILD_DIR/SYSTEM/$platform/paks/Emus/"
            fi
        done
    fi
done

echo "Generated $PAK_COUNT emulator paks"
