#!/bin/bash
# Helper script to compare emulator configs across platforms

CONFIGS_DIR="skeleton/TEMPLATES/minarch-paks/configs"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

usage() {
    cat <<EOF
Usage: $0 <command> [args]

Commands:
  core <CORE>              Show how all platforms handle a specific core (e.g., FC, GB)
  platform <PLATFORM>      Show all custom configs for a platform
  diff <CORE> <PLATFORM>   Show detailed diff between default and platform config
  summary                  Show overview of all customizations
  list                     List all available cores and platforms

Examples:
  $0 core GB               # Compare GB configs across all platforms
  $0 platform rgb30        # Show what rgb30 customizes
  $0 diff PS trimuismart   # Detailed diff of trimuismart's PS config
  $0 summary               # Overview table
EOF
    exit 1
}

cmd_core() {
    local core=$1
    if [ -z "$core" ]; then
        echo "Error: Please specify a core (e.g., FC, GB, GBA)"
        exit 1
    fi

    echo -e "${BLUE}=== Config comparison for $core ===${NC}\n"

    # Show default first
    if [ -f "$CONFIGS_DIR/default/${core}.cfg" ]; then
        echo -e "${GREEN}DEFAULT:${NC}"
        head -3 "$CONFIGS_DIR/default/${core}.cfg" | sed 's/^/  /'
        echo
    fi

    # Show platform-specific configs
    for platform_dir in "$CONFIGS_DIR"/*; do
        local platform=$(basename "$platform_dir")
        if [ "$platform" = "default" ]; then
            continue
        fi

        if [ -f "$platform_dir/${core}.cfg" ]; then
            echo -e "${YELLOW}$platform (CUSTOM):${NC}"
            head -3 "$platform_dir/${core}.cfg" | sed 's/^/  /'
            echo
        else
            echo -e "$platform: ${GREEN}uses default${NC}"
        fi
    done
}

cmd_platform() {
    local platform=$1
    if [ -z "$platform" ]; then
        echo "Error: Please specify a platform (e.g., miyoomini, trimuismart)"
        exit 1
    fi

    echo -e "${BLUE}=== Custom configs for $platform ===${NC}\n"

    if [ ! -d "$CONFIGS_DIR/$platform" ]; then
        echo -e "${GREEN}No custom configs - uses all defaults${NC}"
        return
    fi

    for cfg in "$CONFIGS_DIR/$platform"/*.cfg; do
        if [ -f "$cfg" ]; then
            local core=$(basename "$cfg" .cfg)
            echo -e "${YELLOW}$core:${NC}"
            head -2 "$cfg" | sed 's/^/  /'
            echo
        fi
    done
}

cmd_diff() {
    local core=$1
    local platform=$2

    if [ -z "$core" ] || [ -z "$platform" ]; then
        echo "Error: Please specify both core and platform"
        exit 1
    fi

    local default_cfg="$CONFIGS_DIR/default/${core}.cfg"
    local platform_cfg="$CONFIGS_DIR/$platform/${core}.cfg"

    if [ ! -f "$default_cfg" ]; then
        echo "Error: Default config not found: $default_cfg"
        exit 1
    fi

    if [ ! -f "$platform_cfg" ]; then
        echo -e "${GREEN}$platform uses default config for $core${NC}"
        exit 0
    fi

    echo -e "${BLUE}=== Diff: default vs $platform for $core ===${NC}\n"
    diff -u "$default_cfg" "$platform_cfg" | head -30
}

cmd_summary() {
    echo -e "${BLUE}=== Config Customization Summary ===${NC}\n"

    # Count total configs per platform
    printf "%-15s %s\n" "PLATFORM" "CUSTOM CONFIGS"
    printf "%-15s %s\n" "--------" "--------------"

    for platform_dir in "$CONFIGS_DIR"/*; do
        local platform=$(basename "$platform_dir")
        if [ "$platform" = "default" ]; then
            local count=$(ls "$platform_dir"/*.cfg 2>/dev/null | wc -l)
            printf "%-15s %d cores (baseline)\n" "$platform" $count
        else
            local count=$(ls "$platform_dir"/*.cfg 2>/dev/null | wc -l)
            if [ $count -gt 0 ]; then
                printf "%-15s %d custom\n" "$platform" $count
            fi
        fi
    done

    echo
    echo -e "${BLUE}Platforms using all defaults:${NC}"
    for platform in miyoomini zero28 my282 my355 magicmini rg35xx; do
        if [ ! -d "$CONFIGS_DIR/$platform" ]; then
            echo "  ✓ $platform"
        fi
    done
}

cmd_list() {
    echo -e "${BLUE}=== Available Cores ===${NC}"
    ls "$CONFIGS_DIR/default"/*.cfg | xargs -n 1 basename | sed 's/.cfg$//' | column -c 80

    echo
    echo -e "${BLUE}=== Platforms with Customizations ===${NC}"
    for dir in "$CONFIGS_DIR"/*; do
        local platform=$(basename "$dir")
        if [ "$platform" != "default" ]; then
            echo "  $platform"
        fi
    done
}

# Main
case "$1" in
    core)
        cmd_core "$2"
        ;;
    platform)
        cmd_platform "$2"
        ;;
    diff)
        cmd_diff "$2" "$3"
        ;;
    summary)
        cmd_summary
        ;;
    list)
        cmd_list
        ;;
    *)
        usage
        ;;
esac
