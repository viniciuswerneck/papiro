# Papiro

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/viniciuswerneck/papiro)](https://github.com/viniciuswerneck/papiro/releases)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/linux-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions/workflows/linux-release.yml)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/windows-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions/workflows/windows-release.yml)

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

## ✨ Features

### 🎨 Editor Core
- **Syntax highlighting** for 9 languages: C++, Python, JavaScript, HTML, CSS, Java, JSON, Markdown, Text
- **Multi-line strings & comments** (Python `"""`/`'''`, JS template literals, C-style `/* */`)
- **Smart indentation** on `{`, `[`, `(`, `:`
- **Auto-close pairs** `()[]{}""''` with selection wrapping
- **Backspace** removes empty pairs at once
- **Bracket matching** highlight
- **Line numbers** gutter with current line highlight
- **Code folding** (brace-based, gutter markers ▼/▶, Ctrl+Shift+. to toggle)
- **Block/rectangular selection** (Alt+drag or Column Selection Mode)
- **Zoom** in/out/reset (Ctrl++ / Ctrl+- / Ctrl+0)
- **Word wrap** toggle
- **Configurable font**

### 📁 File & Project Management
- **Tabbed interface** with drag-reorder, close buttons
- **File Explorer** dock (Ctrl+Alt+E) with folder open, filters, context menu
- **Workspaces** (`.ppw`) — save/restore open files, active tab, explorer root
- **Session restore** — reopens files, cursor positions, dock/toolbar layout
- **Recent files & workspaces** (10 each, persisted)
- **Drag & drop** files to open
- **Encoding/EOL detection & conversion** (UTF-8, UTF-16, Windows-1252; LF/CRLF/CR)

### 🔍 Search & Replace
- **Non-modal Find/Replace** dialog (Ctrl+F / Ctrl+H)
- **Regex** (QRegularExpression), case/whole-word/wrap options
- **Replace All** with backreferences (`\1`, `\n`, `\t`)
- **Find in Files** — recursive folder search, 5000 file/result limits, binary detection
- **Search Results dock** (Ctrl+Alt+F) — grouped by file, line/column/preview, double-click to navigate

### ⌨️ Macros
- **Record/Replay** (Ctrl+F9 / Ctrl+F10 / Ctrl+Shift+F9)
- **9 built-in slots** (Ctrl+Shift+1..9) with persistent assignments
- **Custom shortcuts** — assign any key combination via Macro Manager
- **Real keystroke capture** — preserves case, accents, IME input
- **Coalesced playback** — optimized action sequences
- **Macro Manager** — rename, delete, play, assign to slots, edit shortcuts
- **`.tpm` format** (JSON v1) in `~/.config/Papiro/macros/`

### 📋 Clip Library
- **Snippets** with title, category, tags, usage tracking
- **Dock widget** (Ctrl+Alt+L) with search, filter, tree view
- **Insert** via double-click, button, or context menu
- **Copy to clipboard**
- **JSON persistence** in `~/.config/Papiro/cliplib.json`

### 🔧 External Tools
- **Tool Manager** — configure commands with `$FILE`, `$FILEDIR`, `$FILENAME`, `$FILEEXT`
- **Auto-save before run** (per-tool option)
- **Output dock** (Ctrl+Alt+O) — stdout/stderr, exit code, error parsing
- **JSON persistence** in `~/.config/Papiro/tools.json`

### 🌐 Internationalization
- **3 languages**: Portuguese (PT), English (EN), Spanish (ES)
- **266 translated strings** via Qt Linguist (`.ts`/`.qm`)
- **Auto-detect** from system locale (`pt_BR` → `pt`)
- **Menu**: View → Language
- **Correct plurals** (numerusform)

### 📝 Spell Checking (Hunspell)
- **Dictionaries**: pt_BR, en_US, es_ES (auto-found in standard paths)
- **Wavy red underline** on misspelled words
- **Context menu** suggestions + "Add to dictionary"
- **Check Spelling** (F7) toggle in View menu

### 🎨 Themes
- **Light / Dark** themes with instant switching (View → Theme)
- **Syntax colors** adapt per theme (keyword, comment, string, number, etc.)
- **No flash** — palette applied before window shows
- **Persisted** in QSettings

### 🖨️ Printing
- **Print** (Ctrl+P) via QPrintDialog
- **Print Preview** via QPrintPreviewDialog

### 📦 Packaging
- **Linux**: AppImage, DEB, RPM, portable ZIP
- **Windows**: Qt Installer Framework (NSIS), portable ZIP
- **CI/CD**: GitHub Actions (linux-release.yml, windows-release.yml)

## 🛠️ Development

### Prerequisites
- Qt 6.8+ (Core, Widgets, PrintSupport, LinguistTools)
- CMake 3.21+, C++17 compiler
- Hunspell dev headers (optional, for spell checking)
- OpenGL (for Qt rendering)

### Build
```bash
export PATH="$HOME/.local/bin:$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64:$HOME/.local/gl"
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DOpenGL_GL_PREFERENCE=LEGACY \
  -DOPENGL_INCLUDE_DIR=$HOME/.local/gl/usr/include \
  -DOPENGL_gl_LIBRARY=$HOME/.local/gl/usr/lib/x86_64-linux-gnu/libGL.so .
cmake --build build --config Release -j$(nproc)
```

### Run Tests (Qt Test, offscreen)
```bash
ctest --test-dir build --output-on-failure
```

### Translations
```bash
$HOME/Qt/6.8.3/gcc_64/bin/lupdate -no-obsolete src \
  -ts translations/papiro_pt.ts translations/papiro_en.ts translations/papiro_es.ts
# Fill <translation> tags, then rebuild (lrelease runs during build)
```

### Icons
```bash
sudo apt install inkscape imagemagick
cd resources/icons && ./generate-icons.sh
```

## 🏗️ Architecture

```
src/
  core/     Encoding, SyntaxDefinition, Macro, ClipLibrary, SpellChecker,
            ExternalTool, Workspace                    (no GUI deps)
  editor/   Editor (folding, auto-close, bracket match), SyntaxHighlighter,
            BlockState
  app/      Document, MainWindow
  ui/       Theme, FindReplaceDialog, MacroManagerDialog, PreferencesDialog,
            ClipLibraryWidget, FileExplorerWidget, ToolsDialog,
            KeyboardShortcutsDialog
resources/
  syntax/   *.syn definitions
translations/  papiro_{pt,en,es}.ts → :/i18n/*.qm
tests/      test_core, test_editor, test_mainwindow (Qt Test)
```

## 📄 License

MIT License — see [LICENSE](LICENSE) for details.

---

**Papiro** — Portuguese for *papyrus*. A clean slate for your code.