#include "editor/Editor.h"

#include "core/Macro.h"
#include "core/SyntaxDefinition.h"
#include "core/SpellChecker.h"
#include "editor/SyntaxHighlighter.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

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
        if (m_foldsActive)
            applyFolds();
    });

    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

Editor::~Editor() = default;

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

void Editor::gotoLine(int lineNumber)
{
    const QTextBlock block = document()->findBlockByNumber(qMax(0, lineNumber - 1));
    if (!block.isValid())
        return;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
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
        if (state <= 0 || !(state & FoldedBlockState))
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
    block.setUserState(state ^ FoldedBlockState);
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
        block.setUserState(state | FoldedBlockState);
    }
    applyFolds();
}

void Editor::unfoldAll()
{
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next()) {
        const int state = block.userState();
        if (state > 0 && (state & FoldedBlockState))
            block.setUserState(state & ~FoldedBlockState);
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
    painter.fillRect(event->rect(), QColor(0xF2, 0xF2, 0xF2));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    painter.setPen(QColor(0x8A, 0x8A, 0x8A));
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (m_foldEnds.value(blockNumber, -1) > blockNumber) {
                const int state = block.userState();
                const bool folded = state > 0 && (state & FoldedBlockState);
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
    // Record key press for macro
    if (m_macroManager && m_macroManager->recordingMacro()) {
        // Only record "interesting" keys, not every modifier-only press
        if (!(event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift
              || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta)) {
            recordMacroKeyPress(event->key(), event->modifiers());
        }
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
    format.setBackground(QColor(0xDF, 0xEB, 0xFF));
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

void Editor::insertFromMimeData(const QMimeData *source)
{
    if (source->hasText()) {
        recordMacroAction(source->text());
    }
    QPlainTextEdit::insertFromMimeData(source);
}

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();

    QTextCursor cursor = cursorForPosition(event->pos());
    cursor.select(QTextCursor::WordUnderCursor);
    const QString word = cursor.selectedText();

    if (m_spellChecker && m_spellChecker->isLoaded() && word.size() >= 2
        && !m_spellChecker->isCorrect(word)) {
        auto *spellingMenu = new QMenu(tr("Spelling"), menu);
        const QStringList suggestions = m_spellChecker->suggestions(word, 6);
        if (suggestions.isEmpty()) {
            QAction *none = spellingMenu->addAction(tr("(no suggestions)"));
            none->setEnabled(false);
        } else {
            for (const QString &suggestion : suggestions) {
                QAction *action = spellingMenu->addAction(suggestion);
                connect(action, &QAction::triggered, this, [this, cursor, suggestion]() {
                    QTextCursor replacement = cursor;
                    replacement.insertText(suggestion);
                    setTextCursor(replacement);
                });
            }
        }
        spellingMenu->addSeparator();
        QAction *addAction = spellingMenu->addAction(tr("Add to dictionary"));
        connect(addAction, &QAction::triggered, this, [this, word]() {
            m_spellChecker->addToDictionary(word);
        });

        if (!menu->actions().isEmpty())
            menu->insertMenu(menu->actions().first(), spellingMenu);
        else
            menu->addMenu(spellingMenu);
    }

    menu->exec(event->globalPos());
    delete menu;
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
        selection.format.setBackground(QColor(0xF5, 0xF7, 0xFA));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    addBracketMatchSelections(selections);
    setExtraSelections(selections);
}

// ---------------------------------------------------------------------------
// Macro execution (static helper called from MacroManager)
// ---------------------------------------------------------------------------

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
                if (action.modifiers == Qt::ShiftModifier)
                    ch = ch.toUpper();
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
