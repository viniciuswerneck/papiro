#pragma once

#include "editor/BlockState.h"

#include "core/LSPClient.h"
#include "core/LSPTypes.h"

#include <QPlainTextEdit>
#include <QTextEdit>
#include <QHash>
#include <QTimer>

namespace papiro {

class SyntaxDefinition;
class SyntaxHighlighter;
class MacroManager;
class SpellChecker;
class LSPCompletionWidget;

/*! Plain text editing surface with a line number gutter and current line mark. */
class Editor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;

    void applySyntax(const SyntaxDefinition &definition);
    void clearSyntax();

    /*! Enables live spell checking (nullptr disables it). */
    void setSpellChecker(SpellChecker *checker);

    void setLineNumbersVisible(bool visible);
    bool lineNumbersVisible() const { return m_lineNumbersVisible; }

    void setBaseFont(const QFont &font);

    /*! Re-reads the current theme palette/colors. */
    void refreshTheme();

    void gotoLine(int lineNumber, int column = 1);
    int currentLine() const;
    int currentColumn() const;
    void duplicateCurrentLine();
    void deleteCurrentLine();

    // Block (column) selection
    void setBlockSelectionMode(bool enabled);
    bool blockSelectionMode() const { return m_blockSelectMode; }
    bool hasBlockSelection() const { return m_blockSelection; }
    QString blockSelectionText() const;
    void clearBlockSelection();
    void copySelection();
    void cutSelection();
    void pasteSelection();
    void deleteBlockSelection();
    /*! Creates a rectangular selection directly (block/column coordinates). */
    void selectBlock(int anchorBlock, int anchorColumn, int currentBlock, int currentColumn);

    /*! Clears any block selection before selecting the whole document. */
    void selectAll();

    // Code folding
    void toggleFoldAtCursor();
    void foldAll();
    void unfoldAll();
    void toggleFoldAtGutter(int y);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    // Macro recording hooks
    void setMacroManager(MacroManager *manager);
    void recordMacroAction(const QString &text);
    void recordMacroKeyPress(int key, Qt::KeyboardModifiers modifiers);
    void recordMacroCommand(const QString &command);

signals:
    void findRequested();
    void gotoLineRequested();
    void blockSelectionChanged();
    
    // LSP signals
    void lspDefinitionRequested(const LSP::TextDocumentPositionParams& params);
    void lspReferencesRequested(const LSP::ReferenceParams& params);
    void lspRenameRequested(const LSP::RenameParams& params);
    void lspDiagnosticsChanged();

public slots:
    void onLSPDefinitionResponse(const QVector<LSP::Location>& locations);
    void onLSPReferencesResponse(const QVector<LSP::Location>& locations);
    void onLSPRenameResponse(const std::optional<LSP::WorkspaceEdit>& edit);
    void onLSPPrepareRenameResponse(const std::optional<LSP::Range>& range);
    void onLSPDiagnosticsReceived(const QString& uri, const QVector<LSP::Diagnostic>& diagnostics);
    
    // LSP Completion
    void onLSPCompletionResponse(const QVector<LSP::CompletionItem>& items);
    void onLSPCompletionCancelled();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

private:
    bool handleAutoIndent();
    bool handlePairBackspace();
    bool handleAutoClose(QKeyEvent *event);
    int findMatchingBracket(int position) const;
    void addBracketMatchSelections(QList<QTextEdit::ExtraSelection> &selections);
    void refreshFoldCache();
    void applyFolds();
    void toggleFold(int blockNumber);

    // LSP integration
    void setupLSP();
    void teardownLSP();
    void sendDidOpen();
    void sendDidChange();
    void sendDidSave();
    void sendDidClose();
    void scheduleDidChange();
    void triggerGoToDefinition();
    void triggerFindReferences();
    void triggerRename();
    void disableLSP();
    void setLSPClient(LSPClient* client, const QString& languageId);
    void applyWorkspaceEdit(const LSP::WorkspaceEdit& edit);
    LSP::TextDocumentPositionParams makePositionParams() const;
    LSP::ReferenceParams makeReferenceParams() const;
    LSP::RenameParams makeRenameParams(const QString& newName) const;
    
    // LSP Completion
    void triggerCompletion();
    void showCompletionWidget(const QVector<LSP::CompletionItem>& items);
    void hideCompletionWidget();
    bool isCompletionVisible() const;
    bool handleCompletionKeyPress(QKeyEvent* event);
    QString completionPrefix() const;
    void applyCompletionItem(const LSP::CompletionItem& item);

private:
    struct BlockPoint {
        int block = -1;
        int column = 0;
        bool isValid() const { return block >= 0; }
        bool operator==(const BlockPoint &other) const
        {
            return block == other.block && column == other.column;
        }
    };

    BlockPoint blockPointAt(const QPoint &position) const;
    QTextCursor cursorForBlockPoint(const BlockPoint &point) const;
    void updateBlockSelection(const QPoint &position);
    int blockSelectionTop() const;
    int blockSelectionBottom() const;
    int blockSelectionLeft() const;
    int blockSelectionRight() const;
    QList<QRect> blockSelectionRects() const;

private:
    QWidget *m_lineNumberArea = nullptr;
    SyntaxHighlighter *m_highlighter = nullptr;
    MacroManager *m_macroManager = nullptr;
    SpellChecker *m_spellChecker = nullptr;
    mutable QHash<int, int> m_foldEnds;
    mutable bool m_foldCacheDirty = true;
    bool m_foldsActive = false;
    bool m_lineNumbersVisible = true;
    bool m_blockSelection = false;
    bool m_blockSelecting = false;
    bool m_blockSelectMode = false;
    BlockPoint m_blockAnchor;
    BlockPoint m_blockCursor;

    // LSP state
    LSPClient* m_lspClient = nullptr;
    QString m_lspLanguageId;
    int m_lspDocumentVersion = 0;
    bool m_lspEnabled = false;
    QString m_lspDocumentUri;
    QString m_documentPath;

    // Timer para debounce didChange
    QTimer m_didChangeTimer;
    QVector<LSP::TextDocumentContentChangeEvent> m_pendingChanges;

    // LSP Completion
    LSPCompletionWidget* m_completionWidget = nullptr;
    bool m_completionTriggeredManually = false;

public:
    void setDocumentPath(const QString& path);
    QString documentPath() const { return m_documentPath; }
};

} // namespace papiro
