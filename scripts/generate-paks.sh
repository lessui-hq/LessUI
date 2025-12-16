#!/bin/bash
# Generate platform-specific paks from templates
#
# This script generates all .pak directories for all platforms from canonical templates.
# - Emulator paks: Generated from workspace/all/paks/Emus/
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

# Load platform metadata from shared config
PLATFORMS_JSON="$PROJECT_ROOT/workspace/all/paks/config/platforms.json"
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

# Merge two config files with last-one-wins semantics
# Usage: merge_configs base_file override_file output_file
# If override_file doesn't exist or is empty, just copies base_file
merge_configs() {
    local base_file="$1"
    local override_file="$2"
    local output_file="$3"

    # If no override, just copy base
    if [ ! -f "$override_file" ]; then
        cp "$base_file" "$output_file"
        return
    fi

    # Use awk to merge: base values first, then override values win
    awk '
    BEGIN { key_count = 0 }

    # Function to extract key from a line (handles "bind X = Y" and "key = value" and "-key = value")
    function get_key(line) {
        # Remove leading/trailing whitespace
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)

        # Skip empty lines
        if (line == "") return ""

        # Handle locked prefix
        if (substr(line, 1, 1) == "-") {
            line = substr(line, 2)
        }

        # Find the = sign
        eq_pos = index(line, "=")
        if (eq_pos == 0) return ""

        key = substr(line, 1, eq_pos - 1)
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", key)
        return key
    }

    # First pass: read base file, track order
    FNR == NR {
        key = get_key($0)
        if (key != "") {
            if (!(key in values)) {
                order[key_count++] = key
            }
            values[key] = $0
        } else if ($0 ~ /^[[:space:]]*$/) {
            # Preserve empty lines from base as placeholders
            order[key_count++] = "__EMPTY__" key_count
            values["__EMPTY__" key_count] = ""
        }
        next
    }

    # Second pass: read override file, update values
    {
        key = get_key($0)
        if (key != "") {
            if (!(key in values)) {
                # New key from override, add to end
                order[key_count++] = key
            }
            values[key] = $0
        }
    }

    END {
        for (i = 0; i < key_count; i++) {
            key = order[i]
            print values[key]
        }
    }
    ' "$base_file" "$override_file" > "$output_file"
}
export -f merge_configs

# Function to generate a single pak (called in parallel)
# Arguments: platform core
generate_pak() {
    local platform=$1
    local core=$2

    # Get metadata (inline jq calls for subprocess isolation)
    local nice_prefix=$(jq -r ".platforms.\"$platform\".nice_prefix" "$PLATFORMS_JSON")
    local core_so=$(jq -r ".cores.\"$core\".core" "$CORES_JSON")

    # Create output directory
    local output_dir="$BUILD_DIR/SYSTEM/$platform/paks/Emus/${core}.pak"
    mkdir -p "$output_dir"

    # Generate launch.sh from template
    local launch_template="$TEMPLATE_DIR/launch.sh.template"
    if [ -f "$launch_template" ]; then
        sed -e "s|{{CORE}}|$core_so|g" \
            -e "s|{{NICE_PREFIX}}|$nice_prefix|g" \
            "$launch_template" > "$output_dir/launch.sh"
        chmod +x "$output_dir/launch.sh"
        # Preserve newest timestamp among all dependencies for rsync
        # Find the newest file among template, platforms.json, and cores.json
        local newest="$launch_template"
        [ "$PLATFORMS_JSON" -nt "$newest" ] && newest="$PLATFORMS_JSON"
        [ "$CORES_JSON" -nt "$newest" ] && newest="$CORES_JSON"
        touch -r "$newest" "$output_dir/launch.sh"
    fi

    # Merge config files - base provides defaults, platform overrides specific values
    local cfg_base_dir="$TEMPLATE_DIR/configs/base/${core}"
    local cfg_platform_dir="$TEMPLATE_DIR/configs/${platform}/${core}"
    local base_default="$cfg_base_dir/default.cfg"

    # Skip if no base config exists
    if [ ! -f "$base_default" ]; then
        return
    fi

    # Generate default.cfg: merge base + platform override
    merge_configs "$base_default" "$cfg_platform_dir/default.cfg" "$output_dir/default.cfg"

    # Generate device-variant configs (default-{device}.cfg)
    # These inherit from base/default.cfg, then apply platform device-specific overrides
    # Check both base and platform for device variants
    local device_cfgs=""
    [ -d "$cfg_base_dir" ] && device_cfgs="$device_cfgs $(ls "$cfg_base_dir"/default-*.cfg 2>/dev/null || true)"
    [ -d "$cfg_platform_dir" ] && device_cfgs="$device_cfgs $(ls "$cfg_platform_dir"/default-*.cfg 2>/dev/null || true)"

    # Get unique device variant names
    for cfg_path in $device_cfgs; do
        [ -z "$cfg_path" ] && continue
        local cfg_name=$(basename "$cfg_path")
        local device_tag="${cfg_name#default-}"
        device_tag="${device_tag%.cfg}"

        # Skip if already processed (dedup)
        [ -f "$output_dir/$cfg_name" ] && continue

        # Device variants inherit from base/default.cfg, then apply device-specific overrides
        # Priority: base/default.cfg -> base/default-{device}.cfg -> platform/default-{device}.cfg
        local base_device="$cfg_base_dir/$cfg_name"
        local platform_device="$cfg_platform_dir/$cfg_name"

        if [ -f "$base_device" ]; then
            # Base has device variant - merge base/default.cfg + base/default-{device}.cfg first
            merge_configs "$base_default" "$base_device" "$output_dir/$cfg_name.tmp"
            # Then merge platform override if exists
            merge_configs "$output_dir/$cfg_name.tmp" "$platform_device" "$output_dir/$cfg_name"
            rm -f "$output_dir/$cfg_name.tmp"
        else
            # No base device variant - merge base/default.cfg + platform/default-{device}.cfg
            merge_configs "$base_default" "$platform_device" "$output_dir/$cfg_name"
        fi
    done

}
export -f generate_pak

# Function to copy shared maps to common directory (called once per pak)
# Maps are organized by core name (e.g., maps/fbneo_libretro/map.txt)
# Multiple paks may share the same core, so we copy to each pak that uses it
copy_shared_maps() {
    local core=$1

    # Get the core name from cores.json (no .so extension)
    local core_name=$(jq -r ".cores.\"$core\".core" "$CORES_JSON")

    if [ -z "$core_name" ] || [ "$core_name" = "null" ]; then
        return
    fi

    # Map directory matches core name exactly
    local map_file="$TEMPLATE_DIR/maps/${core_name}/map.txt"

    if [ -f "$map_file" ]; then
        local common_pak_dir="$BUILD_DIR/SYSTEM/common/paks/Emus/${core}.pak"
        mkdir -p "$common_pak_dir"
        cp "$map_file" "$common_pak_dir/map.txt"
        echo "  Copied shared map: ${core}.pak/map.txt (from ${core_name})"
    fi
}
export -f copy_shared_maps

# Function to check if core is compatible with platform architecture
is_core_compatible() {
    local platform=$1
    local core=$2

    local platform_arch=$(jq -r ".platforms.\"$platform\".arch" "$PLATFORMS_JSON")
    local arm64_only=$(jq -r ".cores.\"$core\".arm64_only // false" "$CORES_JSON")

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
    CORES=$(jq -r '.cores | keys[]' "$CORES_JSON")

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
                rsync -a "$pak/" "$BUILD_DIR/SYSTEM/$platform/paks/Emus/$(basename "$pak")/"
            fi
        done
    fi
done

# Copy shared maps to common directory (once, not per-platform)
echo "Copying shared pak maps to .system/common/..."
CORES=$(jq -r '.cores | keys[]' "$CORES_JSON")
for core in $CORES; do
    # Filter by target cores if specified
    if ! core_in_target_list "$core"; then
        continue
    fi
    copy_shared_maps "$core"
done

echo "Generated $PAK_COUNT emulator paks"
