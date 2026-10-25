# Papiro

[![Languages](https://img.shields.io/badge/lang-EN%20%7C%20PT%20%7C%20ES-blue.svg)]()

[🇺🇸 English](README.md) • [🇧🇷 Português](README.pt.md) • [🇪🇸 Español](README.es.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/viniciuswerneck/papiro)](https://github.com/viniciuswerneck/papiro/releases)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/linux-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions/workflows/linux-release.yml)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/windows-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions/workflows/windows-release.yml)

> Un editor de texto enfocado, multiplataforma, construido con **C++17 / Qt 6.8**.
>
> **Papiro** es un editor de texto avanzado estilo *programmer's editor* (como TextPad, Notepad++, EditPlus), hecho para desarrolladores y usuarios avanzados que necesitan:
> - Edición rápida de código y texto con *syntax highlighting* para 9 lenguajes
> - Manipulación de archivos grandes (GBs) sin trabar
> - Herramientas integradas: macros, diff, búsqueda en archivos, herramientas externas
> - Interfaz limpia, ligera y personalizable (temas claro/oscuro, fuente, layout)
> - Multiplataforma: Linux, Windows, futuro macOS
>
> Ideal para: programación, scripts, logs, configuración, markdown, CSV, cualquier texto.

## 📥 Instalación

### Linux

#### AppImage (recomendado, universal)
```bash
wget https://github.com/viniciuswerneck/papiro/releases/latest/download/Papiro-0.4.1-x86_64.AppImage
chmod +x Papiro-*.AppImage
./Papiro-*.AppImage
```

#### Debian/Ubuntu (.deb)
```bash
wget https://github.com/viniciuswerneck/papiro/releases/latest/download/papiro_0.4.1_amd64.deb
sudo apt install ./papiro_*.deb
```

#### Fedora/RHEL
Todavía no hay RPM publicado — use el AppImage de arriba o [compile desde el código fuente](#-compilar-desde-código-fuente).

#### Arch Linux (AUR)
```bash
yay -S papiro  # o paru -S papiro
```

### Windows

#### Instalador (QtIFW)
Descargue `Papiro-0.4.1-windows-x64.exe` de [Releases](https://github.com/viniciuswerneck/papiro/releases) y ejecútelo.

#### ZIP Portable
Descargue `Papiro-0.4.1-windows-x64-portable.zip`, extraiga en cualquier lugar, ejecute `Papiro.exe`.

### Compilar desde Código Fuente
```bash
# Linux
export PATH="$HOME/.local/bin:$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64:$HOME/.local/gl"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOpenGL_GL_PREFERENCE=LEGACY -DOPENGL_INCLUDE_DIR=$HOME/.local/gl/usr/include -DOPENGL_gl_LIBRARY=$HOME/.local/gl/usr/lib/x86_64-linux-gnu/libGL.so .
cmake --build build --config Release -j$(nproc)
./build/Papiro
```

## ✨ Características

### 🎨 Editor Principal
- **Resaltado de sintaxis** para 9 lenguajes: C++, Python, JavaScript, HTML, CSS, Java, JSON, Markdown, Texto
- **Strings y comentarios multi-línea** (Python `"""`/`'''`, JS template literals, C-style `/* */`)
- **Indentación inteligente** en `{`, `[`, `(`, `:`
- **Cierre automático de pares** `()[]{}""''` con envolvimiento de selección
- **Backspace** elimina pares vacíos de una vez
- **Coincidencia de corchetes** destacada
- **Gutter con números de línea** y resaltado de línea actual
- **Plegado de código** (basado en llaves, marcadores ▼/▶ en gutter, Ctrl+Shift+. para alternar)
- **Selección rectangular/en bloque** (Alt+arrastrar o Modo Selección en Columna)
- **Zoom** in/out/reset (Ctrl++ / Ctrl+- / Ctrl+0)
- **Ajuste de línea** alternable
- **Fuente configurable**

### 📁 Gestión de Archivos y Proyectos
- **Interfaz con pestañas** con reordenación por arrastrar, botones de cerrar
- **Explorador de Archivos** dock (Ctrl+Alt+E) con apertura de carpeta, filtros, menú contextual
- **Workspaces** (`.ppw`) — guarda/restaura archivos abiertos, pestaña activa, raíz del explorador
- **Restauración de sesión** — reabre archivos, posiciones del cursor, layout de docks/toolbars
- **Archivos y workspaces recientes** (10 cada uno, persistidos)
- **Arrastrar y soltar** archivos para abrir
- **Detección y conversión de Encoding/EOL** (UTF-8, UTF-16, Windows-1252; LF/CRLF/CR)

### 🔍 Buscar y Reemplazar
- **Diálogo Find/Replace no-modal** (Ctrl+F / Ctrl+H)
- **Regex** (QRegularExpression), opciones case/whole-word/wrap
- **Replace All** con backreferences (`\1`, `\n`, `\t`)
- **Buscar en Archivos** — búsqueda recursiva en carpeta, límites de 5000 archivos/resultados, detección de binarios
- **Dock Resultados de Búsqueda** (Ctrl+Alt+F) — agrupado por archivo, línea/columna/previa, doble-clic navega

### ⌨️ Macros
- **Grabar/Reproducir** (Ctrl+F9 / Ctrl+F10 / Ctrl+Shift+F9)
- **9 slots built-in** (Ctrl+Shift+1..9) con asignaciones persistidas
- **Atajos personalizados** — asigne cualquier combinación vía Gestor de Macros
- **Captura real de teclas** — preserva mayúsculas, acentos, entrada IME
- **Reproducción coalescida** — secuencias de acciones optimizadas
- **Gestor de Macros** — renombrar, eliminar, reproducir, asignar a slots, editar atajos
- **Formato `.tpm`** (JSON v1) en `~/.config/Papiro/macros/`

### 📋 Biblioteca de Clips
- **Snippets** con título, categoría, etiquetas, seguimiento de uso
- **Dock widget** (Ctrl+Alt+L) con búsqueda, filtro, vista en árbol
- **Insertar** via doble-clic, botón o menú contextual
- **Copiar al portapapeles**
- **Persistencia JSON** en `~/.config/Papiro/cliplib.json`

### 🔧 Herramientas Externas
- **Gestor de Herramientas** — configure comandos con `$FILE`, `$FILEDIR`, `$FILENAME`, `$FILEEXT`
- **Guardar automáticamente antes de ejecutar** (opción por herramienta)
- **Dock de Salida** (Ctrl+Alt+O) — stdout/stderr, código de salida, parsing de errores
- **Persistencia JSON** en `~/.config/Papiro/tools.json`

### 🌐 Internacionalización
- **3 idiomas**: Portugués (PT), Inglés (EN), Español (ES)
- **270 strings traducidas** via Qt Linguist (`.ts`/`.qm`)
- **Detección automática** por locale del sistema (`es_ES` → `es`)
- **Selector**: Ver → Idioma y Preferencias → Apariencia → Idioma
- **Predeterminado del sistema** sigue el locale del SO
- **Plurales correctos** (numerusform)

### 📝 Corrector Ortográfico (Hunspell)
- **Diccionarios**: pt_BR, en_US, es_ES (auto-encontrados en rutas estándar)
- **Subrayado rojo ondulado** en palabras incorrectas
- **Menú contextual** con sugerencias + "Añadir al diccionario"
- **Verificar Ortografía** (F7) alternable en menú Ver

### 🎨 Temas
- **Fluent Design** con design tokens (`resources/themes/fluent.json`) y hoja de estilo (`fluent.qss`)
- **Claro / Oscuro** con cambio instantáneo (Ver → Tema)
- **Barra de título Fluent** con Mica en Windows
- **Colores de sintaxis** se adaptan por tema (keyword, comment, string, number, etc.)
- **Sin "flash"** — paleta aplicada antes de mostrar la ventana
- **Persistido** en QSettings

### 🖨️ Impresión
- **Imprimir** (Ctrl+P) via QPrintDialog
- **Vista Preliminar de Impresión** via QPrintPreviewDialog

### 📦 Empaquetado
- **Linux**: AppImage, DEB, RPM, ZIP portable
- **Windows**: Qt Installer Framework (NSIS), ZIP portable
- **CI/CD**: GitHub Actions (linux-release.yml, windows-release.yml)

## 🛠️ Desarrollo

### Prerrequisitos
- Qt 6.8+ (Core, Widgets, PrintSupport, LinguistTools)
- CMake 3.21+, compilador C++17
- Headers de desarrollo de Hunspell (opcional, para corrector ortográfico)
- OpenGL (para renderizado de Qt)

### Compilación
```bash
export PATH="$HOME/.local/bin:$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64:$HOME/.local/gl"
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DOpenGL_GL_PREFERENCE=LEGACY \
  -DOPENGL_INCLUDE_DIR=$HOME/.local/gl/usr/include \
  -DOPENGL_gl_LIBRARY=$HOME/.local/gl/usr/lib/x86_64-linux-gnu/libGL.so .
cmake --build build --config Release -j$(nproc)
```

### Ejecutar Tests (Qt Test, offscreen)
```bash
ctest --test-dir build --output-on-failure
```

### Traducciones
```bash
$HOME/Qt/6.8.3/gcc_64/bin/lupdate -no-obsolete src \
  -ts translations/papiro_pt.ts translations/papiro_en.ts translations/papiro_es.ts
# Rellene las etiquetas <translation>, luego recompile (lrelease corre en el build)
```

### Iconos
```bash
sudo apt install inkscape imagemagick
cd resources/icons && ./generate-icons.sh
```

## 🏗️ Arquitectura

```
src/
  core/     Encoding, SyntaxDefinition, Macro, ClipLibrary, SpellChecker,
            ExternalTool, Workspace                    (sin dependencias GUI)
  editor/   Editor (folding, auto-close, bracket match), SyntaxHighlighter,
            BlockState
  app/      Document, MainWindow
  ui/       Theme, FindReplaceDialog, MacroManagerDialog, PreferencesDialog,
            ClipLibraryWidget, FileExplorerWidget, ToolsDialog,
            KeyboardShortcutsDialog
resources/
  syntax/   definiciones *.syn
translations/  papiro_{pt,en,es}.ts → :/i18n/*.qm
tests/      test_core, test_editor, test_mainwindow (Qt Test)
```

## 📄 Licencia

Licencia MIT — vea [LICENSE](LICENSE) para detalles.

---

**Papiro** — Una hoja en blanco para tu código.