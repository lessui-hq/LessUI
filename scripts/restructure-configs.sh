#!/bin/bash
# Restructure config templates to mirror pak output structure
# Converts: configs/default/{TAG}.cfg → configs/base/{TAG}/default.cfg

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIGS_DIR="$SCRIPT_DIR/../skeleton/TEMPLATES/minarch-paks/configs"

cd "$CONFIGS_DIR"

echo "Restructuring config templates..."

# Step 1: Create base/ and restructure default/ configs
if [ -d "default" ]; then
    mkdir -p base

    for cfg_file in default/*.cfg; do
        if [ -f "$cfg_file" ]; then
            tag=$(basename "$cfg_file" .cfg)
            echo "  Moving $cfg_file → base/$tag/default.cfg"
            mkdir -p "base/$tag"
            mv "$cfg_file" "base/$tag/default.cfg"
        fi
    done

    rmdir default
    echo "✓ Converted default/ → base/{TAG}/default.cfg"
fi

# Step 2: Restructure platform override directories
for platform_dir in */; do
    platform=$(basename "$platform_dir")

    # Skip base and devices directories
    if [ "$platform" = "base" ] || [ "$platform" = "devices" ]; then
        continue
    fi

    echo "Processing platform: $platform"

    # Move {platform}/{TAG}.cfg → {platform}/{TAG}/default.cfg
    if ls "$platform"/*.cfg 1> /dev/null 2>&1; then
        for cfg_file in "$platform"/*.cfg; do
            if [ -f "$cfg_file" ]; then
                tag=$(basename "$cfg_file" .cfg)
                echo "  Moving $cfg_file → $platform/$tag/default.cfg"
                mkdir -p "$platform/$tag"
                mv "$cfg_file" "$platform/$tag/default.cfg"
            fi
        done
    fi
done

echo ""
echo "✓ Config restructure complete!"
echo ""
echo "New structure:"
tree -L 3 -I 'devices' . | head -30
