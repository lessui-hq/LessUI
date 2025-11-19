#!/bin/bash
# Generate platform-specific paks from templates
#
# This script generates all .pak directories for all platforms from canonical templates.
# Templates are in skeleton/TEMPLATES/paks/ and metadata in skeleton/TEMPLATES/*.json
#
# Usage:
#   ./scripts/generate-paks.sh                    # Generate all paks for all platforms
#   ./scripts/generate-paks.sh miyoomini          # Generate paks for specific platform
#   ./scripts/generate-paks.sh miyoomini GB GBA   # Generate specific paks for platform

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEMPLATE_DIR="$PROJECT_ROOT/skeleton/TEMPLATES/minarch-paks"
DIRECT_PAKS_DIR="$PROJECT_ROOT/skeleton/TEMPLATES/paks"
BUILD_DIR="$PROJECT_ROOT/build"

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

# Function to get platform metadata
get_platform_metadata() {
    local platform=$1
    local key=$2
    jq -r ".platforms.\"$platform\".\"$key\"" "$PLATFORMS_JSON"
}

# Function to get core metadata
get_core_metadata() {
    local core_type=$1  # "stock_cores" or "extra_cores"
    local core=$2
    local key=$3
    jq -r ".${core_type}.\"$core\".\"$key\"" "$CORES_JSON"
}

# Function to check if core requires ARM64
is_arm64_only() {
    local core_type=$1
    local core=$2
    local arm64_only=$(jq -r ".${core_type}.\"$core\".arm64_only // false" "$CORES_JSON")
    [ "$arm64_only" = "true" ]
}

# Function to check if core is compatible with platform architecture
is_core_compatible_with_platform() {
    local platform=$1
    local core_type=$2
    local core=$3

    # Get platform architecture
    local platform_arch=$(jq -r ".platforms.\"$platform\".arch" "$PLATFORMS_JSON")

    # If core is arm64_only and platform is arm32, skip
    if [ "$platform_arch" = "arm32" ] && is_arm64_only "$core_type" "$core"; then
        return 1  # Not compatible
    fi

    return 0  # Compatible
}

# Function to check if core is in target list
core_in_target_list() {
    local core=$1
    if [ ${#TARGET_CORES[@]} -eq 0 ]; then
        return 0  # No filter, include all
    fi
    local target
    for target in "${TARGET_CORES[@]}"; do
        if [ "$target" = "$core" ]; then
            return 0  # Found
        fi
    done
    return 1  # Not found
}

# Function to generate a pak
generate_pak() {
    local platform=$1
    local core=$2
    local core_type=$3  # "stock" or "extra"
    local output_base=$4  # "SYSTEM" or "EXTRAS"

    echo "  Generating ${core}.pak for $platform ($core_type)"

    # Get metadata
    local nice_prefix=$(get_platform_metadata "$platform" "nice_prefix")

    local cores_json_type="${core_type}_cores"
    local emu_exe=$(get_core_metadata "$cores_json_type" "$core" "emu_exe")

    # Create output directory
    local output_dir="$BUILD_DIR/$output_base"
    if [ "$output_base" = "SYSTEM" ]; then
        output_dir="$output_dir/$platform/paks/Emus/${core}.pak"
    else
        output_dir="$output_dir/Emus/$platform/${core}.pak"
    fi

    mkdir -p "$output_dir"

    # Generate launch.sh
    local launch_output="$output_dir/launch.sh"

    # Generate from template with substitutions
    local launch_template="$TEMPLATE_DIR/launch.sh.template"
    if [ -f "$launch_template" ]; then
        sed -e "s|{{EMU_EXE}}|$emu_exe|g" \
            -e "s|{{NICE_PREFIX}}|$nice_prefix|g" \
            "$launch_template" > "$launch_output"
        chmod +x "$launch_output"
    fi

    # Generate config files - copy from base, then overwrite/add with platform-specific
    # This mirrors the pak output structure: {TAG}/default.cfg, default-{device}.cfg, etc.

    local cfg_base_dir="$TEMPLATE_DIR/configs/base/${core}"
    local cfg_platform_dir="$TEMPLATE_DIR/configs/${platform}/${core}"

    # Copy base configs first (if exists)
    if [ -d "$cfg_base_dir" ]; then
        cp "$cfg_base_dir"/*.cfg "$output_dir/" 2>/dev/null || true
    fi

    # Copy/overwrite with platform-specific configs (if exists)
    if [ -d "$cfg_platform_dir" ]; then
        cp "$cfg_platform_dir"/*.cfg "$output_dir/" 2>/dev/null || true
    fi
}

# Main generation loop
echo "Generating paks from templates..."
echo "Template dir: $TEMPLATE_DIR"
echo "Output dir: $BUILD_DIR"
echo ""

for platform in $PLATFORMS_TO_GENERATE; do
    echo "Platform: $platform"

    # Get all cores from stock_cores
    CORES=$(jq -r '.stock_cores | keys[]' "$CORES_JSON")

    # Generate all core paks (SYSTEM)
    for core in $CORES; do
        # If specific cores requested, filter
        if ! core_in_target_list "$core"; then
            continue
        fi

        # Check architecture compatibility
        if ! is_core_compatible_with_platform "$platform" "stock_cores" "$core"; then
            echo "  Skipping $core (requires ARM64)"
            continue
        fi

        generate_pak "$platform" "$core" "stock" "SYSTEM"
    done

    # Copy direct paks (non-template paks like PAK.pak)
    if [ -d "$DIRECT_PAKS_DIR" ]; then
        echo "  Copying direct paks..."
        for pak in "$DIRECT_PAKS_DIR"/*.pak; do
            if [ -d "$pak" ]; then
                pak_name=$(basename "$pak")
                echo "    Copying ${pak_name}"
                cp -r "$pak" "$BUILD_DIR/SYSTEM/$platform/paks/Emus/"
            fi
        done
    fi

    echo ""
done

echo "Pak generation complete!"
