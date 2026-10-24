#!/usr/bin/env bash
# Generates PNGs (16,24,32,48,64,128,256,512) + Windows .ico from papiro.svg
# Requires: inkscape, imagemagick (convert)
set -euo pipefail

SVG="papiro.svg"
OUT_DIR="."

# PNGs
for size in 16 24 32 48 64 128 256 512; do
  inkscape "$SVG" --export-type=png -w $size -h $size -o "${OUT_DIR}/papiro-${size}.png"
done

# Main PNG (256) as default
cp "${OUT_DIR}/papiro-256.png" "${OUT_DIR}/papiro.png"

# Windows .ico (multi-resolution)
convert "${OUT_DIR}/papiro-{16,24,32,48,64,128,256}.png" "${OUT_DIR}/papiro.ico"

echo "✅ Generated: papiro.png, papiro-*.png, papiro.ico"