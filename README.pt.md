# Papiro

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/viniciuswerneck/papiro)](https://github.com/viniciuswerneck/papiro/releases)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/linux-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions/workflows/linux-release.yml)
[![GitHub Actions](https://github.com/viniciuswerneck/papiro/actions/workflows/windows-release.yml/badge.svg)](https://github.com/viniciuswerneck/papiro/actions/workflows/windows-release.yml)

> Um editor de texto focado, multiplataforma, construído com **C++17 / Qt 6.8**.

## 📥 Instalação

### Linux

#### AppImage (recomendado, universal)
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
yay -S papiro  # ou paru -S papiro
```

### Windows

#### Instalador (QtIFW)
Baixe `Papiro-0.1.0-windows-x64.exe` em [Releases](https://github.com/viniciuswerneck/papiro/releases) e execute.

#### ZIP Portátil
Baixe `Papiro-0.1.0-windows-x64-portable.zip`, extraia em qualquer lugar, execute `Papiro.exe`.

### Compilar do Código-Fonte
```bash
# Linux
export PATH="$HOME/.local/bin:$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64:$HOME/.local/gl"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOpenGL_GL_PREFERENCE=LEGACY -DOPENGL_INCLUDE_DIR=$HOME/.local/gl/usr/include -DOPENGL_gl_LIBRARY=$HOME/.local/gl/usr/lib/x86_64-linux-gnu/libGL.so .
cmake --build build --config Release -j$(nproc)
./build/Papiro
```

## ✨ Funcionalidades

### 🎨 Editor Principal
- **Destaque de sintaxe** para 9 linguagens: C++, Python, JavaScript, HTML, CSS, Java, JSON, Markdown, Texto
- **Strings e comentários multi-linha** (Python `"""`/`'''`, JS template literals, C-style `/* */`)
- **Indentação inteligente** em `{`, `[`, `(`, `:`
- **Fechamento automático de pares** `()[]{}""''` com envolvimento de seleção
- **Backspace** remove pares vazios de uma vez
- **Correspondência de colchetes** destacada
- **Gutter com números de linha** e destaque da linha atual
- **Dobramento de código** (baseado em chaves, marcadores ▼/▶ no gutter, Ctrl+Shift+. para alternar)
- **Seleção retangular/em bloco** (Alt+arrastar ou Modo Seleção em Coluna)
- **Zoom** in/out/reset (Ctrl++ / Ctrl+- / Ctrl+0)
- **Quebra de linha** alternável
- **Fonte configurável**

### 📁 Gerenciamento de Arquivos e Projetos
- **Interface com abas** com reordenação por arrastar, botões de fechar
- **Explorador de Arquivos** dock (Ctrl+Alt+E) com abertura de pasta, filtros, menu de contexto
- **Workspaces** (`.ppw`) — salva/restaura arquivos abertos, aba ativa, raiz do explorador
- **Restauração de sessão** — reabre arquivos, posições do cursor, layout dos docks/toolbars
- **Arquivos e workspaces recentes** (10 cada, persistidos)
- **Arrastar e soltar** arquivos para abrir
- **Detecção e conversão de Encoding/EOL** (UTF-8, UTF-16, Windows-1252; LF/CRLF/CR)

### 🔍 Busca e Substituição
- **Diálogo Find/Replace não-modal** (Ctrl+F / Ctrl+H)
- **Regex** (QRegularExpression), opções case/whole-word/wrap
- **Replace All** com backreferences (`\1`, `\n`, `\t`)
- **Busca em Arquivos** — busca recursiva em pasta, limites de 5000 arquivos/resultados, detecção de binários
- **Dock Resultados de Busca** (Ctrl+Alt+F) — agrupado por arquivo, linha/coluna/prévia, duplo-clique navega

### ⌨️ Macros
- **Gravar/Reproduzir** (Ctrl+F9 / Ctrl+F10 / Ctrl+Shift+F9)
- **9 slots built-in** (Ctrl+Shift+1..9) com atribuições persistidas
- **Atalhos personalizados** — atribua qualquer combinação via Gerenciador de Macros
- **Captura real de teclas** — preserva maiúsculas, acentos, entrada IME
- **Reprodução coalescida** — sequências de ações otimizadas
- **Gerenciador de Macros** — renomear, excluir, reproduzir, atribuir a slots, editar atalhos
- **Formato `.tpm`** (JSON v1) em `~/.config/Papiro/macros/`

### 📋 Biblioteca de Clips
- **Snippets** com título, categoria, tags, rastreamento de uso
- **Dock widget** (Ctrl+Alt+L) com busca, filtro, tree view
- **Inserir** via duplo-clique, botão ou menu de contexto
- **Copiar para clipboard**
- **Persistência JSON** em `~/.config/Papiro/cliplib.json`

### 🔧 Ferramentas Externas
- **Gerenciador de Ferramentas** — configure comandos com `$FILE`, `$FILEDIR`, `$FILENAME`, `$FILEEXT`
- **Salvar automaticamente antes de executar** (opção por ferramenta)
- **Dock de Saída** (Ctrl+Alt+O) — stdout/stderr, código de saída, parsing de erros
- **Persistência JSON** em `~/.config/Papiro/tools.json`

### 🌐 Internacionalização
- **3 idiomas**: Português (PT), Inglês (EN), Espanhol (ES)
- **266 strings traduzidas** via Qt Linguist (`.ts`/`.qm`)
- **Detecção automática** pelo locale do sistema (`pt_BR` → `pt`)
- **Menu**: Exibir → Idioma
- **Plurais corretos** (numerusform)

### 📝 Corretor Ortográfico (Hunspell)
- **Dicionários**: pt_BR, en_US, es_ES (auto-encontrados em caminhos padrão)
- **Sublinhado vermelho ondulado** em palavras incorretas
- **Menu de contexto** com sugestões + "Adicionar ao dicionário"
- **Verificar Ortografia** (F7) alternável no menu Exibir

### 🎨 Temas
- **Claro / Escuro** com troca instantânea (Exibir → Tema)
- **Cores de sintaxe** adaptam-se por tema (keyword, comment, string, number, etc.)
- **Sem "flash"** — paleta aplicada antes da janela abrir
- **Persistido** em QSettings

### 🖨️ Impressão
- **Imprimir** (Ctrl+P) via QPrintDialog
- **Visualizar Impressão** via QPrintPreviewDialog

### 📦 Empacotamento
- **Linux**: AppImage, DEB, RPM, ZIP portátil
- **Windows**: Qt Installer Framework (NSIS), ZIP portátil
- **CI/CD**: GitHub Actions (linux-release.yml, windows-release.yml)

## 🛠️ Desenvolvimento

### Pré-requisitos
- Qt 6.8+ (Core, Widgets, PrintSupport, LinguistTools)
- CMake 3.21+, compilador C++17
- Headers de desenvolvimento do Hunspell (opcional, para corretor ortográfico)
- OpenGL (para renderização do Qt)

### Compilação
```bash
export PATH="$HOME/.local/bin:$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64:$HOME/.local/gl"
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DOpenGL_GL_PREFERENCE=LEGACY \
  -DOPENGL_INCLUDE_DIR=$HOME/.local/gl/usr/include \
  -DOPENGL_gl_LIBRARY=$HOME/.local/gl/usr/lib/x86_64-linux-gnu/libGL.so .
cmake --build build --config Release -j$(nproc)
```

### Executar Testes (Qt Test, offscreen)
```bash
ctest --test-dir build --output-on-failure
```

### Traduções
```bash
$HOME/Qt/6.8.3/gcc_64/bin/lupdate -no-obsolete src \
  -ts translations/papiro_pt.ts translations/papiro_en.ts translations/papiro_es.ts
# Preencha as tags <translation>, depois recompile (lrelease roda no build)
```

### Ícones
```bash
sudo apt install inkscape imagemagick
cd resources/icons && ./generate-icons.sh
```

## 🏗️ Arquitetura

```
src/
  core/     Encoding, SyntaxDefinition, Macro, ClipLibrary, SpellChecker,
            ExternalTool, Workspace                    (sem dependências GUI)
  editor/   Editor (folding, auto-close, bracket match), SyntaxHighlighter,
            BlockState
  app/      Document, MainWindow
  ui/       Theme, FindReplaceDialog, MacroManagerDialog, PreferencesDialog,
            ClipLibraryWidget, FileExplorerWidget, ToolsDialog,
            KeyboardShortcutsDialog
resources/
  syntax/   definições *.syn
translations/  papiro_{pt,en,es}.ts → :/i18n/*.qm
tests/      test_core, test_editor, test_mainwindow (Qt Test)
```

## 📄 Licença

Licença MIT — veja [LICENSE](LICENSE) para detalhes.

---

**Papiro** — Uma folha em branco para o seu código.