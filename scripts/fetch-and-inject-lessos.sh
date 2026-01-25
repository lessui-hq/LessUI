#!/bin/bash
# fetch-and-inject-lessos.sh - Download LessOS images and inject LessUI
#
# This script fetches LessOS release images from GitHub and injects LessUI.zip
# into the storage partition (partition 2, ext4). The injection is done without
# mounting the filesystem, using debugfs.
#
# Usage:
#   ./scripts/fetch-and-inject-lessos.sh [options]
#
# Options:
#   -t, --tag TAG        LessOS release tag (default: latest)
#   -d, --device DEVICE  Device to fetch (RK3566, SM8250, or "all")
#   -v, --variant VAR    Variant for RK3566 (Generic, Specific, or "all")
#   -z, --zip PATH       Path to LessUI.zip (default: build from releases/)
#   -o, --output DIR     Output directory (default: ./build/LESSOS)
#   -n, --dry-run        Show what would be done without doing it
#   -h, --help           Show this help
#
# Requirements:
#   - curl
#   - gunzip, pigz (for compression)
#   - debugfs (from e2fsprogs, for ext4 injection)
#   - sfdisk (for partition info)
#
# Example:
#   ./scripts/fetch-and-inject-lessos.sh --device RK3566 --variant Generic
#   ./scripts/fetch-and-inject-lessos.sh --device all --tag 20260124

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Defaults
TAG="latest"
DEVICE=""
VARIANT=""
LESSUI_ZIP=""
OUTPUT_DIR="${PROJECT_ROOT}/build/LESSOS"
DRY_RUN=false
REPO="lessui-hq/LessOS"

# Logging functions
log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_success() { echo -e "${GREEN}[OK]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

usage() {
    cat << 'EOF'
Usage:
  ./scripts/fetch-and-inject-lessos.sh [options]

Options:
  -t, --tag TAG        LessOS release tag (default: latest)
  -d, --device DEVICE  Device to fetch (RK3566, SM8250, or "all")
  -v, --variant VAR    Variant for RK3566 (Generic, Specific, or "all")
  -z, --zip PATH       Path to LessUI.zip (default: build from releases/)
  -o, --output DIR     Output directory (default: ./build/LESSOS)
  -n, --dry-run        Show what would be done without doing it
  -h, --help           Show this help

Example:
  ./scripts/fetch-and-inject-lessos.sh --device RK3566 --variant Generic
  ./scripts/fetch-and-inject-lessos.sh --device all --tag 20260124
EOF
    exit 0
}

# Validate option argument (not empty and not another flag)
require_arg() {
    local opt="$1"
    local val="${2:-}"
    if [[ -z "$val" || "$val" == -* ]]; then
        log_error "$opt requires a value"
        exit 1
    fi
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--tag) require_arg "$1" "${2:-}"; TAG="$2"; shift 2 ;;
        -d|--device) require_arg "$1" "${2:-}"; DEVICE="$2"; shift 2 ;;
        -v|--variant) require_arg "$1" "${2:-}"; VARIANT="$2"; shift 2 ;;
        -z|--zip) require_arg "$1" "${2:-}"; LESSUI_ZIP="$2"; shift 2 ;;
        -o|--output) require_arg "$1" "${2:-}"; OUTPUT_DIR="$2"; shift 2 ;;
        -n|--dry-run) DRY_RUN=true; shift ;;
        -h|--help) usage ;;
        *) log_error "Unknown option: $1"; usage ;;
    esac
done

# Check required tools
check_tools() {
    local missing=()

    for tool in curl gunzip sfdisk; do
        if ! command -v "$tool" &>/dev/null; then
            missing+=("$tool")
        fi
    done

    # Check for debugfs (required for ext4 injection)
    if ! command -v debugfs &>/dev/null; then
        missing+=("debugfs (from e2fsprogs)")
    fi

    # Check for pigz or gzip
    if ! command -v pigz &>/dev/null && ! command -v gzip &>/dev/null; then
        missing+=("pigz or gzip")
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        log_error "Missing required tools: ${missing[*]}"
        log_error "Install with: apt-get install curl e2fsprogs fdisk pigz"
        exit 1
    fi
}

# Fetch JSON from GitHub API with error handling
github_api() {
    local endpoint="$1"
    local response http_code body

    response=$(curl -sL -w '\n%{http_code}' "https://api.github.com/${endpoint}")
    http_code=$(echo "$response" | tail -1)
    body=$(echo "$response" | head -n -1)

    if [[ "$http_code" != "200" ]]; then
        log_error "GitHub API returned HTTP ${http_code} for ${endpoint}"
        exit 1
    fi

    echo "$body"
}

# Get release tag (resolve "latest" to actual tag)
get_release_tag() {
    if [[ "$TAG" == "latest" ]]; then
        local response result
        response=$(github_api "repos/${REPO}/releases/latest")
        result=$(echo "$response" | grep '"tag_name"' | sed 's/.*: "\([^"]*\)".*/\1/')

        if [[ -z "$result" ]]; then
            log_error "Could not parse release tag from GitHub API response"
            exit 1
        fi
        TAG="$result"
        log_info "Resolved latest tag: ${TAG}"
    fi
}

# Get list of assets for the release
get_release_assets() {
    local response
    response=$(github_api "repos/${REPO}/releases/tags/${TAG}")
    echo "$response" | grep '"name"' | sed 's/.*: "\([^"]*\)".*/\1/'
}

# Download a release asset
download_asset() {
    local asset_name="$1"
    local output_path="$2"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY-RUN] Would download: ${asset_name}"
        return 0
    fi

    log_info "Downloading: ${asset_name}"

    local download_url="https://github.com/${REPO}/releases/download/${TAG}/${asset_name}"
    local http_code
    http_code=$(curl -sL -w '%{http_code}' -o "$output_path" "$download_url")

    if [[ "$http_code" != "200" ]]; then
        log_error "Failed to download ${asset_name}: HTTP ${http_code}"
        rm -f "$output_path"
        return 1
    fi

    # Verify file is non-empty (catches truncated downloads)
    if [[ ! -s "$output_path" ]]; then
        log_error "Downloaded file is empty: ${asset_name}"
        rm -f "$output_path"
        return 1
    fi

    log_success "Downloaded: ${output_path}"
}

# Find LessUI.zip to inject
find_lessui_zip() {
    if [[ -n "$LESSUI_ZIP" ]]; then
        if [[ ! -f "$LESSUI_ZIP" ]]; then
            log_error "Specified LessUI.zip not found: ${LESSUI_ZIP}"
            exit 1
        fi
        return 0
    fi

    # Look for latest release in releases/
    local releases_dir="${PROJECT_ROOT}/releases"
    if [[ -d "$releases_dir" ]]; then
        # Find most recent LessUI zip
        LESSUI_ZIP=$(find "$releases_dir" -name "LessUI-*.zip" -type f | sort -r | head -1)
    fi

    if [[ -z "$LESSUI_ZIP" || ! -f "$LESSUI_ZIP" ]]; then
        log_error "No LessUI.zip found. Build with 'make package' first or specify with --zip"
        exit 1
    fi

    log_info "Using LessUI zip: ${LESSUI_ZIP}"
}

# Get partition 2 offset and size from disk image
# Returns: start_sector size_sectors
get_partition2_info() {
    local img_path="$1"

    # Use sfdisk to get partition info in sectors
    local part_info
    part_info=$(sfdisk -d "$img_path" 2>/dev/null | grep "^${img_path}2" | head -1)

    if [[ -z "$part_info" ]]; then
        log_error "Could not find partition 2 in image: ${img_path}"
        return 1
    fi

    # Extract using regex matching (more robust than sed)
    local start size
    if [[ "$part_info" =~ start=\ *([0-9]+) ]]; then
        start="${BASH_REMATCH[1]}"
    else
        log_error "Could not parse start sector from: ${part_info}"
        return 1
    fi

    if [[ "$part_info" =~ size=\ *([0-9]+) ]]; then
        size="${BASH_REMATCH[1]}"
    else
        log_error "Could not parse partition size from: ${part_info}"
        return 1
    fi

    echo "$start $size"
}

# Inject LessUI.zip into partition 2 of the image
inject_lessui() {
    local img_path="$1"
    local zip_path="$2"

    log_info "Injecting LessUI.zip into: ${img_path}"

    # Get partition 2 info
    local part_info
    part_info=$(get_partition2_info "$img_path")
    local start_sector size_sectors
    read -r start_sector size_sectors <<< "$part_info"

    local start_bytes=$((start_sector * 512))

    log_info "Partition 2: start=${start_sector} sectors (${start_bytes} bytes), size=${size_sectors} sectors"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY-RUN] Would inject ${zip_path} into partition 2"
        return 0
    fi

    # Extract partition 2 to a temp file
    local tmp_dir
    tmp_dir=$(mktemp -d)
    local part2_img="${tmp_dir}/part2.ext4"

    log_info "Extracting partition 2..."
    if ! dd if="$img_path" of="$part2_img" bs=512 skip="$start_sector" count="$size_sectors" status=none; then
        log_error "Failed to extract partition 2"
        rm -rf "$tmp_dir"
        return 1
    fi

    # Use debugfs to inject the file
    # debugfs -w -R "write <local_file> <path_in_fs>" <fs_image>
    log_info "Injecting LessUI.zip using debugfs..."
    if ! debugfs -w -R "write ${zip_path} /LessUI.zip" "$part2_img"; then
        log_error "debugfs injection failed"
        rm -rf "$tmp_dir"
        return 1
    fi

    # Verify the file was written
    log_info "Verifying injection..."
    if ! debugfs -R "stat /LessUI.zip" "$part2_img" &>/dev/null; then
        log_error "Failed to verify LessUI.zip in partition"
        rm -rf "$tmp_dir"
        return 1
    fi

    # Write partition back to image
    log_info "Writing partition back to image..."
    if ! dd if="$part2_img" of="$img_path" bs=512 seek="$start_sector" conv=notrunc status=none; then
        log_error "Failed to write partition back to image"
        rm -rf "$tmp_dir"
        return 1
    fi

    # Cleanup
    rm -rf "$tmp_dir"

    log_success "Successfully injected LessUI.zip"
}

# Process a single image file
process_image() {
    local asset_name="$1"
    local download_dir="$2"

    local gz_path="${download_dir}/${asset_name}"
    local img_path="${gz_path%.gz}"
    local output_gz="${OUTPUT_DIR}/${asset_name}"

    # Download
    download_asset "$asset_name" "$gz_path"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY-RUN] Would decompress, inject, and recompress"
        return 0
    fi

    # Decompress
    log_info "Decompressing..."
    gunzip -k "$gz_path"

    # Inject
    inject_lessui "$img_path" "$LESSUI_ZIP"

    # Recompress
    log_info "Recompressing..."
    if command -v pigz &>/dev/null; then
        pigz --best --force "$img_path"
    else
        gzip --best --force "$img_path"
    fi

    # Move to output directory (already validated in main)
    mv "${img_path}.gz" "$output_gz"
    rm -f "$gz_path"

    # Create sha256
    (cd "$OUTPUT_DIR" && sha256sum "$(basename "$output_gz")" > "$(basename "$output_gz").sha256")

    log_success "Output: ${output_gz}"
}

# Main
main() {
    log_info "LessOS Image Fetch and Inject"
    log_info "=============================="

    check_tools

    # Validate output directory early (before expensive downloads)
    if [[ "$DRY_RUN" != true ]]; then
        if ! mkdir -p "$OUTPUT_DIR"; then
            log_error "Cannot create output directory: ${OUTPUT_DIR}"
            exit 1
        fi
    fi

    get_release_tag
    find_lessui_zip

    # Get available assets
    local assets
    assets=$(get_release_assets | grep '\.img\.gz$' | grep -v '\.sha256$' || true)

    if [[ -z "$assets" ]]; then
        log_error "No image assets found for tag: ${TAG}"
        exit 1
    fi

    # Filter by device if specified
    if [[ -n "$DEVICE" && "$DEVICE" != "all" ]]; then
        assets=$(echo "$assets" | grep -i "$DEVICE" || true)
    fi

    # Filter by variant if specified
    if [[ -n "$VARIANT" && "$VARIANT" != "all" ]]; then
        assets=$(echo "$assets" | grep -i "$VARIANT" || true)
    fi

    if [[ -z "$assets" ]]; then
        log_error "No matching assets found for device=${DEVICE:-any} variant=${VARIANT:-any}"
        exit 1
    fi

    log_info "Assets to process:"
    while IFS= read -r asset; do
        echo "  - $asset"
    done <<< "$assets"

    # Create temp download directory (global for trap access)
    DOWNLOAD_DIR=$(mktemp -d)
    trap 'rm -rf "$DOWNLOAD_DIR"' EXIT

    # Process each asset (using here-string to avoid subshell)
    while IFS= read -r asset; do
        [[ -z "$asset" ]] && continue
        log_info ""
        log_info "Processing: ${asset}"
        log_info "----------------------------------------"
        process_image "$asset" "$DOWNLOAD_DIR"
    done <<< "$assets"

    log_info ""
    log_success "All images processed!"
    log_info "Output directory: ${OUTPUT_DIR}"
}

main "$@"
