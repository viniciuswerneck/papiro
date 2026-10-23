# Plano de Empacotamento do Papiro

## Visão Geral

**Projeto**: Papiro — Editor de texto cross-platform (Qt 6.8.3)  
**Build System**: CMake + Ninja  
**Target**: `Papiro` (binário ELF Linux / .exe Windows)  
**Qt**: `~/Qt/6.8.3/gcc_64` (Linux), instalador Qt Online (Windows)  
**Dependências**: Qt6::Core, Widgets, PrintSupport, LinguistTools, **opcional** hunspell  
**Recursos embutidos**: syntax definitions (:/syntax), traduções .qm (:/i18n)  
**Configuração usuário**: `~/.config/Papiro/`

---

## 1. Análise de Dependências Runtime

### Linux (ldd no binário atual)

```bash
# Qt libraries (precisam ser embarcadas no AppImage)
libQt6PrintSupport.so.6
libQt6Widgets.so.6
libQt6Gui.so.6
libQt6Core.so.6
libQt6DBus.so.6
libicui18n.so.73
libicuuc.so.73
libicudata.so.73

# Sistema (NÃO embarcar — usar do host)
libstdc++.so.6, libgcc_s.so.1, libc.so.6
libGL.so.1, libEGL.so.1, libxkbcommon.so.0
libfontconfig.so.1, libfreetype.so.6
libX11.so.6, libxcb.so.1, libXau.so.6, libXdmcp.so.6
libglib-2.0.so.0, libgthread-2.0.so.0
libpcre2-8.so.0, libdbus-1.so.3
libz.so.1, libzstd.so.1, libbz2.so.1.0
libpng16.so.16, libexpat.so.1
libdl.so.2, libpthread.so.0, libm.so.6, librt.so.1
libatomic.so.1

# Opcional (se hunspell encontrado no build)
libhunspell-1.7.so.0  → EMBARCAR se usado
```

**Qt plugins necessários** (detectados via `QCoreApplication::libraryPaths()`):
- `platforms/libqxcb.so` (Linux X11)
- `platformthemes/libqgtk3.so` (opcional, tema GTK)
- `styles/libqbreeze.so`, `libqfusion.so` (estilos)
- `iconengines/libqsvgicon.so` (ícones SVG)
- `imageformats/libqjpeg.so`, `libqpng.so`, `libqsvg.so`
- `tls/libqopenssl.so` (se usar SSL)

---

## 2. Ferramentas Recomendadas

| Plataforma | Ferramenta Principal | Alternativa | Instalação |
|------------|---------------------|-------------|------------|
| **Linux (AppImage)** | `linuxdeploy` + `linuxdeploy-plugin-qt` | `linuxdeployqt` (legacy), `appimagetool` standalone | GitHub Releases / AppImage |
| **Linux (.deb/.rpm)** | `CPack` (DEB, RPM generators) | `fpm`, `alien` | `cmake --install` + CPack |
| **Windows (Installer)** | `Qt Installer Framework (QtIFW)` | `NSIS`, `Inno Setup`, `WiX` | Qt Online Installer / GitHub |
| **Windows (Portable)** | `windeployqt` + zip | `cpack -G ZIP` | Qt bin directory |
| **Windows (MSIX/Store)** | `wix` / `makeappx` | QtIFW MSIX | Windows SDK |

### Versões recomendadas (2024/2025)
- `linuxdeploy` / `linuxdeploy-plugin-qt`: **latest continuous** (AppImage)
- `appimagetool`: **v13+** (AppImage runtime)
- `QtIFW`: **4.6+** (Qt 6 support)
- `NSIS`: **3.09+**
- `CMake`: **3.21+** (já usado)

---

## 3. Estrutura de Diretórios dos Pacotes

### 3.1 AppImage (Linux)

```
Papiro-0.1.0-x86_64.AppImage  (arquivo único, executável)
│
├── AppRun                    → entry point (script ou binário)
├── papiro.desktop            → integração desktop
├── papiro.png                → ícone 256x256
├── usr/
│   ├── bin/
│   │   └── papiro            → binário principal (renomeado para lowercase)
│   ├── lib/
│   │   ├── libQt6*.so.6      → Qt libs
│   │   ├── libicu*.so.73     → ICU
│   │   └── libhunspell*.so   → se habilitado
│   ├── share/
│   │   ├── applications/
│   │   │   └── papiro.desktop
│   │   ├── icons/hicolor/256x256/apps/
│   │   │   └── papiro.png
│   │   ├── metainfo/
│   │   │   └── papiro.metainfo.xml  → AppStream metadata
│   │   └── papiro/
│   │       ├── syntax/       → .syn files (ou usar :/syntax do Qt resource)
│   │       └── translations/ → .qm files (ou usar :/i18n do Qt resource)
│   └── plugins/              → Qt plugins (platforms, styles, iconengines, imageformats)
└── .DirIcon                  → symlink para papiro.png
```

### 3.2 Instalador Windows (QtIFW / NSIS)

```
Papiro-0.1.0-windows-x64.exe  (instalador)
│
Instala em: %LOCALAPPDATA%\Programs\Papiro\  (user) ou C:\Program Files\Papiro\ (admin)
│
├── Papiro.exe                → binário principal
├── Qt6Core.dll, Qt6Widgets.dll, Qt6Gui.dll, Qt6PrintSupport.dll, Qt6DBus.dll
├── icuin73.dll, icuuc73.dll, icudt73.dll
├── libhunspell-1.7.dll       → se habilitado
├── platforms/
│   └── qwindows.dll
├── styles/
│   ├── qwindowsvistastyle.dll
│   └── qfusion.dll
├── iconengines/
│   └── qsvgicon.dll
├── imageformats/
│   ├── qjpeg.dll, qpng.dll, qsvg.dll
├── tls/
│   └── qopenssl.dll          → se OpenSSL
├── translations/
│   ├── papiro_pt.qm
│   ├── papiro_en.qm
│   └── papiro_es.qm
├── syntax/                   → opcional se não usar :/syntax
│   ├── cpp.syn, python.syn, ...
├── uninstall.exe             → desinstalador (QtIFW/NSIS)
├── papiro.ico                → ícone Windows (multi-resolução)
└── LICENSE, README.md
```

### 3.3 Portable ZIP (Windows)

```
Papiro-0.1.0-windows-x64-portable.zip
│
├── Papiro.exe
├── [mesmas DLLs e plugins do instalador]
├── translations/
├── syntax/
├── papiro.ico
└── README.txt
```

---

## 4. Arquivos Necessários para Criação

### 4.1 Ícone do Aplicação

**Criar**: `resources/icons/papiro.svg` (vetorial, source)  
**Gerar**: PNGs 16, 24, 32, 48, 64, 128, 256, 512 + `.ico` Windows

```bash
# Script de geração (Inkscape necessário)
inkscape resources/icons/papiro.svg --export-type=png -w 256 -h 256 -o resources/icons/papiro.png
inkscape resources/icons/papiro.svg --export-type=png -w 512 -h 512 -o resources/icons/papiro-512.png
# .ico multi-res (ImageMagick)
convert resources/icons/papiro-{16,24,32,48,64,128,256}.png resources/icons/papiro.ico
```

### 4.2 Arquivo `.desktop` (Linux)

**Arquivo**: `resources/papiro.desktop`

```ini
[Desktop Entry]
Type=Application
Name=Papiro
GenericName=Text Editor
Comment=A focused, cross-platform text editor
Exec=papiro %F
Icon=papiro
Terminal=false
Categories=Utility;TextEditor;Development;
MimeType=text/plain;text/x-csrc;text/x-c++src;text/x-python;text/x-java;text/html;text/css;text/javascript;application/json;text/x-markdown;
StartupNotify=true
Keywords=editor;text;code;programming;
```

### 4.3 AppStream Metainfo (Linux)

**Arquivo**: `resources/papiro.metainfo.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>com.github.vinicius.papiro</id>
  <name>Papiro</name>
  <summary>A focused, cross-platform text editor</summary>
  <description>
    <p>Papiro is a lightweight, distraction-free text editor designed for developers and writers.</p>
    <ul>
      <li>Syntax highlighting for 10+ languages</li>
      <li>Multiple cursors and selections</li>
      <li>Macro recording and playback</li>
      <li>Clip library for snippets</li>
      <li>Spell checking (Hunspell)</li>
      <li>Dark/light themes</li>
      <li>Portuguese, English, Spanish translations</li>
    </ul>
  </description>
  <project_license>MIT</project_license>
  <url type="homepage">https://github.com/vinicius/papiro</url>
  <url type="bugtracker">https://github.com/vinicius/papiro/issues</url>
  <screenshots>
    <screenshot type="default">https://raw.githubusercontent.com/vinicius/papiro/main/screenshots/main.png</screenshot>
  </screenshots>
  <releases>
    <release version="0.1.0" date="2026-10-23">
      <description>Initial public release</description>
    </release>
  </releases>
  <provides>
    <binary>papiro</binary>
  </provides>
</component>
```

### 4.4 Configuração NSIS (Windows) — se não usar QtIFW

**Arquivo**: `packaging/windows/papiro.nsi`

```nsi
!include "MUI2.nsh"
!include "x64.nsh"

Name "Papiro"
OutFile "Papiro-${VERSION}-windows-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\Papiro"
InstallDirRegKey HKCU "Software\Papiro" "InstallDir"
RequestExecutionLevel user

!define MUI_ICON "resources/icons/papiro.ico"
!define MUI_UNICON "resources/icons/papiro.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Portuguese"

Section "Main" SEC_MAIN
  SetOutPath "$INSTDIR"
  File /r "build\windows\*"
  
  CreateDirectory "$SMPROGRAMS\Papiro"
  CreateShortCut "$SMPROGRAMS\Papiro\Papiro.lnk" "$INSTDIR\Papiro.exe" "" "$INSTDIR\papiro.ico"
  CreateShortCut "$DESKTOP\Papiro.lnk" "$INSTDIR\Papiro.exe" "" "$INSTDIR\papiro.ico"
  
  WriteRegStr HKCU "Software\Papiro" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\*.*"
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\Papiro\*.*"
  RMDir "$SMPROGRAMS\Papiro"
  Delete "$DESKTOP\Papiro.lnk"
  DeleteRegKey HKCU "Software\Papiro"
SectionEnd
```

---

## 5. Scripts CMake/CPack (Adicionar ao CMakeLists.txt)

### 5.1 Adições ao CMakeLists.txt principal

```cmake
# ============================================================================
# PACKAGING: CPack configuration for DEB, RPM, AppImage (via external), NSIS
# ============================================================================

# ---------------------------------------------------------------------------
# Basic package metadata
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "Papiro")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A focused, cross-platform text editor")
set(CPACK_PACKAGE_VENDOR "Vinicius")
set(CPACK_PACKAGE_CONTACT "vinicius@example.com")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Papiro")

# ---------------------------------------------------------------------------
# Icon & Desktop file
# ---------------------------------------------------------------------------
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/debian-control-extra.cmake")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")

# Desktop file for Linux packages
set(CPACK_DESKTOP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/papiro.desktop")

# ---------------------------------------------------------------------------
# Linux: DEB generator
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR "DEB;RPM")
  
  # DEB-specific
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6, libqt6gui6, libqt6printsupport6, libqt6dbus6, libhunspell-1.7-0, libglib2.0-0, libfontconfig1, libfreetype6, libx11-6, libxcb1, libegl1, libgl1, libxkbcommon0")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  set(CPACK_DEBIAN_ARCHITECTURE "amd64")
  
  # RPM-specific
  set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.5, hunspell >= 1.7, glib2 >= 2.0, fontconfig, freetype, libX11, libxcb, libEGL, libGL, libxkbcommon")
  set(CPACK_RPM_COMPRESSION_TYPE "zstd")
endif()

# ---------------------------------------------------------------------------
# Windows: NSIS generator
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CPACK_GENERATOR "NSIS;ZIP")
  
  set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\Programs")
  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_DISPLAY_NAME "Papiro ${PROJECT_VERSION}")
  set(CPACK_NSIS_HELP_LINK "https://github.com/vinicius/papiro")
  set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/vinicius/papiro")
  set(CPACK_NSIS_CONTACT "vinicius@example.com")
  set(CPACK_NSIS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/papiro.ico")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "papiro.exe")
  set(CPACK_NSIS_MENU_LINKS "https://github.com/vinicius/papiro;Papiro Website")
  set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "MessageBox MB_OK 'Papiro ${PROJECT_VERSION} Setup'")
  set(CPACK_NSIS_CREATE_ICONS_EXTRA "
    CreateShortCut '\$SMPROGRAMS\\Papiro\\Uninstall.lnk' '\$INSTDIR\\uninstall.exe' '' '\$INSTDIR\\uninstall.exe' 0
  ")
  set(CPACK_NSIS_DELETE_ICONS_EXTRA "
    Delete '\$SMPROGRAMS\\Papiro\\Uninstall.lnk'
    RMDir '\$SMPROGRAMS\\Papiro'
  ")
  
  # ZIP portable
  set(CPACK_ZIP_PACKAGE_NAME "Papiro-${PROJECT_VERSION}-windows-x64-portable")
endif()

# ---------------------------------------------------------------------------
# Install rules for packaging
# ---------------------------------------------------------------------------
# Binário
install(TARGETS Papiro
  RUNTIME DESTINATION bin
  BUNDLE DESTINATION .
)

# Traduções .qm (também embutidas via Qt resource, mas instalar para fallback)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/papiro_pt.qm
  ${CMAKE_CURRENT_BINARY_DIR}/papiro_en.qm
  ${CMAKE_CURRENT_BINARY_DIR}/papiro_es.qm
  DESTINATION share/papiro/translations
)

# Syntax definitions (também embutidas via Qt resource)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/syntax/
  DESTINATION share/papiro/syntax
  FILES_MATCHING PATTERN "*.syn"
)

# Ícone
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/papiro.png
  DESTINATION share/icons/hicolor/256x256/apps
  RENAME papiro.png
)

# Desktop file
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/papiro.desktop
  DESTINATION share/applications
)

# AppStream metainfo
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/papiro.metainfo.xml
  DESTINATION share/metainfo
)

# ---------------------------------------------------------------------------
# Include CPack
# ---------------------------------------------------------------------------
include(CPack)
```

### 5.2 Arquivo extra para DEB: `packaging/linux/debian-control-extra.cmake`

```cmake
# Extra control fields for DEB package
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/debian-control-extra.txt")
```

**Conteúdo de `debian-control-extra.txt`**:
```
Maintainer: Vinicius <vinicius@example.com>
Homepage: https://github.com/vinicius/papiro
Vcs-Git: https://github.com/vinicius/papiro.git
Vcs-Browser: https://github.com/vinicius/papiro
```

---

## 6. Scripts de Build dos Pacotes

### 6.1 Linux: AppImage com `linuxdeploy` + plugin Qt

**Script**: `packaging/linux/build-appimage.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

VERSION="0.1.0"
ARCH="x86_64"
BUILD_DIR="build"
APPDIR="Papiro.AppDir"

# 1. Instalar binários via CMake install
cmake --install ${BUILD_DIR} --prefix ${APPDIR}/usr

# 2. Baixar linuxdeploy e plugin Qt se não existirem
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
QT_PLUGIN_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage"

for url in "$LINUXDEPLOY_URL" "$QT_PLUGIN_URL"; do
  file=$(basename "$url")
  if [[ ! -f "$file" ]]; then
    wget -q "$url" -O "$file"
    chmod +x "$file"
  fi
done

# 3. Executar linuxdeploy com plugin Qt
./linuxdeploy-${ARCH}.AppImage \
  --appdir "${APPDIR}" \
  --plugin qt \
  --output appimage \
  --desktop-file resources/papiro.desktop \
  --icon-file resources/icons/papiro.png \
  --executable ${APPDIR}/usr/bin/Papiro \
  --appimage-extract-and-run

# 4. Renomear output
mv Papiro-*-${ARCH}.AppImage "Papiro-${VERSION}-${ARCH}.AppImage"

echo "✅ AppImage created: Papiro-${VERSION}-${ARCH}.AppImage"
```

### 6.2 Windows: QtIFW Installer

**Estrutura**: `packaging/windows/qtifw/`

```
packaging/windows/qtifw/
├── config/
│   └── config.xml
├── packages/
│   └── com.vendor.papiro/
│       ├── meta/
│       │   ├── package.xml
│       │   └── installscript.qs
│       └── data/          (preenchido pelo script de build)
├── build-installer.ps1
└── build-installer.sh
```

**`config/config.xml`**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<Installer>
  <Name>Papiro</Name>
  <Version>0.1.0</Version>
  <Title>Papiro Text Editor</Title>
  <Publisher>Vinicius</Publisher>
  <StartMenuDir>Papiro</StartMenuDir>
  <TargetDir>@ApplicationsDir@/Papiro</TargetDir>
  <RemoteRepositories/>
  <AllowSpaceInPath>true</AllowSpaceInPath>
  <RunProgram>Papiro.exe</RunProgram>
  <RunProgramDescription>Launch Papiro</RunProgramDescription>
</Installer>
```

**`packages/com.vendor.papiro/meta/package.xml`**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<Package>
  <DisplayName>Papiro</DisplayName>
  <Description>Text editor component</Description>
  <Version>0.1.0</Version>
  <ReleaseDate>2026-10-23</ReleaseDate>
  <Default>true</Default>
  <Script>installscript.qs</Script>
</Package>
```

**`packages/com.vendor.papiro/meta/installscript.qs`**:
```javascript
function Component() {
  component.createOperations();
  component.addOperation("CreateShortcut", 
    "@TargetDir@/Papiro.exe", 
    "@StartMenuDir@/Papiro.lnk",
    "", 
    "@TargetDir@/Papiro.exe", 
    0);
  component.addOperation("CreateShortcut", 
    "@TargetDir@/Papiro.exe", 
    "@DesktopDir@/Papiro.lnk",
    "", 
    "@TargetDir@/Papiro.exe", 
    0);
}
```

**Script de build Windows (PowerShell)**: `packaging/windows/build-installer.ps1`

```powershell
# Build Windows installer with QtIFW
$VERSION = "0.1.0"
$QT_DIR = "C:\Qt\6.8.3\mingw_64"  # ou msvc2022_64
$IFW_DIR = "C:\Qt\QtIFW-4.6"

# 1. Build release
cmake --build build --config Release --target Papiro

# 2. Deploy Qt dependencies
& "$QT_DIR\bin\windeployqt.exe" --release --no-translations --no-openssl --no-angle --no-compiler-runtime "build\Release\Papiro.exe"

# 3. Copiar traduções e syntax para pasta de deploy
Copy-Item "build\papiro_*.qm" "build\Release\translations\"
Copy-Item "resources\syntax\*.syn" "build\Release\syntax\"
Copy-Item "resources\icons\papiro.ico" "build\Release\"

# 4. Criar estrutura QtIFW
$PKG_DIR = "packaging\windows\qtifw\packages\com.vendor.papiro\data"
Remove-Item -Recurse -Force $PKG_DIR -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $PKG_DIR | Out-Null
Copy-Item "build\Release\*" $PKG_DIR -Recurse

# 5. Gerar instalador
& "$IFW_DIR\bin\binarycreator.exe" -c "packaging\windows\qtifw\config\config.xml" -p "packaging\windows\qtifw\packages" "Papiro-${VERSION}-windows-x64.exe"

Write-Host "✅ Installer created: Papiro-${VERSION}-windows-x64.exe"
```

### 6.3 Windows: Portable ZIP (simples)

```powershell
# Após windeployqt
Compress-Archive -Path "build\Release\*" -DestinationPath "Papiro-${VERSION}-windows-x64-portable.zip"
```

---

## 7. CI/CD GitHub Actions

### 7.1 Workflow Linux (AppImage + DEB + RPM)

**Arquivo**: `.github/workflows/linux-release.yml`

```yaml
name: Linux Release

on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:

permissions:
  contents: write

jobs:
  build:
    name: Build Linux (Ubuntu 22.04)
    runs-on: ubuntu-22.04
    container:
      image: ubuntu:22.04
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          apt-get update
          apt-get install -y --no-install-recommends \
            cmake ninja-build g++ \
            qt6-base-dev qt6-tools-dev qt6-translations-l10n \
            libhunspell-dev \
            libgl1-mesa-dev libxkbcommon-dev \
            wget ca-certificates
      
      - name: Configure
        run: |
          cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Run tests
        run: |
          cd build && ctest --output-on-failure
      
      - name: Install to AppDir
        run: |
          cmake --install build --prefix AppDir/usr
      
      - name: Download linuxdeploy
        run: |
          wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
          wget -q "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
          chmod +x linuxdeploy-*.AppImage
      
      - name: Build AppImage
        run: |
          ./linuxdeploy-x86_64.AppImage \
            --appdir AppDir \
            --plugin qt \
            --output appimage \
            --desktop-file resources/papiro.desktop \
            --icon-file resources/icons/papiro.png \
            --executable AppDir/usr/bin/Papiro
      
      - name: Build DEB/RPM via CPack
        run: |
          cd build
          cpack -G DEB -G RPM
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: linux-packages
          path: |
            Papiro-*.AppImage
            build/*.deb
            build/*.rpm
      
      - name: Create Release
        if: startsWith(github.ref, 'refs/tags/')
        uses: softprops/action-gh-release@v2
        with:
          files: |
            Papiro-*.AppImage
            build/*.deb
            build/*.rpm
          generate_release_notes: true
```

### 7.2 Workflow Windows (QtIFW + Portable ZIP)

**Arquivo**: `.github/workflows/windows-release.yml`

```yaml
name: Windows Release

on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:

permissions:
  contents: write

jobs:
  build:
    name: Build Windows (MSVC 2022)
    runs-on: windows-2022
    defaults:
      run:
        shell: pwsh
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      
      - name: Install Qt (via aqtinstall)
        uses: jurplel/install-qt-action@v3
        with:
          version: "6.8.3"
          target: "desktop"
          arch: "win64_msvc2022_64"
          modules: "qtcharts qtnetwork qtserialport"
          cache: true
      
      - name: Install Qt Installer Framework
        run: |
          $IFW_URL = "https://download.qt.io/official_releases/qt-installer-framework/4.6.0/qt-installer-framework-windows-x86_64-4.6.0.exe"
          Invoke-WebRequest -Uri $IFW_URL -OutFile "qtifw-installer.exe"
          .\qtifw-installer.exe --script .github/scripts/qtifw-install.qs
      
      - name: Configure
        run: |
          $qt = "C:\Qt\6.8.3\msvc2022_64"
          cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$qt
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Run tests
        run: |
          cd build
          ctest --output-on-failure -C Release
      
      - name: Deploy Qt dependencies (windeployqt)
        run: |
          $qt = "C:\Qt\6.8.3\msvc2022_64"
          & "$qt\bin\windeployqt.exe" --release --no-translations --no-openssl --no-angle --no-compiler-runtime "build\Release\Papiro.exe"
      
      - name: Copy translations and resources
        run: |
          Copy-Item "build\papiro_*.qm" "build\Release\translations\"
          Copy-Item "resources\syntax\*.syn" "build\Release\syntax\"
          Copy-Item "resources\icons\papiro.ico" "build\Release\"
      
      - name: Build QtIFW Installer
        run: |
          $IFW = "C:\Qt\QtIFW-4.6"
          $PKG_DIR = "packaging\windows\qtifw\packages\com.vendor.papiro\data"
          Remove-Item -Recurse -Force $PKG_DIR -ErrorAction SilentlyContinue
          New-Item -ItemType Directory -Force -Path $PKG_DIR | Out-Null
          Copy-Item "build\Release\*" $PKG_DIR -Recurse
          & "$IFW\bin\binarycreator.exe" -c "packaging\windows\qtifw\config\config.xml" -p "packaging\windows\qtifw\packages" "Papiro-${{ github.ref_name }}-windows-x64.exe"
      
      - name: Build Portable ZIP
        run: |
          Compress-Archive -Path "build\Release\*" -DestinationPath "Papiro-${{ github.ref_name }}-windows-x64-portable.zip"
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: windows-packages
          path: |
            Papiro-*-windows-x64.exe
            Papiro-*-windows-x64-portable.zip
      
      - name: Create Release
        if: startsWith(github.ref, 'refs/tags/')
        uses: softprops/action-gh-release@v2
        with:
          files: |
            Papiro-*-windows-x64.exe
            Papiro-*-windows-x64-portable.zip
          generate_release_notes: true
```

**Arquivo**: `.github/scripts/qtifw-install.qs` (script silencioso para instalar QtIFW)

```javascript
Controller.prototype.WelcomePageCallback = function() {
  gui.clickButton(buttons.NextButton);
}
Controller.prototype.CredentialsPageCallback = function() {
  gui.clickButton(buttons.NextButton);
}
Controller.prototype.ComponentSelectionPageCallback = function() {
  var widget = gui.currentPageWidget();
  widget.deselectAll();
  widget.selectComponent("Qt Installer Framework");
  gui.clickButton(buttons.NextButton);
}
Controller.prototype.TargetDirectoryPageCallback = function() {
  gui.currentPageWidget().TargetDirectoryLineEdit.text = "C:\\Qt\\QtIFW-4.6";
  gui.clickButton(buttons.NextButton);
}
Controller.prototype.LicenseAgreementPageCallback = function() {
  gui.currentPageWidget().AcceptLicenseRadioButton.checked = true;
  gui.clickButton(buttons.NextButton);
}
Controller.prototype.ReadyForInstallationPageCallback = function() {
  gui.clickButton(buttons.NextButton);
}
Controller.prototype.FinishedPageCallback = function() {
  gui.clickButton(buttons.FinishButton);
}
```

---

## 8. Checklist de Validação do Pacote Final

### 8.1 AppImage (Linux)

| Item | Verificação | Comando |
|------|-------------|---------|
| ✅ Executa | `./Papiro-*.AppImage` abre a janela | Manual |
| ✅ Ícone | Ícone aparece no launcher/dock | Manual |
| ✅ .desktop | `xdg-open` abre arquivos de texto | `xdg-open test.txt` |
| ✅ Mime types | Associado a text/* | `xdg-mime query default text/plain` |
| ✅ Traduções | PT/EN/ES funcionam | Menu View → Language |
| ✅ Syntax highlight | C++, Python, etc. coloridos | Abrir arquivo .cpp |
| ✅ Spell check | Hunspell funciona (sublinhado vermelho) | Digitar palavra errada |
| ✅ Config persists | `~/.config/Papiro/` criado | Verificar diretório |
| ✅ AppStream | `appstreamcli validate` passa | `appstreamcli validate usr/share/metainfo/papiro.metainfo.xml` |
| ✅ No external Qt deps | `ldd` só mostra system libs | `ldd Papiro` dentro do AppImage |
| ✅ Tamanho razoável | < 100 MB | `ls -lh Papiro-*.AppImage` |

### 8.2 DEB/RPM (Linux)

| Item | Verificação | Comando |
|------|-------------|---------|
| ✅ Instala | `sudo dpkg -i` / `sudo rpm -i` | Manual |
| ✅ Dependências | `apt install -f` / `dnf install` resolve | Manual |
| ✅ Executa | `papiro` no PATH | `which papiro && papiro` |
| ✅ Desktop entry | Aparece no menu aplicações | Manual |
| ✅ Desinstala | `apt remove papiro` / `rpm -e papiro` | Manual |
| ✅ Lintian | `lintian` sem erros críticos | `lintian papiro_*.deb` |

### 8.3 Windows Installer (QtIFW/NSIS)

| Item | Verificação | Comando |
|------|-------------|---------|
| ✅ Instala (user) | Sem admin rights | Executar .exe |
| ✅ Instala (admin) | Com UAC, Program Files | Botão direito → Run as admin |
| ✅ Start Menu | Atalho em "Papiro" | Menu Iniciar |
| ✅ Desktop | Atalho na área de trabalho | Manual |
| ✅ Executa | `Papiro.exe` abre | Manual |
| ✅ Traduções | PT/EN/ES no menu | View → Language |
| ✅ Syntax highlight | Arquivos coloridos | Abrir .py, .cpp |
| ✅ Spell check | Hunspell DLL carregada | Verificar log |
| ✅ Desinstala | Uninstaller remove tudo | Painel de controle |
| ✅ Assinatura | Code signing (opcional) | `signtool verify` |

### 8.4 Windows Portable ZIP

| Item | Verificação |
|------|-------------|
| ✅ Extrai e roda | Sem instalação |
| ✅ Config em pasta | `Papiro.ini` ao lado do .exe |
| ✅ Sem admin | Funciona de pendrive |
| ✅ Limpo | Remove pasta → sem lixo |

---

## 9. Próximos Passos (Ordem Sugerida)

1. **Criar assets visuais**
   - [ ] `resources/icons/papiro.svg` (vetorial)
   - [ ] Gerar PNGs + `.ico` via script

2. **Adicionar arquivos de packaging**
   - [ ] `resources/papiro.desktop`
   - [ ] `resources/papiro.metainfo.xml`
   - [ ] `packaging/linux/debian-control-extra.txt`

3. **Atualizar CMakeLists.txt**
   - [ ] Adicionar seção CPack (conforme seção 5)
   - [ ] Adicionar `install()` para ícone, desktop, metainfo, traduções, syntax

4. **Testar build local**
   - [ ] `cmake --build build --target package` (DEB/RPM)
   - [ ] Rodar `packaging/linux/build-appimage.sh`
   - [ ] Validar checklist 8.1 e 8.2

5. **Windows (máquina Windows ou CI)**
   - [ ] Configurar `packaging/windows/qtifw/`
   - [ ] Testar `build-installer.ps1`
   - [ ] Validar checklist 8.3 e 8.4

6. **CI/CD**
   - [ ] Adicionar workflows GitHub Actions
   - [ ] Testar push de tag `v0.1.0`
   - [ ] Verificar artifacts no Release

7. **Documentação**
   - [ ] Atualizar README com badges de download
   - [ ] Adicionar instruções de instalação por plataforma

---

## 10. Referências Úteis

- **linuxdeploy + Qt plugin**: https://github.com/linuxdeploy/linuxdeploy-plugin-qt
- **AppImage spec**: https://docs.appimage.org/
- **QtIFW Manual**: https://doc.qt.io/qtinstallerframework/
- **CPack Generators**: https://cmake.org/cmake/help/latest/module/CPack.html
- **windeployqt**: https://doc.qt.io/qt-6/windows-deployment.html
- **AppStream metadata**: https://www.freedesktop.org/software/appstream/docs/chap-Metadata.html
- **Freedesktop .desktop spec**: https://specifications.freedesktop.org/desktop-entry-spec/latest/
- **NSIS Docs**: https://nsis.sourceforge.io/Docs/
