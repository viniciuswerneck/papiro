#pragma once

#include "editor/BlockState.h"

#include <QPlainTextEdit>
#include <QTextEdit>
#include <QHash>

namespace papiro {

class SyntaxDefinition;
class SyntaxHighlighter;
class MacroManager;
class SpellChecker;

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
};

} // namespace papiro
