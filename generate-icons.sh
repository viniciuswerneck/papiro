#!/bin/bash
# Script para gerar ícones multi-resolução a partir do SVG placeholder
# Requer: inkscape e imagemagick (convert)
# Ubuntu/Debian: sudo apt-get install inkscape imagemagick

set -e

SVG_SOURCE="resources/icons/papiro.svg"
ICON_DIR="resources/icons"

# Verificar se o SVG existe
if [ ! -f "$SVG_SOURCE" ]; then
    echo "Erro: $SVG_SOURCE não encontrado"
    exit 1
fi

# Verificar ferramentas
if ! command -v inkscape &> /dev/null; then
    echo "Erro: inkscape não instalado. Instale com: sudo apt-get install inkscape"
    exit 1
fi

if ! command -v convert &> /dev/null; then
    echo "Erro: imagemagick (convert) não instalado. Instale com: sudo apt-get install imagemagick"
    exit 1
fi

echo "Gerando PNGs multi-resolução..."
for size in 16 24 32 48 64 128 256; do
    inkscape "$SVG_SOURCE" --export-type=png -w $size -h $size -o "$ICON_DIR/papiro-${size}.png"
    echo "  ✓ ${size}x${size}"
done

echo "Gerando ICO para Windows..."
convert "$ICON_DIR/papiro-{16,24,32,48,64,128,256}.png" "$ICON_DIR/papiro.ico"
echo "  ✓ papiro.ico"

echo "Gerando PNG principal 256x256 (para .desktop / AppImage)..."
inkscape "$SVG_SOURCE" --export-type=png -w 256 -h 256 -o "$ICON_DIR/papiro.png"
echo "  ✓ papiro.png"

echo ""
echo "Ícones gerados com sucesso em $ICON_DIR/"
ls -la "$ICON_DIR"/papiro*