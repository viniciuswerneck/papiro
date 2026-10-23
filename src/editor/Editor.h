#pragma once

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

    void gotoLine(int lineNumber);
    int currentLine() const;
    int currentColumn() const;
    void duplicateCurrentLine();
    void deleteCurrentLine();

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

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

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

    static constexpr int FoldedBlockState = 0x1000;

private:
    QWidget *m_lineNumberArea = nullptr;
    SyntaxHighlighter *m_highlighter = nullptr;
    MacroManager *m_macroManager = nullptr;
    SpellChecker *m_spellChecker = nullptr;
    mutable QHash<int, int> m_foldEnds;
    mutable bool m_foldCacheDirty = true;
    bool m_foldsActive = false;
    bool m_lineNumbersVisible = true;
};

} // namespace papiro
