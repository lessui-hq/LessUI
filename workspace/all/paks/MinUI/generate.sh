#!/bin/bash
# Generate launch.sh for a single platform
# Usage: ./generate.sh <platform> <build_dir>

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEMPLATE="$SCRIPT_DIR/launch.sh.template"
CONFIG_JSON="$SCRIPT_DIR/../config/platforms.json"

platform="$1"
build_dir="$2"
platform_dir="$SCRIPT_DIR/platforms/$platform"
output_dir="$build_dir/SYSTEM/$platform/paks/MinUI.pak"

# Read platform config from shared JSON
if ! jq -e ".platforms.\"$platform\"" "$CONFIG_JSON" > /dev/null 2>&1; then
	echo "  Warning: No config for $platform in platforms.json, skipping"
	exit 0
fi

echo "  Generating MinUI.pak for $platform"

# Extract values from JSON
arch=$(jq -r ".platforms.\"$platform\".arch" "$CONFIG_JSON")
sdcard_path=$(jq -r ".platforms.\"$platform\".sdcard_path" "$CONFIG_JSON")
shutdown_cmd=$(jq -r ".platforms.\"$platform\".shutdown_cmd" "$CONFIG_JSON")

# Derive values from arch
if [ "$arch" = "arm32" ]; then
	platform_arch="arm"
	cores_subpath=".system/cores/a7"
else
	platform_arch="arm64"
	cores_subpath=".system/cores/a53"
fi

mkdir -p "$output_dir"
cp "$TEMPLATE" "$output_dir/launch.sh"

# Substitute variables
sed -i.bak \
	-e "s|{{PLATFORM}}|$platform|g" \
	-e "s|{{PLATFORM_ARCH}}|$platform_arch|g" \
	-e "s|{{SDCARD_PATH}}|$sdcard_path|g" \
	-e "s|{{CORES_SUBPATH}}|$cores_subpath|g" \
	-e "s|{{SHUTDOWN_CMD}}|$shutdown_cmd|g" \
	"$output_dir/launch.sh"

# Substitute hooks (pre-init, init, late-init)
for hook in pre-init init late-init; do
	hook_file="$platform_dir/$hook.sh"
	marker="{{HOOK:$hook}}"

	if [ -f "$hook_file" ]; then
		tmp=$(mktemp)
		cat "$hook_file" > "$tmp"
		awk -v marker="$marker" -v file="$tmp" '
			$0 == marker { while ((getline line < file) > 0) print line; next }
			{ print }
		' "$output_dir/launch.sh" > "$output_dir/launch.sh.new"
		mv "$output_dir/launch.sh.new" "$output_dir/launch.sh"
		rm "$tmp"
	else
		grep -v "^${marker}$" "$output_dir/launch.sh" > "$output_dir/launch.sh.new" || true
		mv "$output_dir/launch.sh.new" "$output_dir/launch.sh"
	fi
done

rm -f "$output_dir/launch.sh.bak"
chmod +x "$output_dir/launch.sh"
