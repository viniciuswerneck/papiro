#include "core/Macro.h"
#include "editor/BlockState.h"
#include "editor/Editor.h"
#include "editor/SyntaxHighlighter.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QContextMenuEvent>
#include <QInputMethodEvent>
#include <QMenu>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>
#include <QtTest>

using namespace papiro;

class TestEditor : public QObject {
    Q_OBJECT

private slots:
    void init();
    void autoClosesPairs();
    void stepsOverClosingCharacter();
    void wrapsSelection();
    void pairBackspace();
    void autoIndentInsideBlock();
    void gotoLineAndColumn();
    void duplicateAndDeleteLine();
    void blockCommentSpansLines();
    void multilineStringsRespectDefinition();
    void rehighlightPreservesFoldBit();
    void themeChangesSyntaxColors();
    void blockSelectionCopyAndCut();
    void contextMenuContainsEditingActions();
    void macroRecordsTextAndPlaysBack();
};

void TestEditor::init()
{
    Theme::setMode(Theme::Mode::Light);
}

void TestEditor::autoClosesPairs()
{
    Editor editor;
    editor.resize(400, 300);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    QTest::keyClicks(&editor, "(");
    QCOMPARE(editor.toPlainText(), QStringLiteral("()"));
    QCOMPARE(editor.textCursor().position(), 1);

    QTest::keyClicks(&editor, "x");
    QCOMPARE(editor.toPlainText(), QStringLiteral("(x)"));
    QCOMPARE(editor.textCursor().position(), 2);

    QTest::keyClicks(&editor, "[");
    QCOMPARE(editor.toPlainText(), QStringLiteral("(x[])"));
    QCOMPARE(editor.textCursor().position(), 3);
}

void TestEditor::stepsOverClosingCharacter()
{
    Editor editor;
    editor.resize(400, 300);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    QTest::keyClicks(&editor, "(");
    QTest::keyClicks(&editor, "a");
    QTest::keyClicks(&editor, ")");

    QCOMPARE(editor.toPlainText(), QStringLiteral("(a)"));
    QCOMPARE(editor.textCursor().position(), 3);
}

void TestEditor::wrapsSelection()
{
    Editor editor;
    editor.resize(400, 300);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    editor.setPlainText(QStringLiteral("word"));
    QTextCursor selection = editor.textCursor();
    selection.select(QTextCursor::WordUnderCursor);
    editor.setTextCursor(selection);

    QTest::keyClicks(&editor, "(");
    QCOMPARE(editor.toPlainText(), QStringLiteral("(word)"));
    QCOMPARE(editor.textCursor().selectedText(), QStringLiteral("word"));
}

void TestEditor::pairBackspace()
{
    Editor editor;
    editor.resize(400, 300);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    QTest::keyClicks(&editor, "(");
    QCOMPARE(editor.toPlainText(), QStringLiteral("()"));

    QTest::keyClick(&editor, Qt::Key_Backspace);
    QCOMPARE(editor.toPlainText(), QString());
}

void TestEditor::autoIndentInsideBlock()
{
    Editor editor;
    editor.resize(400, 300);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    editor.setPlainText(QStringLiteral("if (ready) {"));
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QStringLiteral("if (ready) {\n    "));

    editor.setPlainText(QStringLiteral("    value:"));
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QStringLiteral("    value:\n        "));
}

void TestEditor::gotoLineAndColumn()
{
    Editor editor;
    editor.setPlainText(QStringLiteral("one\ntwo\nthree"));

    editor.gotoLine(2, 2);
    QCOMPARE(editor.currentLine(), 2);
    QCOMPARE(editor.currentColumn(), 2);
    QCOMPARE(editor.textCursor().position(), 5);

    // Columns beyond the end of the line are clamped.
    editor.gotoLine(3, 99);
    QCOMPARE(editor.currentLine(), 3);
    QCOMPARE(editor.currentColumn(), 6);
}

void TestEditor::duplicateAndDeleteLine()
{
    Editor editor;
    editor.setPlainText(QStringLiteral("one\ntwo\nthree"));

    editor.gotoLine(2);
    editor.duplicateCurrentLine();
    QCOMPARE(editor.toPlainText(), QStringLiteral("one\ntwo\ntwo\nthree"));

    editor.gotoLine(2);
    editor.deleteCurrentLine();
    QCOMPARE(editor.toPlainText(), QStringLiteral("one\ntwo\nthree"));
}

namespace {

const QColor kCommentColor(0x8A, 0x8A, 0x8A);
const QColor kStringColor(0x06, 0x7D, 0x17);
const QColor kKeywordColor(0x00, 0x33, 0xB3);

bool hasColorAt(const QTextBlock &block, int position, const QColor &color)
{
    const QList<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (position >= range.start && position < range.start + range.length)
            return range.format.foreground().color() == color;
    }
    return false;
}

} // namespace

void TestEditor::blockCommentSpansLines()
{
    QTextDocument document;
    SyntaxDefinition definition;
    definition.blockCommentStart = QStringLiteral("/*");
    definition.blockCommentEnd = QStringLiteral("*/");
    definition.stringDelimiters = {QStringLiteral("\"")};
    definition.keywords = {QStringLiteral("int")};

    SyntaxHighlighter highlighter(&document, definition);
    document.setPlainText(QStringLiteral("int a; /* start\nmiddle\nend */ int b;"));
    highlighter.rehighlight();

    const QTextBlock first = document.firstBlock();
    const QTextBlock second = first.next();
    const QTextBlock third = second.next();

    QCOMPARE(first.userState() & BlockState::SyntaxMask, 1);
    QCOMPARE(second.userState() & BlockState::SyntaxMask, 1);
    QCOMPARE(third.userState() & BlockState::SyntaxMask, 0);

    QVERIFY(hasColorAt(first, 7, kCommentColor));
    QVERIFY(hasColorAt(second, 0, kCommentColor));
    QVERIFY(hasColorAt(third, 0, kCommentColor));
    QVERIFY(hasColorAt(third, 7, kKeywordColor)); // "int" after the comment
}

void TestEditor::multilineStringsRespectDefinition()
{
    QTextDocument document;
    SyntaxDefinition definition;
    definition.stringDelimiters = {QStringLiteral("\""), QStringLiteral("'")};
    definition.multilineStringDelimiters = {QStringLiteral("\"\"\""),
                                            QStringLiteral("'''")};

    SyntaxHighlighter highlighter(&document, definition);
    document.setPlainText(QStringLiteral(
        "x = \"\"\"hello\n"
        "world\"\"\"\n"
        "y = \"single\"\n"
        "z = \"open\n"
        "q = 1"));
    highlighter.rehighlight();

    const QTextBlock first = document.firstBlock();
    const QTextBlock second = first.next();
    const QTextBlock third = second.next();
    const QTextBlock fourth = third.next();
    const QTextBlock fifth = fourth.next();

    QVERIFY((first.userState() & BlockState::SyntaxMask) >= 100);
    QCOMPARE(second.userState() & BlockState::SyntaxMask, 0);
    QCOMPARE(third.userState() & BlockState::SyntaxMask, 0);
    // A plain double quote must not leak into the next line.
    QCOMPARE(fourth.userState() & BlockState::SyntaxMask, 0);
    QCOMPARE(fifth.userState() & BlockState::SyntaxMask, 0);

    QVERIFY(hasColorAt(first, 4, kStringColor));
    QVERIFY(hasColorAt(second, 0, kStringColor));
    QVERIFY(hasColorAt(fourth, 4, kStringColor));
    QVERIFY(!hasColorAt(fifth, 0, kStringColor));
}

void TestEditor::rehighlightPreservesFoldBit()
{
    QTextDocument document;
    SyntaxDefinition definition;
    definition.blockCommentStart = QStringLiteral("/*");
    definition.blockCommentEnd = QStringLiteral("*/");

    SyntaxHighlighter highlighter(&document, definition);
    document.setPlainText(QStringLiteral("/* start\nmiddle\nend */"));
    highlighter.rehighlight();

    QTextBlock first = document.firstBlock();
    QCOMPARE(first.userState() & BlockState::SyntaxMask, 1);
    first.setUserState((first.userState() & BlockState::SyntaxMask) | BlockState::Folded);

    highlighter.rehighlight();

    first = document.firstBlock();
    QVERIFY(first.userState() & BlockState::Folded);
    QCOMPARE(first.userState() & BlockState::SyntaxMask, 1);
    // The fold bit on the previous block must not confuse the syntax state.
    QCOMPARE(first.next().userState() & BlockState::SyntaxMask, 1);
}

void TestEditor::themeChangesSyntaxColors()
{
    QTextDocument document;
    SyntaxDefinition definition;
    definition.keywords = {QStringLiteral("int")};
    definition.stringDelimiters = {QStringLiteral("\"")};

    SyntaxHighlighter highlighter(&document, definition);
    document.setPlainText(QStringLiteral("int x = \"hi\";"));
    highlighter.rehighlight();

    const QTextBlock block = document.firstBlock();
    QVERIFY(hasColorAt(block, 0, Theme::colors(Theme::Mode::Light).keyword));
    QVERIFY(hasColorAt(block, 8, Theme::colors(Theme::Mode::Light).string));

    Theme::setMode(Theme::Mode::Dark);
    highlighter.refreshTheme();

    const QTextBlock darkBlock = document.firstBlock();
    QVERIFY(hasColorAt(darkBlock, 0, Theme::colors(Theme::Mode::Dark).keyword));
    QVERIFY(hasColorAt(darkBlock, 8, Theme::colors(Theme::Mode::Dark).string));

    QVERIFY(Theme::palette(Theme::Mode::Dark).color(QPalette::Base)
            != Theme::palette(Theme::Mode::Light).color(QPalette::Base));
    QCOMPARE(Theme::fromKey(QStringLiteral("dark")), Theme::Mode::Dark);
    QCOMPARE(Theme::fromKey(QStringLiteral("unknown")), Theme::Mode::Light);
}

void TestEditor::blockSelectionCopyAndCut()
{
    Editor editor;
    editor.resize(500, 300);
    editor.setPlainText(QStringLiteral("abcdefgh\nijklmnop\nqrstuvwx"));
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    QVERIFY(!editor.hasBlockSelection());
    editor.selectBlock(0, 2, 2, 6);
    QVERIFY(editor.hasBlockSelection());
    QCOMPARE(editor.blockSelectionText(), QStringLiteral("cdef\nklmn\nstuv"));

    QApplication::clipboard()->clear();
    editor.copySelection();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("cdef\nklmn\nstuv"));
    QVERIFY(editor.hasBlockSelection());

    editor.cutSelection();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("cdef\nklmn\nstuv"));
    QCOMPARE(editor.toPlainText(), QStringLiteral("abgh\nijop\nqrwx"));
    QVERIFY(!editor.hasBlockSelection());

    editor.setBlockSelectionMode(true);
    QVERIFY(editor.blockSelectionMode());
    editor.setBlockSelectionMode(false);
    QVERIFY(!editor.blockSelectionMode());

    // Alt+drag also creates a block selection directly on the viewport.
    editor.setPlainText(QStringLiteral("abcdefgh\nijklmnop\nqrstuvwx"));
    const auto pointAt = [&editor](int blockNumber, int column) {
        const QTextBlock block = editor.document()->findBlockByNumber(blockNumber);
        QTextCursor cursor(editor.document());
        cursor.setPosition(block.position() + column);
        const QRect rectangle = editor.cursorRect(cursor);
        return QPoint(rectangle.x() + 1, rectangle.center().y());
    };
    QTest::mousePress(editor.viewport(), Qt::LeftButton, Qt::AltModifier, pointAt(0, 2));
    QTest::mouseMove(editor.viewport(), pointAt(2, 6));
    QTest::mouseRelease(editor.viewport(), Qt::LeftButton, Qt::AltModifier, pointAt(2, 6));
    QVERIFY(editor.hasBlockSelection());
    QVERIFY(!editor.blockSelectionText().isEmpty());
}

void TestEditor::contextMenuContainsEditingActions()
{
    Editor editor;
    editor.resize(500, 300);
    editor.setPlainText(QStringLiteral("alpha beta"));
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    QStringList labels;
    QTimer::singleShot(100, [&labels]() {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (!menu)
            return;
        for (QAction *action : menu->actions()) {
            if (!action->isSeparator() && !action->menu())
                labels.append(action->text());
        }
        menu->close();
    });

    QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(10, 10),
                            editor.mapToGlobal(QPoint(10, 10)));
    QApplication::sendEvent(editor.viewport(), &event);

    const QString all = labels.join(QLatin1Char('|'));
    QVERIFY(all.contains(QStringLiteral("Copy")));
    QVERIFY(all.contains(QStringLiteral("Paste")));
    QVERIFY(all.contains(QStringLiteral("uplicate")));
    QVERIFY(all.contains(QStringLiteral("Column Selection")));
    QVERIFY(all.contains(QStringLiteral("Find")));
    QVERIFY(all.contains(QStringLiteral("Go to Line")));
}

void TestEditor::macroRecordsTextAndPlaysBack()
{
    Editor editor;
    MacroManager manager;
    editor.setMacroManager(&manager);
    editor.resize(400, 300);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    manager.startRecording(QStringLiteral("Typed text"));
    QTest::keyClicks(&editor, QStringLiteral("abc"));

    QInputMethodEvent ime;
    ime.setCommitString(QStringLiteral("áç"));
    QApplication::sendEvent(&editor, &ime);

    Macro *macro = manager.stopRecording();
    QVERIFY(macro);
    QCOMPARE(macro->actions().size(), 1);
    QCOMPARE(macro->actions().first().type, MacroAction::TextInput);
    QCOMPARE(macro->actions().first().text, QStringLiteral("abcáç"));

    editor.clear();
    manager.playMacro(macro, &editor);
    QCOMPARE(editor.toPlainText(), QStringLiteral("abcáç"));
}

QTEST_MAIN(TestEditor)

#include "test_editor.moc"
