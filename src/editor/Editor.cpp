#include "editor/Editor.h"

#include "core/LSPClient.h"
#include "core/LSPTypes.h"
#include "core/Macro.h"
#include "core/SyntaxDefinition.h"
#include "core/SpellChecker.h"
#include "editor/SyntaxHighlighter.h"
#include "ui/LSPCompletionWidget.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextLine>
#include <QUrl>

namespace papiro {
namespace {

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(Editor *editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override { m_editor->lineNumberAreaPaintEvent(event); }
    void mousePressEvent(QMouseEvent *event) override
    {
        m_editor->toggleFoldAtGutter(event->position().toPoint().y());
    }

private:
    Editor *m_editor;
};

} // namespace

Editor::Editor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFrameShape(QFrame::NoFrame);
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(QLatin1Char(' ')));

    connect(this, &QPlainTextEdit::blockCountChanged, this,
            &Editor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &Editor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &Editor::highlightCurrentLine);
    connect(document(), &QTextDocument::contentsChanged, this, [this] {
        m_foldCacheDirty = true;
        if (m_blockSelection)
            clearBlockSelection();
        if (m_foldsActive)
            applyFolds();
        if (m_lspEnabled)
            scheduleDidChange();
    });

    // LSP timers
    m_didChangeTimer.setSingleShot(true);
    m_didChangeTimer.setInterval(300);
    connect(&m_didChangeTimer, &QTimer::timeout, this, &Editor::sendDidChange);

    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

Editor::~Editor()
{
    if (m_lspEnabled) {
        sendDidClose();
        teardownLSP();
    }
}

void Editor::setMacroManager(MacroManager *manager)
{
    m_macroManager = manager;
}

void Editor::recordMacroAction(const QString &text)
{
    if (m_macroManager && m_macroManager->recordingMacro()) {
        m_macroManager->recordingMacro()->addAction(MacroAction::fromTextInput(text));
    }
}

void Editor::recordMacroKeyPress(int key, Qt::KeyboardModifiers modifiers)
{
    if (m_macroManager && m_macroManager->recordingMacro()) {
        m_macroManager->recordingMacro()->addAction(MacroAction::fromKeyPress(key, modifiers));
    }
}

void Editor::recordMacroCommand(const QString &command)
{
    if (m_macroManager && m_macroManager->recordingMacro()) {
        m_macroManager->recordingMacro()->addAction(MacroAction::fromCommand(command));
    }
}

void Editor::applySyntax(const SyntaxDefinition &definition)
{
    delete m_highlighter;
    m_highlighter = new SyntaxHighlighter(document(), definition);
    if (m_spellChecker)
        m_highlighter->setSpellChecker(m_spellChecker);
    m_highlighter->rehighlight();
}

void Editor::clearSyntax()
{
    delete m_highlighter;
    m_highlighter = nullptr;
}

void Editor::setSpellChecker(SpellChecker *checker)
{
    if (m_spellChecker == checker)
        return;
    m_spellChecker = checker;
    if (m_highlighter)
        m_highlighter->setSpellChecker(checker);
    else if (checker)
        document()->markContentsDirty(0, document()->characterCount());
}

void Editor::setLineNumbersVisible(bool visible)
{
    if (m_lineNumbersVisible == visible)
        return;
    m_lineNumbersVisible = visible;
    m_lineNumberArea->setVisible(visible);
    updateLineNumberAreaWidth();
}

void Editor::setBaseFont(const QFont &font)
{
    setFont(font);
    document()->setDefaultFont(font);
    setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')));
    updateLineNumberAreaWidth();
}

void Editor::refreshTheme()
{
    if (m_highlighter)
        m_highlighter->refreshTheme();
    highlightCurrentLine();
    viewport()->update();
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

void Editor::gotoLine(int lineNumber, int column)
{
    const QTextBlock block = document()->findBlockByNumber(qMax(0, lineNumber - 1));
    if (!block.isValid())
        return;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    const int maxColumn = qMax(0, block.length() - 1);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                        qBound(0, column - 1, maxColumn));
    setTextCursor(cursor);
    centerCursor();
}

int Editor::currentLine() const
{
    return textCursor().blockNumber() + 1;
}

int Editor::currentColumn() const
{
    return textCursor().positionInBlock() + 1;
}

void Editor::duplicateCurrentLine()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    const QString lineText = cursor.block().text();
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText(QStringLiteral("\n") + lineText);
    cursor.endEditBlock();
    setTextCursor(cursor);
    recordMacroCommand("duplicateLine");
}

void Editor::deleteCurrentLine()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    if (!cursor.atEnd())
        cursor.deleteChar();
    cursor.endEditBlock();
    setTextCursor(cursor);
    recordMacroCommand("deleteLine");
}

void Editor::refreshFoldCache()
{
    if (!m_foldCacheDirty)
        return;
    m_foldCacheDirty = false;
    m_foldEnds.clear();

    QVector<QPair<QChar, int>> stack;
    int number = 0;
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next(), ++number) {
        const QString text = block.text();
        for (const QChar ch : text) {
            if (ch == QLatin1Char('{') || ch == QLatin1Char('[') || ch == QLatin1Char('(')) {
                stack.append(qMakePair(ch, number));
            } else if (ch == QLatin1Char('}') || ch == QLatin1Char(']')
                       || ch == QLatin1Char(')')) {
                if (stack.isEmpty())
                    continue;
                const int start = stack.takeLast().second;
                if (start != number)
                    m_foldEnds.insert(start, number);
            }
        }
    }
}

void Editor::applyFolds()
{
    refreshFoldCache();

    const int blockCount = document()->blockCount();
    if (blockCount > 200000)
        return;

    QVector<bool> hidden(blockCount, false);
    bool anyFold = false;

    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next()) {
        const int state = block.userState();
        if (state <= 0 || !(state & BlockState::Folded))
            continue;
        const int end = m_foldEnds.value(block.blockNumber(), -1);
        if (end <= block.blockNumber())
            continue;
        anyFold = true;
        for (int i = block.blockNumber() + 1; i <= end && i < hidden.size(); ++i)
            hidden[i] = true;
    }

    m_foldsActive = anyFold;

    int number = 0;
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next(), ++number) {
        const bool hide = hidden.value(number, false);
        block.setVisible(!hide);
        block.setLineCount(hide ? 0 : 1);
    }

    viewport()->update();
    if (m_lineNumberArea)
        m_lineNumberArea->update();
}

void Editor::toggleFold(int blockNumber)
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;

    refreshFoldCache();
    if (!m_foldEnds.contains(blockNumber))
        return;

    int state = block.userState();
    if (state < 0)
        state = 0;
    block.setUserState(state ^ BlockState::Folded);
    applyFolds();

    if (!textCursor().block().isVisible()) {
        QTextCursor cursor(block);
        setTextCursor(cursor);
        centerCursor();
    }
}

void Editor::toggleFoldAtCursor()
{
    toggleFold(textCursor().blockNumber());
}

void Editor::foldAll()
{
    refreshFoldCache();
    for (auto it = m_foldEnds.constBegin(); it != m_foldEnds.constEnd(); ++it) {
        QTextBlock block = document()->findBlockByNumber(it.key());
        if (!block.isValid())
            continue;
        int state = block.userState();
        if (state < 0)
            state = 0;
        block.setUserState(state | BlockState::Folded);
    }
    applyFolds();
}

void Editor::unfoldAll()
{
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next()) {
        const int state = block.userState();
        if (state > 0 && (state & BlockState::Folded))
            block.setUserState(state & ~BlockState::Folded);
    }
    applyFolds();
}

void Editor::toggleFoldAtGutter(int y)
{
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    while (block.isValid()) {
        const int bottom = top + qRound(blockBoundingRect(block).height());
        if (block.isVisible() && y >= top && y < bottom) {
            toggleFold(block.blockNumber());
            return;
        }
        block = block.next();
        top = bottom;
    }
}

int Editor::lineNumberAreaWidth() const
{
    int digits = 1;
    int maximum = qMax(1, blockCount());
    while (maximum >= 10) {
        maximum /= 10;
        ++digits;
    }
    return 22 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void Editor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    refreshFoldCache();

    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), Theme::colors().gutterBackground);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    painter.setPen(Theme::colors().gutterText);
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (m_foldEnds.value(blockNumber, -1) > blockNumber) {
                const int state = block.userState();
                const bool folded = state > 0 && (state & BlockState::Folded);
                painter.drawText(QRect(2, top, 10, fontMetrics().height()), Qt::AlignCenter,
                                 folded ? QStringLiteral("▶") : QStringLiteral("▼"));
            }
            painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(),
                             Qt::AlignRight, QString::number(blockNumber + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void Editor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect area = contentsRect();
    m_lineNumberArea->setGeometry(QRect(area.left(), area.top(), lineNumberAreaWidth(),
                                       area.height()));
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    // Handle completion widget keys first
    if (isCompletionVisible()) {
        if (handleCompletionKeyPress(event)) {
            return;
        }
    }

    // Record actual text for macro playback. Key codes alone cannot represent
    // lowercase letters, accented characters or layout-specific symbols.
    if (m_macroManager && m_macroManager->recordingMacro()) {
        const QString text = event->text();
        bool printable = !text.isEmpty();
        for (const QChar ch : text) {
            if (!ch.isPrint()) {
                printable = false;
                break;
            }
        }

        if (printable) {
            recordMacroAction(text);
        } else if (!(event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift
                     || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta)) {
            recordMacroKeyPress(event->key(), event->modifiers());
        }
    }

    if (m_blockSelection) {
        if (event->key() == Qt::Key_Escape) {
            clearBlockSelection();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Copy)) {
            copySelection();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Cut)) {
            cutSelection();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Paste)) {
            pasteSelection();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::SelectAll)) {
            clearBlockSelection();
            QPlainTextEdit::selectAll();
            event->accept();
            return;
        }
        if (!isReadOnly()
            && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
            deleteBlockSelection();
            event->accept();
            return;
        }
        if (!isReadOnly() && !event->text().isEmpty()
            && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier
                                       | Qt::MetaModifier))) {
            deleteBlockSelection();
            QPlainTextEdit::keyPressEvent(event);
            return;
        }
        clearBlockSelection();
    }

    const Qt::KeyboardModifiers blockingModifiers =
        Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    if (!isReadOnly() && !(event->modifiers() & blockingModifiers)) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (handleAutoIndent())
                return;
        } else if (event->key() == Qt::Key_Backspace) {
            if (handlePairBackspace())
                return;
        } else if (handleAutoClose(event)) {
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

bool Editor::handleAutoIndent()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        return false;

    const QString blockText = cursor.block().text();
    int indentLength = 0;
    while (indentLength < blockText.size() && blockText.at(indentLength).isSpace())
        ++indentLength;
    const QString indent = blockText.left(indentLength);

    const QString trimmed = blockText.trimmed();
    const QChar last = trimmed.isEmpty() ? QChar() : trimmed.back();
    const bool opensBlock = last == QLatin1Char('{') || last == QLatin1Char('[')
        || last == QLatin1Char('(') || last == QLatin1Char(':');
    const QString extraIndent = opensBlock ? QStringLiteral("    ") : QString();

    const int position = cursor.position();
    const QChar next = document()->characterAt(position);
    const bool closesBlock = next == QLatin1Char('}') || next == QLatin1Char(']')
        || next == QLatin1Char(')');

    cursor.beginEditBlock();
    if (closesBlock) {
        cursor.insertText(QStringLiteral("\n") + indent + extraIndent + QStringLiteral("\n") + indent);
        cursor.movePosition(QTextCursor::Up);
        cursor.movePosition(QTextCursor::EndOfBlock);
        setTextCursor(cursor);
    } else {
        cursor.insertText(QStringLiteral("\n") + indent + extraIndent);
    }
    cursor.endEditBlock();
    return true;
}

bool Editor::handlePairBackspace()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        return false;

    const int position = cursor.position();
    if (position <= 0 || position >= document()->characterCount() - 1)
        return false;

    const QChar previous = document()->characterAt(position - 1);
    const QChar next = document()->characterAt(position);
    const bool isPair = (previous == QLatin1Char('(') && next == QLatin1Char(')'))
        || (previous == QLatin1Char('[') && next == QLatin1Char(']'))
        || (previous == QLatin1Char('{') && next == QLatin1Char('}'))
        || (previous == QLatin1Char('"') && next == QLatin1Char('"'))
        || (previous == QLatin1Char('\'') && next == QLatin1Char('\''));
    if (!isPair)
        return false;

    cursor.beginEditBlock();
    cursor.deletePreviousChar();
    cursor.deleteChar();
    cursor.endEditBlock();
    return true;
}

bool Editor::handleAutoClose(QKeyEvent *event)
{
    const QString text = event->text();
    if (text.size() != 1)
        return false;

    const QChar ch = text.at(0);
    QTextCursor cursor = textCursor();

    // Typing a closing bracket directly before the same character just steps over it.
    if (ch == QLatin1Char(')') || ch == QLatin1Char(']') || ch == QLatin1Char('}')) {
        if (document()->characterAt(cursor.position()) == ch) {
            cursor.movePosition(QTextCursor::NextCharacter);
            setTextCursor(cursor);
            return true;
        }
        return false;
    }

    QChar closing;
    bool isQuote = false;
    switch (ch.unicode()) {
    case '(': closing = QLatin1Char(')'); break;
    case '[': closing = QLatin1Char(']'); break;
    case '{': closing = QLatin1Char('}'); break;
    case '"': closing = QLatin1Char('"'); isQuote = true; break;
    case '\'': closing = QLatin1Char('\''); isQuote = true; break;
    default: return false;
    }

    if (isQuote) {
        const int position = cursor.position();
        const QChar previous = position > 0 ? document()->characterAt(position - 1) : QChar();
        const QChar next = document()->characterAt(position);
        // Avoid interfering with contractions (don't) and text typed before a word.
        if (previous.isLetterOrNumber() || next.isLetterOrNumber())
            return false;
    }

    if (cursor.hasSelection()) {
        const QString selected = cursor.selectedText();
        if (selected.contains(QChar::ParagraphSeparator))
            return false;
        const int length = selected.size();
        cursor.beginEditBlock();
        cursor.insertText(QString(ch) + selected + closing);
        cursor.setPosition(cursor.position() - length - 1);
        cursor.setPosition(cursor.position() + length, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
        cursor.endEditBlock();
        return true;
    }

    cursor.beginEditBlock();
    cursor.insertText(QString(ch) + closing);
    cursor.movePosition(QTextCursor::PreviousCharacter);
    setTextCursor(cursor);
    cursor.endEditBlock();
    return true;
}

int Editor::findMatchingBracket(int position) const
{
    const QChar ch = document()->characterAt(position);
    QChar open;
    QChar close;
    bool forward = true;
    switch (ch.unicode()) {
    case '(': open = QLatin1Char('('); close = QLatin1Char(')'); break;
    case '[': open = QLatin1Char('['); close = QLatin1Char(']'); break;
    case '{': open = QLatin1Char('{'); close = QLatin1Char('}'); break;
    case ')': open = QLatin1Char('('); close = QLatin1Char(')'); forward = false; break;
    case ']': open = QLatin1Char('['); close = QLatin1Char(']'); forward = false; break;
    case '}': open = QLatin1Char('{'); close = QLatin1Char('}'); forward = false; break;
    default: return -1;
    }

    const int count = document()->characterCount();
    int depth = 0;
    const int limit = 200000;
    int steps = 0;
    for (int i = position; i >= 0 && i < count && steps < limit; i += forward ? 1 : -1, ++steps) {
        const QChar current = document()->characterAt(i);
        if (current == open) {
            if (!forward) {
                --depth;
                if (depth == 0)
                    return i;
            } else {
                ++depth;
            }
        } else if (current == close) {
            if (forward) {
                --depth;
                if (depth == 0)
                    return i;
            } else {
                ++depth;
            }
        }
    }
    return -1;
}

void Editor::addBracketMatchSelections(QList<QTextEdit::ExtraSelection> &selections)
{
    const QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        return;

    auto isBracket = [](QChar c) {
        return c == QLatin1Char('(') || c == QLatin1Char(')') || c == QLatin1Char('[')
            || c == QLatin1Char(']') || c == QLatin1Char('{') || c == QLatin1Char('}');
    };

    const int position = cursor.position();
    int bracketPosition = -1;
    if (position < document()->characterCount() && isBracket(document()->characterAt(position)))
        bracketPosition = position;
    else if (position > 0 && isBracket(document()->characterAt(position - 1)))
        bracketPosition = position - 1;
    if (bracketPosition < 0)
        return;

    const int match = findMatchingBracket(bracketPosition);
    if (match < 0)
        return;

    QTextCharFormat format;
    format.setBackground(Theme::colors().bracketMatch);
    format.setFontWeight(QFont::Bold);

    for (int location : {bracketPosition, match}) {
        QTextEdit::ExtraSelection selection;
        selection.format = format;
        selection.cursor = QTextCursor(document());
        selection.cursor.setPosition(location);
        selection.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        selections.append(selection);
    }
}

void Editor::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_macroManager && m_macroManager->recordingMacro()
        && !event->commitString().isEmpty()) {
        recordMacroAction(event->commitString());
    }
    QPlainTextEdit::inputMethodEvent(event);
}

void Editor::insertFromMimeData(const QMimeData *source)
{
    if (m_blockSelection)
        deleteBlockSelection();
    if (source->hasText()) {
        recordMacroAction(source->text());
    }
    QPlainTextEdit::insertFromMimeData(source);
}

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    const bool hasBlock = m_blockSelection;
    const bool hasLinear = textCursor().hasSelection();
    const bool hasSelection = hasBlock || hasLinear;

    QTextCursor wordCursor = cursorForPosition(event->pos());
    wordCursor.select(QTextCursor::WordUnderCursor);
    const QString word = wordCursor.selectedText();

    if (m_spellChecker && m_spellChecker->isLoaded() && word.size() >= 2
        && !m_spellChecker->isCorrect(word)) {
        auto *spellingMenu = menu.addMenu(tr("Spelling"));
        const QStringList suggestions = m_spellChecker->suggestions(word, 6);
        if (suggestions.isEmpty()) {
            QAction *none = spellingMenu->addAction(tr("(no suggestions)"));
            none->setEnabled(false);
        } else {
            for (const QString &suggestion : suggestions) {
                QAction *action = spellingMenu->addAction(suggestion);
                connect(action, &QAction::triggered, this, [this, wordCursor, suggestion]() {
                    QTextCursor replacement = wordCursor;
                    replacement.insertText(suggestion);
                    setTextCursor(replacement);
                });
            }
        }
        spellingMenu->addSeparator();
        QAction *addAction = spellingMenu->addAction(tr("Add to dictionary"));
        connect(addAction, &QAction::triggered, this, [this, word]() {
            if (m_spellChecker)
                m_spellChecker->addToDictionary(word);
        });
        menu.addSeparator();
    }

    QAction *undoAction = menu.addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(!isReadOnly() && document()->isUndoAvailable());
    connect(undoAction, &QAction::triggered, this, &QPlainTextEdit::undo);

    QAction *redoAction = menu.addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(!isReadOnly() && document()->isRedoAvailable());
    connect(redoAction, &QAction::triggered, this, &QPlainTextEdit::redo);

    menu.addSeparator();

    QAction *cutAction = menu.addAction(tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    cutAction->setEnabled(!isReadOnly() && hasSelection);
    connect(cutAction, &QAction::triggered, this, &Editor::cutSelection);

    QAction *copyAction = menu.addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(hasSelection);
    connect(copyAction, &QAction::triggered, this, &Editor::copySelection);

    QAction *pasteAction = menu.addAction(tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setEnabled(!isReadOnly() && canPaste());
    connect(pasteAction, &QAction::triggered, this, &Editor::pasteSelection);

    QAction *deleteAction = menu.addAction(tr("&Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setEnabled(!isReadOnly() && hasSelection);
    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (m_blockSelection) {
            deleteBlockSelection();
        } else {
            QTextCursor cursor = textCursor();
            cursor.removeSelectedText();
            setTextCursor(cursor);
        }
    });

    menu.addSeparator();

    QAction *selectAllAction = menu.addAction(tr("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, &Editor::selectAll);

    menu.addSeparator();

    QAction *duplicateAction = menu.addAction(tr("D&uplicate Line"));
    duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateAction->setEnabled(!isReadOnly());
    connect(duplicateAction, &QAction::triggered, this, &Editor::duplicateCurrentLine);

    QAction *deleteLineAction = menu.addAction(tr("Delete &Line"));
    deleteLineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    deleteLineAction->setEnabled(!isReadOnly());
    connect(deleteLineAction, &QAction::triggered, this, &Editor::deleteCurrentLine);

    QAction *foldAction = menu.addAction(tr("Toggle &Fold"));
    connect(foldAction, &QAction::triggered, this, &Editor::toggleFoldAtCursor);

    menu.addSeparator();

    QAction *columnModeAction = menu.addAction(tr("&Column Selection Mode"));
    columnModeAction->setCheckable(true);
    columnModeAction->setChecked(m_blockSelectMode);
    connect(columnModeAction, &QAction::toggled, this, &Editor::setBlockSelectionMode);

    QAction *findAction = menu.addAction(tr("&Find / Replace..."));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &Editor::findRequested);

    QAction *gotoAction = menu.addAction(tr("&Go to Line..."));
    gotoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(gotoAction, &QAction::triggered, this, &Editor::gotoLineRequested);

    menu.exec(event->globalPos());
}

void Editor::setBlockSelectionMode(bool enabled)
{
    if (m_blockSelectMode == enabled)
        return;
    m_blockSelectMode = enabled;
    if (!enabled)
        m_blockSelecting = false;
}

Editor::BlockPoint Editor::blockPointAt(const QPoint &position) const
{
    BlockPoint point;
    const QTextCursor cursor = cursorForPosition(position);
    const QTextBlock block = cursor.block();
    if (!block.isValid())
        return point;

    point.block = block.blockNumber();
    point.column = cursor.positionInBlock();

    QTextLayout *layout = block.layout();
    if (layout && layout->lineCount() > 0) {
        QTextCursor lineStart(document());
        lineStart.setPosition(block.position());
        const QTextLine line = layout->lineAt(0);
        const qreal x = position.x() - cursorRect(lineStart).x() - line.x();
        point.column = line.xToCursor(x);
    }

    point.column = qBound(0, point.column, block.length());
    return point;
}

QTextCursor Editor::cursorForBlockPoint(const BlockPoint &point) const
{
    QTextCursor cursor(document());
    const QTextBlock block = document()->findBlockByNumber(point.block);
    if (!block.isValid())
        return cursor;
    cursor.setPosition(block.position() + qBound(0, point.column, block.length()));
    return cursor;
}

int Editor::blockSelectionTop() const
{
    return qMin(m_blockAnchor.block, m_blockCursor.block);
}

int Editor::blockSelectionBottom() const
{
    return qMax(m_blockAnchor.block, m_blockCursor.block);
}

int Editor::blockSelectionLeft() const
{
    return qMin(m_blockAnchor.column, m_blockCursor.column);
}

int Editor::blockSelectionRight() const
{
    return qMax(m_blockAnchor.column, m_blockCursor.column);
}

QList<QRect> Editor::blockSelectionRects() const
{
    QList<QRect> rectangles;
    if (!m_blockSelection || !m_blockAnchor.isValid() || !m_blockCursor.isValid())
        return rectangles;

    const int left = blockSelectionLeft();
    const int right = blockSelectionRight();
    for (int number = blockSelectionTop(); number <= blockSelectionBottom(); ++number) {
        const QTextBlock block = document()->findBlockByNumber(number);
        if (!block.isValid() || !block.isVisible())
            continue;

        const int start = qMin(left, block.length());
        const int end = qMin(right, block.length());

        QTextCursor startCursor(document());
        startCursor.setPosition(block.position() + start);
        QTextCursor endCursor(document());
        endCursor.setPosition(block.position() + end);

        const QRect startRect = cursorRect(startCursor);
        const QRect endRect = cursorRect(endCursor);
        const int x1 = qMin(startRect.x(), endRect.x());
        const int x2 = qMax(startRect.x(), endRect.x());
        if (x2 <= x1 || startRect.height() <= 0)
            continue;
        rectangles.append(QRect(QPoint(x1, startRect.y()), QSize(x2 - x1, startRect.height())));
    }
    return rectangles;
}

QString Editor::blockSelectionText() const
{
    if (!m_blockSelection || !m_blockAnchor.isValid())
        return QString();

    const int left = blockSelectionLeft();
    const int right = blockSelectionRight();
    const int width = right - left;
    if (width <= 0)
        return QString();

    QStringList lines;
    for (int number = blockSelectionTop(); number <= blockSelectionBottom(); ++number) {
        const QTextBlock block = document()->findBlockByNumber(number);
        if (!block.isValid())
            continue;
        const QString text = block.text();
        const int start = qMin(left, text.size());
        const int end = qMin(right, text.size());
        QString piece = text.mid(start, qMax(0, end - start));
        if (piece.size() < width)
            piece += QString(width - piece.size(), QLatin1Char(' '));
        lines.append(piece);
    }
    return lines.join(QLatin1Char('\n'));
}

void Editor::clearBlockSelection()
{
    if (!m_blockSelection && !m_blockSelecting)
        return;
    m_blockSelection = false;
    m_blockSelecting = false;
    viewport()->update();
    emit blockSelectionChanged();
}

void Editor::copySelection()
{
    if (m_blockSelection) {
        const QString text = blockSelectionText();
        if (!text.isEmpty())
            QApplication::clipboard()->setText(text);
        return;
    }
    QPlainTextEdit::copy();
}

void Editor::cutSelection()
{
    if (!m_blockSelection) {
        QPlainTextEdit::cut();
        return;
    }
    if (isReadOnly())
        return;

    const QString text = blockSelectionText();
    if (!text.isEmpty())
        QApplication::clipboard()->setText(text);
    deleteBlockSelection();
}

void Editor::pasteSelection()
{
    if (!m_blockSelection) {
        QPlainTextEdit::paste();
        return;
    }
    if (isReadOnly())
        return;

    deleteBlockSelection();
    QPlainTextEdit::paste();
}

void Editor::deleteBlockSelection()
{
    if (!m_blockSelection || isReadOnly() || !m_blockAnchor.isValid())
        return;

    const int top = blockSelectionTop();
    const int left = blockSelectionLeft();
    const int right = blockSelectionRight();

    QTextCursor editorCursor(document());
    editorCursor.beginEditBlock();
    for (int number = blockSelectionBottom(); number >= top; --number) {
        const QTextBlock block = document()->findBlockByNumber(number);
        if (!block.isValid() || !block.isVisible())
            continue;
        const int start = qMin(left, block.length());
        const int end = qMin(right, block.length());
        if (end <= start)
            continue;

        editorCursor.setPosition(block.position() + start);
        editorCursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
        editorCursor.removeSelectedText();
    }
    editorCursor.endEditBlock();

    clearBlockSelection();
    setTextCursor(cursorForBlockPoint({top, left}));
    ensureCursorVisible();
}

void Editor::selectAll()
{
    clearBlockSelection();
    QPlainTextEdit::selectAll();
}

void Editor::selectBlock(int anchorBlock, int anchorColumn, int currentBlock, int currentColumn)
{
    const QTextBlock anchor = document()->findBlockByNumber(anchorBlock);
    const QTextBlock current = document()->findBlockByNumber(currentBlock);
    if (!anchor.isValid() || !current.isValid())
        return;

    m_blockAnchor = {anchorBlock, qBound(0, anchorColumn, anchor.length())};
    m_blockCursor = {currentBlock, qBound(0, currentColumn, current.length())};
    m_blockSelection = true;
    m_blockSelecting = false;
    setTextCursor(cursorForBlockPoint(m_blockCursor));
    viewport()->update();
    emit blockSelectionChanged();
}

void Editor::updateBlockSelection(const QPoint &position)
{
    const BlockPoint point = blockPointAt(position);
    if (point == m_blockCursor)
        return;
    m_blockCursor = point;
    setTextCursor(cursorForBlockPoint(point));
    ensureCursorVisible();
    viewport()->update();
}

void Editor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && ((event->modifiers() & Qt::AltModifier) || m_blockSelectMode)) {
        setFocus(Qt::MouseFocusReason);
        m_blockAnchor = blockPointAt(event->position().toPoint());
        m_blockCursor = m_blockAnchor;
        m_blockSelection = true;
        m_blockSelecting = true;
        setTextCursor(cursorForBlockPoint(m_blockCursor));
        viewport()->update();
        emit blockSelectionChanged();
        event->accept();
        return;
    }

    if (m_blockSelection)
        clearBlockSelection();
    QPlainTextEdit::mousePressEvent(event);
}

void Editor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_blockSelecting) {
        updateBlockSelection(event->position().toPoint());
        event->accept();
        return;
    }
    QPlainTextEdit::mouseMoveEvent(event);
}

void Editor::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_blockSelecting && event->button() == Qt::LeftButton) {
        updateBlockSelection(event->position().toPoint());
        m_blockSelecting = false;
        emit blockSelectionChanged();
        event->accept();
        return;
    }
    QPlainTextEdit::mouseReleaseEvent(event);
}

void Editor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);

    if (!m_blockSelection)
        return;

    QPainter painter(viewport());
    QColor color = palette().color(QPalette::Highlight);
    color.setAlpha(80);
    for (const QRect &rectangle : blockSelectionRects())
        painter.fillRect(rectangle, color);
}

void Editor::updateLineNumberAreaWidth()
{
    setViewportMargins(m_lineNumbersVisible ? lineNumberAreaWidth() : 0, 0, 0, 0);
}

void Editor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (!m_lineNumbersVisible)
        return;
    if (dy != 0)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void Editor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(Theme::colors().currentLine);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    addBracketMatchSelections(selections);
    setExtraSelections(selections);
}

// ============================================================================
// LSP Integration
// ============================================================================

void Editor::setDocumentPath(const QString& path)
{
    m_documentPath = path;
    if (m_lspEnabled) {
        m_lspDocumentUri = QUrl::fromLocalFile(path).toString();
    }
}

void Editor::setLSPClient(LSPClient *client, const QString &languageId)
{
    if (m_lspEnabled) {
        teardownLSP();
    }
    
    m_lspClient = client;
    m_lspLanguageId = languageId;
    m_lspDocumentVersion = 0;
    m_lspDocumentUri = QUrl::fromLocalFile(m_documentPath).toString();
    
    if (m_lspClient && m_lspClient->isInitialized()) {
        setupLSP();
    } else if (m_lspClient) {
        connect(m_lspClient, &LSPClient::stateChanged, this, [this](LSPClient::State state) {
            if (state == LSPClient::State::Ready) {
                setupLSP();
            }
        });
    }
}

void Editor::disableLSP()
{
    if (!m_lspEnabled)
        return;
    
    sendDidClose();
    teardownLSP();
}

void Editor::setupLSP()
{
    if (!m_lspClient || !m_lspClient->isInitialized() || m_lspEnabled)
        return;
    
    // Connect LSP signals
    connect(m_lspClient, &LSPClient::diagnosticsReceived,
            this, &Editor::onLSPDiagnosticsReceived);
    
    m_lspEnabled = true;
    sendDidOpen();
}

void Editor::teardownLSP()
{
    if (!m_lspEnabled)
        return;
    
    if (m_lspClient) {
        disconnect(m_lspClient, &LSPClient::diagnosticsReceived,
                   this, &Editor::onLSPDiagnosticsReceived);
    }
    
    m_didChangeTimer.stop();
    m_pendingChanges.clear();
    
    m_lspEnabled = false;
    m_lspClient = nullptr;
}

void Editor::sendDidOpen()
{
    if (!m_lspClient || !m_lspEnabled)
        return;
    
    LSP::TextDocumentItem item;
    item.uri = m_lspDocumentUri;
    item.languageId = m_lspLanguageId;
    item.version = m_lspDocumentVersion;
    item.text = toPlainText();
    
    m_lspClient->didOpen(item);
}

void Editor::sendDidChange()
{
    if (!m_lspClient || !m_lspEnabled || m_pendingChanges.isEmpty())
        return;
    
    LSP::VersionedTextDocumentIdentifier doc;
    doc.uri = m_lspDocumentUri;
    doc.version = ++m_lspDocumentVersion;
    
    m_lspClient->didChange(doc, m_pendingChanges);
    m_pendingChanges.clear();
}

void Editor::sendDidSave()
{
    if (!m_lspClient || !m_lspEnabled)
        return;
    
    LSP::TextDocumentIdentifier doc;
    doc.uri = m_lspDocumentUri;
    
    m_lspClient->didSave(doc, toPlainText());
}

void Editor::sendDidClose()
{
    if (!m_lspClient)
        return;
    
    LSP::TextDocumentIdentifier doc;
    doc.uri = m_lspDocumentUri;
    
    m_lspClient->didClose(doc);
}

void Editor::scheduleDidChange()
{
    // Capture the current change
    // Note: This is a simplified version - in reality we'd need to track what actually changed
    // For now, we send full document content on each change (less efficient but works)
    LSP::TextDocumentContentChangeEvent change;
    change.text = toPlainText();
    // No range = full document replacement
    
    m_pendingChanges.append(change);
    m_didChangeTimer.start();
}

void Editor::triggerGoToDefinition()
{
    if (!m_lspEnabled || !m_lspClient)
        return;
    
    LSP::TextDocumentPositionParams params = makePositionParams();
    emit lspDefinitionRequested(params);
    
    m_lspClient->requestDefinition(params, [this](const QVector<LSP::Location>& locations) {
        onLSPDefinitionResponse(locations);
    });
}

void Editor::triggerFindReferences()
{
    if (!m_lspEnabled || !m_lspClient)
        return;
    
    LSP::ReferenceParams params = makeReferenceParams();
    emit lspReferencesRequested(params);
    
    m_lspClient->requestReferences(params, [this](const QVector<LSP::Location>& locations) {
        onLSPReferencesResponse(locations);
    });
}

void Editor::triggerRename()
{
    if (!m_lspEnabled || !m_lspClient)
        return;
    
    // For now, use selected text or word under cursor
    QString currentName = textCursor().selectedText();
    if (currentName.isEmpty()) {
        // Get word under cursor
        QTextCursor cursor = textCursor();
        cursor.select(QTextCursor::WordUnderCursor);
        currentName = cursor.selectedText();
    }
    if (currentName.isEmpty())
        return;
    
    // For now, we'll show a simple dialog to get the new name
    // In a real implementation, this would show an inline rename UI
    bool ok = false;
    QString newName = QInputDialog::getText(this, tr("Rename Symbol"), 
                                            tr("New name:"), QLineEdit::Normal,
                                            currentName,
                                            &ok);
    if (!ok || newName.isEmpty() || newName == currentName)
        return;
    
    LSP::RenameParams params = makeRenameParams(newName);
    emit lspRenameRequested(params);
    
    m_lspClient->requestRename(params, [this](const std::optional<LSP::WorkspaceEdit>& edit) {
        onLSPRenameResponse(edit);
    });
}

LSP::TextDocumentPositionParams Editor::makePositionParams() const
{
    LSP::TextDocumentPositionParams params;
    params.textDocument.uri = m_lspDocumentUri;
    QTextCursor cursor = textCursor();
    params.position.line = cursor.blockNumber();
    params.position.character = cursor.positionInBlock();
    return params;
}

LSP::ReferenceParams Editor::makeReferenceParams() const
{
    LSP::ReferenceParams params;
    params.textDocument.uri = m_lspDocumentUri;
    QTextCursor cursor = textCursor();
    params.position.line = cursor.blockNumber();
    params.position.character = cursor.positionInBlock();
    params.context.includeDeclaration = true;
    return params;
}

LSP::RenameParams Editor::makeRenameParams(const QString& newName) const
{
    LSP::RenameParams params;
    params.textDocument.uri = m_lspDocumentUri;
    QTextCursor cursor = textCursor();
    params.position.line = cursor.blockNumber();
    params.position.character = cursor.positionInBlock();
    params.newName = newName;
    return params;
}

void Editor::applyWorkspaceEdit(const LSP::WorkspaceEdit& edit)
{
    if (!edit.changes.isEmpty()) {
        // Group edits by document
        for (auto it = edit.changes.begin(); it != edit.changes.end(); ++it) {
            const QString& uri = it.key();
            const QVector<LSP::TextEdit>& edits = it.value();
            
            if (uri == m_lspDocumentUri) {
                // Apply edits to current document
                QTextCursor cursor(document());
                cursor.beginEditBlock();
                
                // Sort edits by position (reverse order to maintain positions)
                QVector<LSP::TextEdit> sortedEdits = edits;
                std::sort(sortedEdits.begin(), sortedEdits.end(), [](const LSP::TextEdit& a, const LSP::TextEdit& b) {
                    if (a.range.start.line != b.range.start.line)
                        return a.range.start.line > b.range.start.line;
                    return a.range.start.character > b.range.start.character;
                });
                
                for (const LSP::TextEdit& textEdit : sortedEdits) {
                    const int startPos = document()->findBlockByNumber(textEdit.range.start.line).position() 
                                        + textEdit.range.start.character;
                    const int endPos = document()->findBlockByNumber(textEdit.range.end.line).position() 
                                      + textEdit.range.end.character;
                    
                    cursor.setPosition(startPos);
                    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
                    cursor.insertText(textEdit.newText);
                }
                
                cursor.endEditBlock();
            } else {
                // Different document - emit signal for MainWindow to handle
                // TODO: emit signal to open file
            }
        }
    }
}

void Editor::onLSPDefinitionResponse(const QVector<LSP::Location>& locations)
{
    if (locations.isEmpty())
        return;
    
    // Navigate to first location
    const LSP::Location& loc = locations.first();
    if (loc.uri == m_lspDocumentUri) {
        // Same document - navigate locally
        QTextCursor cursor(document());
        const QTextBlock block = document()->findBlockByNumber(loc.range.start.line);
        if (block.isValid()) {
            cursor.setPosition(block.position() + loc.range.start.character);
            setTextCursor(cursor);
            centerCursor();
        }
    } else {
        // Different document - emit signal for MainWindow to handle
        // TODO: emit signal to open file
    }
}

void Editor::onLSPReferencesResponse(const QVector<LSP::Location>& locations)
{
    // TODO: Show references in a panel
    Q_UNUSED(locations);
}

void Editor::onLSPRenameResponse(const std::optional<LSP::WorkspaceEdit>& edit)
{
    if (edit) {
        applyWorkspaceEdit(*edit);
    }
}

void Editor::onLSPPrepareRenameResponse(const std::optional<LSP::Range>& range)
{
    Q_UNUSED(range);
    // TODO: Show inline rename UI
}

void Editor::onLSPDiagnosticsReceived(const QString& uri, const QVector<LSP::Diagnostic>& diagnostics)
{
    if (uri != m_lspDocumentUri)
        return;
    
    if (m_highlighter) {
        m_highlighter->setLSPDiagnostics(diagnostics);
    }
    emit lspDiagnosticsChanged();
}

// ============================================================================
// LSP Completion
// ============================================================================

void Editor::triggerCompletion()
{
    if (!m_lspEnabled || !m_lspClient)
        return;

    LSP::TextDocumentPositionParams params = makePositionParams();
    LSP::CompletionParams completionParams;
    completionParams.textDocument = params.textDocument;
    completionParams.position = params.position;
    
    // Set trigger kind - manually invoked
    LSP::CompletionContext context;
    context.triggerKind = LSP::CompletionContext::TriggerKind::Invoked;
    completionParams.context = context;

    m_completionTriggeredManually = true;
    
    m_lspClient->requestCompletion(params, [this](const QVector<LSP::CompletionItem>& items) {
        onLSPCompletionResponse(items);
    });
}

void Editor::onLSPCompletionResponse(const QVector<LSP::CompletionItem>& items)
{
    if (items.isEmpty()) {
        hideCompletionWidget();
        return;
    }
    
    showCompletionWidget(items);
}

void Editor::onLSPCompletionCancelled()
{
    hideCompletionWidget();
}

void Editor::showCompletionWidget(const QVector<LSP::CompletionItem>& items)
{
    if (!m_completionWidget) {
        m_completionWidget = new LSPCompletionWidget(this, this);
        connect(m_completionWidget, &LSPCompletionWidget::completionAccepted,
                this, [this](const LSP::CompletionItem& item) {
            applyCompletionItem(item);
        });
        connect(m_completionWidget, &LSPCompletionWidget::completionCancelled,
                this, &Editor::hideCompletionWidget);
    }

    // Position at cursor
    QRect cursorRect = this->cursorRect();
    QPoint position = cursorRect.bottomLeft();
    
    m_completionWidget->showCompletions(items, position);
}

void Editor::hideCompletionWidget()
{
    if (m_completionWidget && m_completionWidget->isVisible()) {
        m_completionWidget->hideCompletions();
    }
    m_completionTriggeredManually = false;
}

bool Editor::isCompletionVisible() const
{
    return m_completionWidget && m_completionWidget->isVisible();
}

bool Editor::handleCompletionKeyPress(QKeyEvent* event)
{
    if (!m_completionWidget || !m_completionWidget->isVisible())
        return false;

    switch (event->key()) {
        case Qt::Key_Down:
            if (event->modifiers() == Qt::NoModifier) {
                m_completionWidget->selectNext();
                return true;
            }
            break;
        case Qt::Key_Up:
            if (event->modifiers() == Qt::NoModifier) {
                m_completionWidget->selectPrevious();
                return true;
            }
            break;
        case Qt::Key_PageUp:
            if (event->modifiers() == Qt::NoModifier) {
                m_completionWidget->selectPageUp();
                return true;
            }
            break;
        case Qt::Key_PageDown:
            if (event->modifiers() == Qt::NoModifier) {
                m_completionWidget->selectPageDown();
                return true;
            }
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (event->modifiers() == Qt::NoModifier) {
                m_completionWidget->acceptCompletion();
                return true;
            }
            break;
        case Qt::Key_Escape:
            if (event->modifiers() == Qt::NoModifier) {
                m_completionWidget->cancelCompletion();
                return true;
            }
            break;
        default:
            // Pass through printable characters to update filter
            if (!event->text().isEmpty() && 
                !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
                QString newPrefix = completionPrefix() + event->text();
                m_completionWidget->setFilter(newPrefix);
                // Don't consume the event - let it be processed for text input
                return false;
            }
            // Backspace should also update filter
            if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier) {
                QString prefix = completionPrefix();
                if (!prefix.isEmpty()) {
                    prefix.chop(1);
                    m_completionWidget->setFilter(prefix);
                }
                return false;
            }
            break;
    }
    return false;
}

QString Editor::completionPrefix() const
{
    QTextCursor cursor = textCursor();
    int position = cursor.positionInBlock();
    QString blockText = cursor.block().text();
    
    // Find the word prefix before cursor
    int start = position;
    while (start > 0 && (blockText.at(start - 1).isLetterOrNumber() || blockText.at(start - 1) == QLatin1Char('_')))
        --start;
    
    return blockText.mid(start, position - start);
}

void Editor::applyCompletionItem(const LSP::CompletionItem& item)
{
    QString insertText = item.insertText.value_or(item.label);
    
    // Calculate the prefix to replace
    QString prefix = completionPrefix();
    if (!prefix.isEmpty()) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefix.length());
        cursor.insertText(insertText);
        setTextCursor(cursor);
    } else {
        // No prefix, just insert
        textCursor().insertText(insertText);
    }
}

void Editor::focusOutEvent(QFocusEvent *event)
{
    // Hide completion widget when editor loses focus
    if (m_completionWidget && m_completionWidget->isVisible()) {
        // Delay to allow click on completion item
        QTimer::singleShot(100, this, [this]() {
            if (m_completionWidget && m_completionWidget->isVisible() && !hasFocus()) {
                m_completionWidget->cancelCompletion();
            }
        });
    }
    QPlainTextEdit::focusOutEvent(event);
}

// ============================================================================
// Macro execution (static helper called from MacroManager)
// ============================================================================

bool executeMacroAction(const MacroAction &action, QPlainTextEdit *editor)
{
    if (!editor)
        return false;

    switch (action.type) {
    case MacroAction::TextInput:
        if (!action.text.isEmpty())
            editor->insertPlainText(action.text);
        return true;

    case MacroAction::KeyPress: {
        // Simulate key press via QKeyEvent - we'll use editor's keyPressEvent path
        // For simplicity, map common keys to editor actions
        switch (action.key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            editor->insertPlainText("\n");
            return true;
        case Qt::Key_Tab:
            editor->insertPlainText("\t");
            return true;
        case Qt::Key_Backspace:
            editor->textCursor().deletePreviousChar();
            return true;
        case Qt::Key_Delete:
            editor->textCursor().deleteChar();
            return true;
        case Qt::Key_Left:
            editor->moveCursor(QTextCursor::Left, action.modifiers & Qt::ShiftModifier
                                             ? QTextCursor::KeepAnchor
                                             : QTextCursor::MoveAnchor);
            return true;
        case Qt::Key_Right:
            editor->moveCursor(QTextCursor::Right, action.modifiers & Qt::ShiftModifier
                                              ? QTextCursor::KeepAnchor
                                              : QTextCursor::MoveAnchor);
            return true;
        case Qt::Key_Up:
            editor->moveCursor(QTextCursor::Up, action.modifiers & Qt::ShiftModifier
                                            ? QTextCursor::KeepAnchor
                                            : QTextCursor::MoveAnchor);
            return true;
        case Qt::Key_Down:
            editor->moveCursor(QTextCursor::Down, action.modifiers & Qt::ShiftModifier
                                             ? QTextCursor::KeepAnchor
                                             : QTextCursor::MoveAnchor);
            return true;
        case Qt::Key_Home:
            editor->moveCursor(QTextCursor::StartOfLine, action.modifiers & Qt::ShiftModifier
                                                      ? QTextCursor::KeepAnchor
                                                      : QTextCursor::MoveAnchor);
            return true;
        case Qt::Key_End:
            editor->moveCursor(QTextCursor::EndOfLine, action.modifiers & Qt::ShiftModifier
                                                    ? QTextCursor::KeepAnchor
                                                    : QTextCursor::MoveAnchor);
            return true;
        case Qt::Key_PageUp:
            editor->moveCursor(QTextCursor::Up, action.modifiers & Qt::ShiftModifier
                                           ? QTextCursor::KeepAnchor
                                           : QTextCursor::MoveAnchor);
            // repeat for page size
            return true;
        case Qt::Key_PageDown:
            editor->moveCursor(QTextCursor::Down, action.modifiers & Qt::ShiftModifier
                                               ? QTextCursor::KeepAnchor
                                               : QTextCursor::MoveAnchor);
            return true;
        default:
            // If it's a printable character with no modifiers (or just Shift), treat as text
            if (action.key >= Qt::Key_Space && action.key <= Qt::Key_AsciiTilde
                && (action.modifiers == Qt::NoModifier || action.modifiers == Qt::ShiftModifier)) {
                QChar ch(static_cast<ushort>(action.key));
                if (action.key >= Qt::Key_A && action.key <= Qt::Key_Z)
                    ch = (action.modifiers & Qt::ShiftModifier) ? ch.toUpper() : ch.toLower();
                editor->insertPlainText(ch);
                return true;
            }
            return false;
        }
    }

    case MacroAction::EditorCommand: {
        const QString &cmd = action.command;
        if (cmd == "cut")
            editor->cut();
        else if (cmd == "copy")
            editor->copy();
        else if (cmd == "paste")
            editor->paste();
        else if (cmd == "undo")
            editor->undo();
        else if (cmd == "redo")
            editor->redo();
        else if (cmd == "selectAll")
            editor->selectAll();
        else if (cmd == "duplicateLine") {
            // Duplicate current line - need to use Editor's method
            if (auto *e = qobject_cast<Editor *>(editor))
                e->duplicateCurrentLine();
        }
        else if (cmd == "deleteLine") {
            if (auto *e = qobject_cast<Editor *>(editor))
                e->deleteCurrentLine();
        }
        else if (cmd == "findNext")
            ; // handled at higher level
        else if (cmd == "findPrevious")
            ;
        else if (cmd == "replace")
            ;
        else
            return false;
        return true;
    }

    case MacroAction::CursorMovement:
        return false;
    }
    return false;
}

} // namespace papiro
