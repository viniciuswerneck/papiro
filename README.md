# Papiro

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/viniciuswerneck/papiro)](https://github.com/viniciuswerneck/papiro/releases)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/linux-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/windows-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions)

> A focused, cross-platform text editor built with **C++17 / Qt 6.8**.

## 📥 Installation

### Linux

#### AppImage (recommended, universal)
```bash
wget https://github.com/viniciuswerneck/papiro/releases/latest/download/Papiro-0.1.0-x86_64.AppImage
chmod +x Papiro-*.AppImage
./Papiro-*.AppImage
```

#### Debian/Ubuntu (.deb)
```bash
wget https://github.com/viniciuswerneck/papiro/releases/latest/download/papiro_0.1.0_amd64.deb
sudo apt install ./papiro_*.deb
```

#### Fedora/RHEL (.rpm)
```bash
wget https://github.com/viniciuswerneck/papiro/releases/latest/download/papiro-0.1.0-1.x86_64.rpm
sudo dnf install ./papiro-*.rpm
```

#### Arch Linux (AUR)
```bash
yay -S papiro  # or paru -S papiro
```

### Windows

#### Installer (QtIFW)
Download `Papiro-0.1.0-windows-x64.exe` from [Releases](https://github.com/viniciuswerneck/papiro/releases), run it.

#### Portable ZIP
Download `Papiro-0.1.0-windows-x64-portable.zip`, extract anywhere, run `Papiro.exe`.

### Build from Source
```bash
# Linux
export PATH="$HOME/.local/bin:$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64:$HOME/.local/gl"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOpenGL_GL_PREFERENCE=LEGACY -DOPENGL_INCLUDE_DIR=$HOME/.local/gl/usr/include -DOPENGL_gl_LIBRARY=$HOME/.local/gl/usr/lib/x86_64-linux-gnu/libGL.so .
cmake --build build --config Release -j$(nproc)
./build/Papiro
```

### Features at a Glance
- Syntax highlighting: C++, Python, JS, HTML, CSS, Java, JSON, Markdown, Text
- Multi-line strings/comments, auto-indent, auto-close brackets
- Macros (record/replay, 9 slots, custom shortcuts)
- Clip library (snippets with categories/tags)
- Spell checking (Hunspell: pt_BR, en_US, es_ES)
- Dark/Light themes, i18n (PT/EN/ES)
- File explorer, workspaces, external tools
- Find in files, block selection, folding
- Printing, session restore