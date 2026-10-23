#!/usr/bin/env python3
# Script para gerar ícones multi-resolução a partir do SVG usando cairosvg + Pillow
# Não requer inkscape/imagemagick instalados no sistema

import os
import sys
from pathlib import Path

# Adicionar o diretório do usuário ao path para cairosvg
sys.path.insert(0, os.path.expanduser('~/.local/lib/python3.14/site-packages'))

try:
    import cairosvg
    from PIL import Image
except ImportError as e:
    print(f"Erro ao importar dependências: {e}")
    print("Instale com: pip3 install --user --break-system-packages cairosvg")
    sys.exit(1)

SVG_SOURCE = Path("resources/icons/papiro.svg")
ICON_DIR = Path("resources/icons")

# Verificar se o SVG existe
if not SVG_SOURCE.exists():
    print(f"Erro: {SVG_SOURCE} não encontrado")
    sys.exit(1)

print("Gerando PNGs multi-resolução...")
sizes = [16, 24, 32, 48, 64, 128, 256]
png_files = []

for size in sizes:
    output_path = ICON_DIR / f"papiro-{size}.png"
    cairosvg.svg2png(
        url=str(SVG_SOURCE),
        write_to=str(output_path),
        output_width=size,
        output_height=size
    )
    png_files.append(output_path)
    print(f"  ✓ {size}x{size}")

print("Gerando ICO para Windows...")
# Carregar todas as imagens PNG
images = [Image.open(p) for p in png_files]
# Salvar como ICO
ico_path = ICON_DIR / "papiro.ico"
images[0].save(
    ico_path,
    format='ICO',
    sizes=[(img.width, img.height) for img in images],
    append_images=images[1:]
)
print(f"  ✓ papiro.ico")

print("Gerando PNG principal 256x256 (para .desktop / AppImage)...")
main_png = ICON_DIR / "papiro.png"
cairosvg.svg2png(
    url=str(SVG_SOURCE),
    write_to=str(main_png),
    output_width=256,
    output_height=256
)
print(f"  ✓ papiro.png")

print("")
print("Ícones gerados com sucesso em", ICON_DIR)
for f in sorted(ICON_DIR.glob("papiro*")):
    print(f"  {f.name} ({f.stat().st_size} bytes)")