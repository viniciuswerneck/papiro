#!/usr/bin/env bash
set -euo pipefail

APPNAME="Papiro"
VERSION="0.1.0"
BUILD_DIR="build"
APPDIR="${BUILD_DIR}/AppDir"

# Download linuxdeploy and Qt plugin if not present
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
QT_PLUGIN_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

for url in "$LINUXDEPLOY_URL" "$QT_PLUGIN_URL"; do
    file=$(basename "$url")
    if [[ ! -f "$file" ]]; then
        echo "Downloading $file..."
        wget -q "$url" -O "$file"
        chmod +x "$file"
    fi
done

rm -rf "${APPDIR}"
mkdir -p "${APPDIR}"

# AppRun entry point
cp "${BUILD_DIR}/${APPNAME}" "${APPDIR}/AppRun"
chmod +x "${APPDIR}/AppRun"

# Desktop file, icon, metainfo
cp resources/papiro.desktop "${APPDIR}/"
cp resources/papiro.metainfo.xml "${APPDIR}/"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
cp resources/icons/papiro.svg "${APPDIR}/usr/share/icons/hicolor/scalable/apps/"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
cp resources/icons/papiro.png "${APPDIR}/usr/share/icons/hicolor/256x256/apps/" 2>/dev/null || true

# Bundle Qt deps via linuxdeploy + plugin qt
export QMAKE="/home/vinicius/Qt/6.8.3/gcc_64/bin/qmake"
./linuxdeploy-x86_64.AppImage --appdir "${APPDIR}" --plugin qt --output appimage \
    --desktop-file resources/papiro.desktop \
    --icon-file resources/icons/papiro.png \
    --executable "${APPDIR}/AppRun"

# Rename output and move to build directory
mv Papiro-x86_64.AppImage "${BUILD_DIR}/${APPNAME}-${VERSION}-x86_64.AppImage"

echo "AppImage gerado em: ${BUILD_DIR}/${APPNAME}-${VERSION}-x86_64.AppImage"