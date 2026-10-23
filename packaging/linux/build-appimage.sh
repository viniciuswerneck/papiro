#!/bin/bash
set -euo pipefail

APPNAME="Papiro"
VERSION="0.1.0"
BUILD_DIR="build"
APPDIR="${BUILD_DIR}/AppDir"

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
linuxdeploy --appdir "${APPDIR}" --plugin qt --output appimage

echo "AppImage gerado em: ${BUILD_DIR}/${APPNAME}-${VERSION}-x86_64.AppImage"