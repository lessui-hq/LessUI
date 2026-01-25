#!/bin/bash
# fetch-and-inject-lessos.sh - Download LessOS images and inject LessUI
#
# Fetches LessOS release images from GitHub and injects LessUI.zip into the
# storage partition (partition 2, ext4) using debugfs (no mount required).
#
# Usage:
#   ./scripts/fetch-and-inject-lessos.sh [options]
#
# Options:
#   -t, --tag TAG        LessOS release tag (default: latest)
#   -d, --device DEVICE  Device to fetch (RK3566, SM8250, or "all")
#   -v, --variant VAR    Variant for RK3566 (Generic, Specific, or "all")
#   -o, --output DIR     Output directory (default: ./build/LESSOS)
#   -n, --dry-run        Show what would be done without doing it
#   -h, --help           Show this help
#
# Example:
#   ./scripts/fetch-and-inject-lessos.sh --device RK3566 --variant Generic
#   ./scripts/fetch-and-inject-lessos.sh --device all --tag 20260124

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Defaults
TAG="latest"
DEVICE=""
VARIANT=""
OUTPUT_DIR="${PROJECT_ROOT}/build/LESSOS"
LESSUI_ZIP=""
DRY_RUN=false
REPO="lessui-hq/LessOS"

usage() {
    sed -n '2,/^[^#]/p' "$0" | grep '^#' | cut -c3-
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--tag)
            [[ -z "${2:-}" || "$2" == -* ]] && { echo "Error: $1 requires a value"; exit 1; }
            TAG="$2"; shift 2 ;;
        -d|--device)
            [[ -z "${2:-}" || "$2" == -* ]] && { echo "Error: $1 requires a value"; exit 1; }
            DEVICE="$2"; shift 2 ;;
        -v|--variant)
            [[ -z "${2:-}" || "$2" == -* ]] && { echo "Error: $1 requires a value"; exit 1; }
            VARIANT="$2"; shift 2 ;;
        -o|--output)
            [[ -z "${2:-}" || "$2" == -* ]] && { echo "Error: $1 requires a value"; exit 1; }
            OUTPUT_DIR="$2"; shift 2 ;;
        -n|--dry-run) DRY_RUN=true; shift ;;
        -h|--help) usage ;;
        *) echo "Error: Unknown option: $1"; exit 1 ;;
    esac
done

# GitHub API helper
github_api() {
    local endpoint="$1"
    local response http_code

    response=$(curl -sL --connect-timeout 30 -w '\n%{http_code}' "https://api.github.com/${endpoint}")
    http_code=$(echo "$response" | tail -1)

    if [[ "$http_code" != "200" ]]; then
        echo "Error: GitHub API returned HTTP ${http_code} for ${endpoint}" >&2
        exit 1
    fi

    echo "$response" | head -n -1
}

# Resolve "latest" to actual tag
resolve_tag() {
    if [[ "$TAG" == "latest" ]]; then
        TAG=$(github_api "repos/${REPO}/releases/latest" | jq -r '.tag_name')
        if [[ -z "$TAG" || "$TAG" == "null" ]]; then
            echo "Error: Could not resolve latest release tag" >&2
            exit 1
        fi
        echo "Resolved latest tag: ${TAG}"
    fi
}

# Get image assets for the release
get_release_assets() {
    github_api "repos/${REPO}/releases/tags/${TAG}" | jq -r '.assets[].name' | grep '\.img\.gz$'
}

# Download a release asset
download_asset() {
    local asset_name="$1"
    local output_path="$2"

    if [[ "$DRY_RUN" == true ]]; then
        echo "[DRY-RUN] Would download: ${asset_name}"
        return 0
    fi

    echo "Downloading: ${asset_name}"
    local url="https://github.com/${REPO}/releases/download/${TAG}/${asset_name}"
    local http_code
    http_code=$(curl -sL --connect-timeout 30 --max-time 1800 -w '%{http_code}' -o "$output_path" "$url")

    if [[ "$http_code" != "200" ]]; then
        echo "Error: Failed to download ${asset_name}: HTTP ${http_code}" >&2
        rm -f "$output_path"
        return 1
    fi

    if [[ ! -s "$output_path" ]]; then
        echo "Error: Downloaded file is empty: ${asset_name}" >&2
        rm -f "$output_path"
        return 1
    fi
}

# Contents to include in LessOS builds (excludes platform-specific boot folders)
LESSOS_INCLUDE=(bin Bios lessos LessUI.7z README.txt Roms Saves Tools)

# Create LessOS-specific zip from build/BASE
prepare_lessui_zip() {
    local base_dir="${PROJECT_ROOT}/build/BASE"

    if [[ ! -d "$base_dir" ]]; then
        echo "Error: build/BASE not found. Run 'make all' first." >&2
        exit 1
    fi

    echo "Creating LessOS-specific zip from build/BASE..."
    mkdir -p "$OUTPUT_DIR"
    LESSUI_ZIP="${OUTPUT_DIR}/LessUI-lessos.zip"
    rm -f "$LESSUI_ZIP"

    if ! (cd "$base_dir" && zip -rq "$LESSUI_ZIP" "${LESSOS_INCLUDE[@]}"); then
        echo "Error: Failed to create LessOS zip" >&2
        exit 1
    fi

    if [[ ! -s "$LESSUI_ZIP" ]]; then
        echo "Error: LessOS zip is empty" >&2
        exit 1
    fi

    echo "Created: ${LESSUI_ZIP}"
}

# Get partition 2 offset and size from disk image
get_partition2_info() {
    local img_path="$1"
    local part_info

    part_info=$(sfdisk -d "$img_path" 2>/dev/null | grep "^${img_path}2" | head -1)

    if [[ -z "$part_info" ]]; then
        echo "Error: Could not find partition 2 in image" >&2
        return 1
    fi

    local start size
    [[ "$part_info" =~ start=\ *([0-9]+) ]] && start="${BASH_REMATCH[1]}"
    [[ "$part_info" =~ size=\ *([0-9]+) ]] && size="${BASH_REMATCH[1]}"

    if [[ -z "$start" || -z "$size" ]]; then
        echo "Error: Could not parse partition info" >&2
        return 1
    fi

    echo "$start $size"
}

# Inject LessUI.zip into partition 2 of the image
inject_lessui() {
    local img_path="$1"
    local zip_path="$2"

    echo "Injecting LessUI.zip into image..."

    local part_info start_sector size_sectors
    part_info=$(get_partition2_info "$img_path")
    read -r start_sector size_sectors <<< "$part_info"

    if [[ "$DRY_RUN" == true ]]; then
        echo "[DRY-RUN] Would inject into partition 2 at sector ${start_sector}"
        return 0
    fi

    local tmp_dir part2_img
    tmp_dir=$(mktemp -d)
    part2_img="${tmp_dir}/part2.ext4"

    # Extract partition
    if ! dd if="$img_path" of="$part2_img" bs=512 skip="$start_sector" count="$size_sectors" status=none; then
        echo "Error: Failed to extract partition 2" >&2
        rm -rf "$tmp_dir"
        return 1
    fi

    # Inject file using debugfs
    if ! debugfs -w -R "write ${zip_path} /LessUI.zip" "$part2_img" 2>/dev/null; then
        echo "Error: debugfs injection failed" >&2
        rm -rf "$tmp_dir"
        return 1
    fi

    # Verify injection
    if ! debugfs -R "stat /LessUI.zip" "$part2_img" &>/dev/null; then
        echo "Error: Injection verification failed" >&2
        rm -rf "$tmp_dir"
        return 1
    fi

    # Write partition back
    if ! dd if="$part2_img" of="$img_path" bs=512 seek="$start_sector" conv=notrunc status=none; then
        echo "Error: Failed to write partition back to image" >&2
        rm -rf "$tmp_dir"
        return 1
    fi

    rm -rf "$tmp_dir"
    echo "Injection complete"
}

# Process a single image file
process_image() {
    local asset_name="$1"
    local download_dir="$2"

    local gz_path="${download_dir}/${asset_name}"
    local img_path="${gz_path%.gz}"
    local output_gz="${OUTPUT_DIR}/${asset_name}"

    download_asset "$asset_name" "$gz_path"

    if [[ "$DRY_RUN" == true ]]; then
        echo "[DRY-RUN] Would decompress, inject, and recompress"
        return 0
    fi

    echo "Decompressing..."
    gunzip -k "$gz_path"

    inject_lessui "$img_path" "$LESSUI_ZIP"

    echo "Recompressing..."
    pigz --best --force "$img_path"

    mv "${img_path}.gz" "$output_gz"
    rm -f "$gz_path"

    # Create checksum
    (cd "$OUTPUT_DIR" && sha256sum "$(basename "$output_gz")" > "$(basename "$output_gz").sha256")

    echo "Output: ${output_gz}"
}

# Main
main() {
    echo "LessOS Image Fetch and Inject"
    echo "=============================="

    [[ "$DRY_RUN" != true ]] && mkdir -p "$OUTPUT_DIR"

    resolve_tag

    if [[ "$DRY_RUN" == true ]]; then
        echo "[DRY-RUN] Would create LessOS zip from build/BASE"
    else
        prepare_lessui_zip
    fi

    # Get and filter assets
    local assets
    assets=$(get_release_assets || true)

    if [[ -z "$assets" ]]; then
        echo "Error: No image assets found for tag: ${TAG}" >&2
        exit 1
    fi

    [[ -n "$DEVICE" && "$DEVICE" != "all" ]] && assets=$(echo "$assets" | grep -i "$DEVICE" || true)
    [[ -n "$VARIANT" && "$VARIANT" != "all" ]] && assets=$(echo "$assets" | grep -i "$VARIANT" || true)

    if [[ -z "$assets" ]]; then
        echo "Error: No matching assets for device=${DEVICE:-any} variant=${VARIANT:-any}" >&2
        exit 1
    fi

    echo "Assets to process:"
    while IFS= read -r a; do echo "  - $a"; done <<< "$assets"

    # Setup temp directory with cleanup
    DOWNLOAD_DIR=$(mktemp -d)
    trap 'rm -rf "$DOWNLOAD_DIR"' EXIT

    # Process each asset
    while IFS= read -r asset; do
        [[ -z "$asset" ]] && continue
        echo ""
        echo "Processing: ${asset}"
        echo "----------------------------------------"
        process_image "$asset" "$DOWNLOAD_DIR"
    done <<< "$assets"

    echo ""
    echo "All images processed!"
    echo "Output: ${OUTPUT_DIR}"
}

main "$@"
