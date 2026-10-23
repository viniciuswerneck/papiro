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

### Preferências
- [x] Diálogo único em **Editar → Preferências...** (`Ctrl+,`)
- [x] Fonte do editor, tema, restauração de sessão e opções de exibição
- [x] Padrões de busca (case, palavra inteira, regex, wrap) aplicados ao Find/Replace
- [x] Alterações aplicadas na hora e persistidas em `QSettings`
- [x] Teste de integração do diálogo e da aplicação das preferências

### Macros
- [x] `Macro` / `MacroAction` / `MacroManager` (core)
- [x] Gravação de teclas, texto e comandos de editor
- [x] Texto real digitado preservado (minúsculas, acentos e IME), não só key codes
- [x] Replay com loop e coalescência de ações
- [x] 9 slots built-in (Ctrl+Shift+1..9)
- [x] Formato `.tpm` (JSON v1), diretório `~/.config/Papiro/macros/`
- [x] Menu Macros + toolbar (Rec / Stop / Play)
- [x] Gravação salva automaticamente em `~/.config/Papiro/macros/`
- [x] Gerenciador real: renomear, excluir, reproduzir, atribuir/limpar slots
- [x] Atribuições dos slots persistidas em `slots.json` e restauradas ao abrir
- [x] Nomes dos slots aparecem no menu Macros e nos atalhos Ctrl+Shift+1..9

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
- [x] 266 strings extraídas com `lupdate`, traduzidas para **PT**, **EN** e **ES**
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

### Sessão automática
- [x] Reabre os arquivos da sessão anterior com aba ativa (QSettings)
- [x] Restaura posição do cursor (linha/coluna) de cada arquivo
- [x] Restaura workspace aberto e layout dos docks/toolbar (`saveState`)
- [x] Arquivo → **Restaurar Sessão Anterior** (ligável/desligável, persistido);
      desligado, inicia com um documento vazio e limpa a sessão salva

### Painel de resultados de busca
- [x] Diálogo Find/Replace com botão **Find All**
- [x] Dock **Search Results** (Ctrl+Alt+F) com agrupamento por arquivo,
      linha, coluna e prévia do texto
- [x] Busca em todas as abas abertas, respeitando case/whole-word/regex
- [x] Duplo-clique/Enter navega até a ocorrência
- [x] Limite de 5.000 resultados por busca (aviso de truncamento)

### Testes automatizados (Qt Test)
- [x] `test_core`: encoding/EOL, parser `.syn`, macros (JSON e texto),
      workspace `.ppw` e Clip Library
- [x] `test_editor`: auto-fechamento, wrap de seleção, backspace de par,
      auto-indentação, go-to-line/coluna, duplicar/deletar linha
- [x] `test_mainwindow`: restauração de sessão (abrir → fechar → reabrir),
      sessão desativada e painel de resultados de busca
- [x] `ctest` habilitado no CMake (`BUILD_TESTING`), rodando em `offscreen`
- [x] Correção descoberta pelos testes: validação UTF-8 estrita (sequências
      incompletas/overlong não eram detectadas como Windows-1252)

### Syntax multi-linha
- [x] Estado de sintaxe persistente entre blocos (block comments e strings)
- [x] Delimitadores multi-linha configuráveis por `.syn`
      (`MultilineStringDelimiters`)
- [x] Python: `"""` e `'''` atravessam linhas
- [x] JavaScript/TypeScript: template literals com backtick
- [x] Strings normais (`"`, `'`) não vazam mais para a linha seguinte
- [x] Delimitadores ordenados por tamanho (evita `"` vencer `"""`)
- [x] `QTextBlock::userState` separado: bits baixos = sintaxe, bit `0x1000` =
      folding; rehighlight não perde mais o estado de dobra

### Temas claro/escuro
- [x] `ui/Theme` centraliza paletas e cores de sintaxe
- [x] Menu **View → Theme** com Light/Dark, persistido em `ui/theme`
- [x] Paleta global aplicada antes da janela abrir (sem “flash” claro)
- [x] Cores do editor: linha atual, par de colchetes, gutter e sintaxe
      (keyword/comment/string/número) trocam junto com o tema
- [x] `Editor::refreshTheme()` e `SyntaxHighlighter::refreshTheme()` atualizam
      abas abertas na hora
- [x] Teste garantindo que o highlighter muda de cor e que as paletas diferem

### Busca em pasta/arquivos
- [x] Campo de pasta no diálogo de busca, com botão para escolher diretório
- [x] Botão **Find in Folder** varre subpastas recursivamente
- [x] Resultados agrupados por caminho relativo, com linha/coluna/prévia
- [x] Clique no resultado abre o arquivo (mesmo fora das abas abertas) e
      posiciona o cursor
- [x] Limites de segurança: 5 MB por arquivo, 5000 arquivos e 5000 resultados
- [x] Detecção de binários e leitura com o mesmo `decodeFile` do editor
- [x] **Find in Folder...** também no botão direito do explorador de arquivos

### Menu de contexto e seleção em bloco
- [x] Menu de contexto próprio no editor: desfazer/refazer, recortar, copiar,
      colar, excluir, selecionar tudo, duplicar/excluir linha, dobrar bloco,
      localizar/substituir, ir para linha e sugestões ortográficas
- [x] Seleção retangular com **Alt+arrastar** ou **Column Selection Mode**
      (ativável pelo botão direito)
- [x] Copiar/recortar/excluir em bloco preservam colunas, com padding de espaços
- [x] Colar sobre seleção em bloco substitui a região e mantém o fluxo normal
- [x] Desenho da seleção em bloco com a cor de destaque do tema
- [x] Testes cobrindo seleção programática, Alt+arrastar, copiar e recortar

---

## 🚧 Em andamento

- [ ] **Atalhos personalizados de macros** — combinações além dos 9 slots fixos

---

## 📁 Estrutura

```
src/
  core/     Encoding, SyntaxDefinition, Macro, ClipLibrary, SpellChecker,
            ExternalTool, Workspace                    (sem GUI)
  editor/   Editor (folding, auto-close, bracket match), SyntaxHighlighter,
            BlockState
  app/      Document, MainWindow
  ui/       Theme, FindReplaceDialog, MacroManagerDialog, PreferencesDialog,
            ClipLibraryWidget, FileExplorerWidget, ToolsDialog
resources/
  syntax/   *.syn
translations/ papiro_{pt,en,es}.ts -> :/i18n/*.qm
tests/      test_core, test_editor, test_mainwindow (Qt Test)
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

# Testes (Qt Test, rodam em offscreen)
ctest --test-dir build --output-on-failure
```

## 🌐 Traduções

```bash
$HOME/Qt/6.8.3/gcc_64/bin/lupdate -no-obsolete src \
  -ts translations/papiro_pt.ts translations/papiro_en.ts translations/papiro_es.ts
# preencher <translation> e rebuildar (lrelease roda no build)
```

## 📌 Estado ao encerrar (23/10/2026)

- Build Release atualizado e funcionando (`build/Papiro`)
- Testes: `core`, `editor` e `mainwindow` passando (100%)
- App em execução na sessão Wayland (último PID conhecido: 709893)
- Dependências locais:
  - Qt 6.8.3: `~/Qt/6.8.3/gcc_64`
  - OpenGL: `~/.local/gl`
  - Hunspell dev/libs: `~/.local/hunspell`
  - Dicionários: `~/.local/share/hunspell` (pt_BR, es_ES, en_US)
- Configs do usuário: `~/.config/Papiro/` (`cliplib.json`, `macros/`, `tools.json`,
  `Papiro.conf` com idioma/fonte/workspaces recentes/sessão)
- `TextPad.exe` é o alvo de análise e **não** deve ser modificado nem commitado

## ▶️ Retomar amanhã

1. Recompilar: comando em **Build** acima (já configurado, basta `cmake --build build`)
2. Rodar: `cd build && setsid nohup ./Papiro > /tmp/papiro_run.log 2>&1 &`
3. Verificar: `ctest --test-dir build --output-on-failure`
4. Próximos passos sugeridos:
   - [ ] **Atalhos de macros** — permitir combinações além dos 9 slots fixos
   - [ ] **Empacotamento** — AppImage/instalador e atalho de desktop

## 🔍 Verificações rápidas

- Folding validado com teste isolado (dobrar bloco externo oculta linhas internas)
- Spell checker validado: `pt_BR` carrega, aceita "casa" e rejeita "xzqwv"
- Traduções: 266/266 finalizadas em PT, EN e ES
- `ctest`: 3/3 suítes passando (core, editor, mainwindow)
- Atalhos de macros validados em teste: `Ctrl+F9`, `Ctrl+F10`,
  `Ctrl+Shift+F9` e `Ctrl+Shift+1`
- Preferências validadas: tema aplicado na hora e opções de busca repassadas
  ao diálogo Find/Replace
- Syntax multi-linha validado: block comment, `"""`/`'''` e template literals,
  com estado de folding preservado após rehighlight
- Busca em pasta validada com subdiretórios e abertura do resultado
- Seleção em bloco validada com Alt+arrastar, copiar e recortar
- Macros validadas: gravação, persistência em disco, renomeação, exclusão e
  restauração das atribuições dos slots
- Reprodução de macros preserva o texto real digitado (minúsculas e acentos),
  incluindo entrada por IME/dead keys

