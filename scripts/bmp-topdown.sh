#!/bin/bash
# Convert BMP from bottom-up to top-down by reversing scanlines and negating height
# Usage: ./bmp-topdown.sh input.bmp output.bmp

set -e

if [ $# -ne 2 ]; then
    echo "Usage: $0 input.bmp output.bmp"
    exit 1
fi

INPUT="$1"
OUTPUT="$2"

# Use Python to reverse scanlines and patch header
python3 - "$INPUT" "$OUTPUT" << 'PYTHON_SCRIPT'
import struct
import sys

input_file = sys.argv[1]
output_file = sys.argv[2]

with open(input_file, 'rb') as f:
    # Read header
    header = bytearray(f.read(54))

    # Parse dimensions
    width = struct.unpack_from('<i', header, 0x12)[0]
    height = struct.unpack_from('<i', header, 0x16)[0]
    bits = struct.unpack_from('<H', header, 0x1C)[0]

    bytes_per_pixel = bits // 8
    row_size = width * bytes_per_pixel
    padding = (4 - (row_size % 4)) % 4
    padded_row_size = row_size + padding

    # Read all scanlines
    f.seek(54)
    rows = []
    for y in range(abs(height)):
        row = f.read(padded_row_size)
        rows.append(row)

# Reverse scanline order and negate height
struct.pack_into('<i', header, 0x16, -abs(height))

# Write output
with open(output_file, 'wb') as f:
    f.write(header)
    # Write scanlines in reverse order
    for row in reversed(rows):
        f.write(row)

print(f"Converted {width}x{height} -> {width}x{-abs(height)} (reversed scanlines)", file=sys.stderr)
PYTHON_SCRIPT
