# Comparação: Papiro (seu programa) vs TextPad (Helios Software Solutions)

> **Nota:** "Textpad Foco" provavelmente se refere ao **TextPad** (editor Windows da Helios), que tem um "modo foco" ou você pode ter digitado "foco" por engano. O TextPad não tem um produto chamado "Textpad Foco" específico.

---

## ✅ O que o **Papiro JÁ TEM** (e TextPad não tem ou é mais fraco)

| Feature | Papiro | TextPad |
|---------|--------|---------|
| **LSP (Language Server Protocol)** | ✅ Completo: completion, hover, go-to-def, find refs, rename, diagnostics | ❌ Não tem |
| **Git Integration** | ✅ Blame inline, diff view, commit navigation | ❌ Não tem nativo |
| **Code Folding** | ✅ Baseado em chaves `{ } [ ] ( )` | ❌ Não tem |
| **Multi-platform** | ✅ Linux, Windows (Qt 6) | ❌ Apenas Windows |
| **Temas Dark/Light** | ✅ Built-in, alternável | ⚠️ Apenas cores do sistema |
| **Clip Library (snippets)** | ✅ Built-in, categorias, tags | ⚠️ Add-ons separados (Clip Libraries) |
| **Spell Check (Hunspell)** | ✅ pt_BR, en_US, es_ES integrados | ⚠️ Usa dicionários do Windows |
| **Block/Column Selection Visual** | ✅ Feedback visual + operações completas | ✅ Tem modo, mas menos visual |
| **Auto-close brackets/quotes** | ✅ Inteligente (evita contrações) | ❌ Não tem |
| **Auto-indent inteligente** | ✅ Detecta `{ } [ ] ( ) :` | ✅ Tem, mas mais básico |
| **Bracket Matching** | ✅ Highlight par correspondente | ❌ Não tem |
| **Workspaces** | ✅ Salvar/restaurar sessão, projetos | ❌ Não tem conceito |
| **File Explorer dock** | ✅ Integrado | ❌ Não tem |
| **External Tools** | ✅ Configurável com output dock | ✅ Tem, com regex p/ erros |
| **i18n (UI)** | ✅ PT, EN, ES | ✅ 10+ idiomas |
| **Encoding/Line Endings** | ✅ UTF-8, Latin1, etc + LF/CRLF | ✅ Unicode, UNC paths |
| **Print/Print Preview** | ✅ Básico | ✅ Avançado (headers/footers) |

---

## ❌ O que o **TextPad TEM** e o **Papiro NÃO TEM**

| Feature | Descrição | Prioridade |
|---------|-----------|------------|
| **Suporte a arquivos巨大 (GBs)** | Edita arquivos até limite da memória virtual | 🔴 Alta (se necessário) |
| **Split View (2 views por arquivo)** | Ver mesmo arquivo em dois painéis (horizontal/vertical) | 🟡 Média |
| **Ordenação de linhas (Sort)** | Ordenar até 3 chaves (colunas) | 🟡 Média |
| **File Comparison (Diff)** | Ferramenta nativa de comparação de arquivos | 🟡 Média |
| **Print Preview avançado** | Headers/footers personalizáveis, page breaks | 🟢 Baixa |
| **Multiline Regex Search/Replace** | Regex que atravessa múltiplas linhas | 🟡 Média |
| **Drag & Drop entre documentos** | Arrastar texto entre abas/janelas | 🟢 Baixa |
| **Cursor "livre" (além do fim da linha)** | Posicionar cursor em área vazia do documento | 🟢 Baixa |
| **Display de whitespace (tabs/espaços)** | Mostrar `→` para tabs, `·` para espaços | 🟡 Média |
| **Integração Java JDK** | Detecta JDK, compila/executa Java nativamente | 🟢 Baixa (nichado) |
| **External tools com regex p/ erro** | Clicar em erro no output → pula pro arquivo:linha | 🟡 Média |
| **Mark Clean command** | Marcar buffer como "limpo" p/ controle de undo | 🟢 Baixa |
| **WildEdit (ferramenta separada)** | Batch edit em hierarquia de arquivos | 🟢 Baixa (ferramenta à parte) |
| **Clip Libraries como add-ons** | Bibliotecas de snippets baixáveis separadamente | 🟢 Baixa (Papiro tem built-in) |
| **Line joining/splitting inteligente** | Join lines preservando parágrafos no word-wrap | 🟢 Baixa |

---

## 🎯 Recomendações de Implementação (ordem de impacto)

### 1. **Whitespace Display** (Fácil, alto valor)
```cpp
// Em Editor.cpp - paintEvent ou SyntaxHighlighter
// Mostrar: tabs como →, espaços como ·, fim de linha como ¶
```

### 2. **Split View / Clone View** (Médio, alto valor)
```cpp
// MainWindow: action "Split View" → cria segunda Editor para mesmo Document
// Sincronizar scroll, cursor, selection entre views
```

### 3. **Multiline Regex** (Médio)
```cpp
// FindReplaceDialog: adicionar opção "Multiline"
// QRegularExpression::MultilineOption
```

### 4. **File Diff Tool** (Médio/Alto)
```cpp
// Novo dialog: DiffView (side-by-side ou unified)
// Usar algoritmo Myers ou QDiff (se disponível)
// GitDiffView já existe → generalizar para qualquer 2 arquivos
```

### 5. **Sort Lines** (Fácil)
```cpp
// Editor::sortLines(int keyCount = 1, bool ascending = true)
// Menu: Edit → Sort Lines
```

### 6. **Huge File Support** (Difícil, só se necessário)
```cpp
// Lazy loading / memory mapping
// QFile::map() ou mmap() direto
// Virtual scroll (render só linhas visíveis)
// Desativar syntax highlight em arquivos > 50MB
```

### 7. **External Tool Error Parsing** (Médio)
```cpp
// ToolManager: regex pattern para "file:line:col: message"
// Clicar no output → openFileAtLine(file, line)
```

### 8. **Print Preview Avançado** (Médio)
```cpp
// QPrintPreviewDialog + QTextDocument::print()
// Headers/footers: ${filename}, ${date}, ${page}, ${totalPages}
```

### 9. **Cursor Free Positioning** (Fácil)
```cpp
// Editor: setCursorConstrained(bool)
// Se false: cursor pode ir além do fim da linha (spaces virtuais)
```

---

## 📊 Resumo por Categoria

| Categoria | Papiro | TextPad | Vencedor |
|-----------|--------|---------|----------|
| **Edição Core** | ✅ Moderno (auto-close, indent, bracket match) | ✅ Sólido (block indent, sort) | **Papiro** |
| **Navegação/Busca** | ✅ Find in files, regex, LSP goto-def | ✅ Regex multiline, 2 views | **Empate** |
| **Programação** | ✅ **LSP, Git, Folding** | ❌ Básico (syntax, external tools) | **Papiro** 🏆 |
| **Arquivos Grandes** | ⚠️ Qt limita (~100MB prático) | ✅ **Virtual memory limit** | **TextPad** |
| **UI/UX** | ✅ Dock widgets, temas, i18n | ⚠️ Clássica Windows, toolbars | **Papiro** |
| **Extensibilidade** | ⚠️ Hardcoded (C++) | ✅ Macros, Clip Libs, WildEdit | **TextPad** |
| **Printing** | ✅ Básico | ✅ **Avançado (headers/footers)** | **TextPad** |

---

## 🚀 Próximos Passos Sugeridos

1. **Quick wins (1-2 dias cada):**
   - [x] Whitespace display (tabs/espaços/EOL)
   - [x] Sort lines (até 3 chaves)
   - [x] Cursor free positioning option
   - [x] Multiline regex option

2. **Médio prazo (1-2 semanas cada):**
   - [x] Split/Clone view
   - [x] Diff tool (reusar GitDiffView)
   - [x] External tool error parsing

3. **Longo prazo (se necessário):**
   - [x] Huge file support (memory mapping) — modo somente leitura com mmap e prévia limitada
   - [x] Print preview avançado

### Extras implementados além da lista

- [x] Mark Clean command (File → Mark Document Clean, `Ctrl+Shift+M`)
- [x] Drag & drop de texto entre abas
- [x] Integração Java JDK (detecta JDK, compila e executa `.java` em Tools)
- [x] Join/Split lines inteligente (`Ctrl+J` e Edit → Split Lines)

### Não aplicável / fora do escopo

- WildEdit é um aplicativo separado (batch edit em árvore de arquivos).
- Clip Libraries como add-ons: o Papiro já tem biblioteca de snippets embutida.


---

## 💡 Conclusão

**Papiro é mais moderno para desenvolvimento** (LSP, Git, folding, themes, cross-platform).
**TextPad vence em:** arquivos gigantes, split view, sorting, diff nativo, printing avançado, ecossistema de macros/add-ons.

Para um editor **moderno de código**, Papiro já está à frente. Para **edição de logs/arquivos de dados gigantes** ou **usuários Windows legacy**, TextPad ainda tem vantagens específicas.

> **Dica:** Se seu público-alvo são desenvolvedores, foque em LSP/DX. Se inclui analistas de log/data, priorize huge files + split view + sorting.