# Papiro — Progresso do Desenvolvimento

> Clone independente de editor de texto inspirado em editores comerciais (sem copiar código/assets).
> Stack: **C++17 / Qt 6.8** · Linux + Windows · i18n PT/EN/ES

---

## ✅ Concluído

### Fundação
- [x] Build system CMake + Ninja/Qt6 (Release)
- [x] Instalação local do Qt 6.8.3 + dependências OpenGL (`~/.local/gl`)
- [x] Estrutura modular: `core` (sem GUI) / `editor` / `app` / `ui`

### Núcleo de texto
- [x] Detecção de encoding: UTF-8, UTF-8 BOM, UTF-16 LE/BE, Windows-1252
- [x] Normalização de fim de linha: LF, CRLF, CR
- [x] Parser de definições de sintaxe (`.syn`, estilo INI)
- [x] 9 definições embutidas: C++, Python, JS, HTML, CSS, Java, JSON, Markdown, Text
- [x] Syntax highlighting genérico dirigido por `SyntaxDefinition`
- [x] Detecção automática de sintaxe pela extensão

### Editor
- [x] `Editor` (QPlainTextEdit) com gutter de números de linha
- [x] Destaque da linha atual
- [x] Duplicar linha (Ctrl+D) / Deletar linha (Ctrl+Shift+K)
- [x] Go to Line (Ctrl+G)
- [x] Zoom in/out/reset
- [x] Word wrap toggle
- [x] Fonte configurável
- [x] Correspondência de parênteses/colchetes/chaves destacada
- [x] Auto-indentação em `{`, `[`, `(`, `:`
- [x] Auto-fechamento de pares `()[]{}"" ''` (com envolver seleção)
- [x] Backspace remove par vazio de uma vez

### Documento & persistência
- [x] Modelo `Document` com save atômico
- [x] Rastreamento de encoding/EOL e conversão na gravação
- [x] Estado "modificado" e título com `*`

### Interface principal
- [x] Abas (`QTabWidget`) com fechar/mover
- [x] Menus: File, Edit, Search, Document, View, Macros, Help
- [x] Toolbar principal
- [x] Barra de status (posição, seleção, sintaxe, encoding, EOL)
- [x] Arquivos recentes (10 itens, persistido)
- [x] Drag-and-drop de arquivos
- [x] Persistência de geometria/fonte/preferências (QSettings)

### Busca & substituição
- [x] Diálogo Find/Replace não-modal
- [x] Regex (QRegularExpression), case/whole-word/wrap
- [x] Replace All com backrefs (`\1`, `\n`, `\t`)

### Macros
- [x] `Macro` / `MacroAction` / `MacroManager` (core)
- [x] Gravação de teclas, texto e comandos de editor
- [x] Replay com loop e coalescência de ações
- [x] 9 slots built-in (Ctrl+Shift+1..9)
- [x] Formato `.tpm` (JSON v1), diretório `~/.config/Papiro/macros/`
- [x] Menu Macros + toolbar (Rec / Stop / Play)
- [x] Gerenciador de macros (diálogo informativo)

### Clip Library
- [x] `Clip` / `ClipLibrary` (core) com UUID, categorias, tags, uso
- [x] Persistência JSON em `~/.config/Papiro/cliplib.json`
- [x] `ClipLibraryWidget`: busca, filtro por categoria, tree view
- [x] Diálogo de edição de clip (título, categoria, tags, conteúdo)
- [x] Inserir por duplo-clique / botão / menu de contexto
- [x] Dock widget à direita, toggle Ctrl+Alt+L
- [x] Copiar para clipboard

---

### Internacionalização (i18n)
- [x] Infraestrutura `QTranslator` + `.ts`/`.qm` embutidos (`:/i18n`)
- [x] 182 strings extraídas com `lupdate`, traduzidas para **PT** e **ES** (EN canônico)
- [x] Menu **Exibir → Idioma** (English / Português / Español), persistido em QSettings
- [x] Seleção automática pelo locale do sistema (`pt_BR` → `pt`)
- [x] Plural correto (`numerusform`)

### Corretor ortográfico (hunspell)
- [x] `SpellChecker` (core) com carregamento de dicionário, cache e sugestões
- [x] Detecção opcional do hunspell no CMake (`PAPIRO_HAVE_HUNSPELL`)
- [x] Dicionários pt_BR, es_ES e en_US (busca em `~/.local/share/hunspell` e locais padrão)
- [x] Sublinhado vermelho ondulado em palavras incorretas no `SyntaxHighlighter`
- [x] Sugestões + "Adicionar ao dicionário" no menu de contexto do editor
- [x] Verificar Ortografia (F7) em Exibir, persistido em QSettings

### Explorador de arquivos
- [x] Dock esquerdo com árvore de diretórios (`QFileSystemModel`)
- [x] Filtro de arquivos por nome, ordenação e ocultação de colunas
- [x] Duplo-clique abre o arquivo em uma aba
- [x] Menu de contexto: Abrir / Copiar Caminho
- [x] File → Open Folder... (Ctrl+Shift+O), persistência da última pasta
- [x] Exibir → File Explorer (Ctrl+Alt+E)

### Ferramentas externas
- [x] `ExternalTool` / `ToolManager` (core) com persistência JSON (`~/.config/Papiro/tools.json`)
- [x] Menu **Ferramentas** com execução e configuração
- [x] Variáveis `$FILE`, `$FILEDIR`, `$FILENAME`, `$FILEEXT`
- [x] Salvar automaticamente antes de executar (opcional por ferramenta)
- [x] Dock **Saída** com stdout/stderr, código de saída e erros (Ctrl+Alt+O)
- [x] Diálogo de configuração (adicionar/remover/aplicar)

### Projetos / Workspace
- [x] `Workspace` (core) com persistência JSON `.ppw` (arquivos, aba ativa, pasta raiz)
- [x] Arquivo → Abrir Workspace... (Ctrl+Shift+W) / Salvar Workspace Como... (Ctrl+Alt+S)
- [x] Submenu de workspaces recentes (10 itens, persistido)
- [x] Abrir workspace pergunta sobre alterações não salvas e restaura o explorador
- [x] Nome do workspace exibido no título da janela

### Impressão
- [x] Imprimir (Ctrl+P) via QPrintDialog
- [x] Visualizar Impressão via QPrintPreviewDialog
- [x] Dependência `Qt6::PrintSupport` adicionada ao build

### Dobramento (folding)
- [x] Detecção de blocos `{`, `[`, `(` com fechamento em linhas seguintes
- [x] Marcadores ▼/▶ no gutter; clique para dobrar/desdobrar
- [x] Documento → Folding: Alternar Dobra (Ctrl+Shift+.), Dobrar Tudo, Desdobrar Tudo
- [x] Estado de dobra mantido por bloco (`QTextBlock::userState`)
- [x] Recalculado automaticamente ao editar

---

## 🚧 Em andamento

- [ ] **Syntax avançado** — estados multi-linha e realce adicionais
- [ ] **Sessão automática** — restaurar arquivos abertos ao iniciar

---

## 📁 Estrutura

```
src/
  core/     Encoding, SyntaxDefinition, Macro, ClipLibrary, SpellChecker,
            ExternalTool, Workspace                    (sem GUI)
  editor/   Editor (folding, auto-close, bracket match), SyntaxHighlighter
  app/      Document, MainWindow
  ui/       FindReplaceDialog, ClipLibraryWidget, FileExplorerWidget, ToolsDialog
resources/
  syntax/   *.syn
translations/ papiro_{pt,en,es}.ts -> :/i18n/*.qm
```

## 🔨 Build

```bash
export PATH="$HOME/.local/bin:$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64:$HOME/.local/gl"
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DOpenGL_GL_PREFERENCE=LEGACY \
  -DOPENGL_INCLUDE_DIR=$HOME/.local/gl/usr/include \
  -DOPENGL_gl_LIBRARY=$HOME/.local/gl/usr/lib/x86_64-linux-gnu/libGL.so .
cmake --build build --config Release -j$(nproc)
./build/Papiro
```

## 🌐 Traduções

```bash
$HOME/Qt/6.8.3/gcc_64/bin/lupdate -no-obsolete src \
  -ts translations/papiro_pt.ts translations/papiro_en.ts translations/papiro_es.ts
# preencher <translation> e rebuildar (lrelease roda no build)
```

## 📌 Estado ao encerrar (23/10/2026)

- Build Release atualizado e funcionando (`build/Papiro`)
- App em execução na sessão Wayland (último PID conhecido: 98859)
- Dependências locais:
  - Qt 6.8.3: `~/Qt/6.8.3/gcc_64`
  - OpenGL: `~/.local/gl`
  - Hunspell dev/libs: `~/.local/hunspell`
  - Dicionários: `~/.local/share/hunspell` (pt_BR, es_ES, en_US)
- Configs do usuário: `~/.config/Papiro/` (`cliplib.json`, `macros/`, `tools.json`,
  `Papiro.conf` com idioma/fonte/workspaces recentes)
- `TextPad.exe` é o alvo de análise e **não** deve ser modificado nem commitado

## ▶️ Retomar amanhã

1. Recompilar: comando em **Build** acima (já configurado, basta `cmake --build build`)
2. Rodar: `cd build && setsid nohup ./Papiro > /tmp/papiro_run.log 2>&1 &`
3. Próximos passos sugeridos:
   - [ ] **Sessão automática** — reabrir arquivos/aba ao iniciar (QSettings)
   - [ ] **Painel de resultados de busca** — lista de ocorrências no dock inferior
   - [ ] **Syntax avançado** — estados multi-linha extras (heredoc, template strings)
   - [ ] **Temas claro/escuro** + cores de sintaxe configuráveis
   - [ ] **Testes automatizados** (Qt Test) para core e editor

## 🔍 Verificações rápidas

- Folding validado com teste isolado (dobrar bloco externo oculta linhas internas)
- Spell checker validado: `pt_BR` carrega, aceita "casa" e rejeita "xzqwv"
- Traduções: 182/182 finalizadas em PT, EN e ES

