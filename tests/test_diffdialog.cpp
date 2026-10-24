#include "ui/DiffDialog.h"

#include <QComboBox>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QtTest>

using namespace papiro;

namespace {

QString numberedLines(int count)
{
    QStringList lines;
    for (int index = 1; index <= count; ++index)
        lines.append(QStringLiteral("line%1").arg(index, 2, 10, QLatin1Char('0')));
    return lines.join(QLatin1Char('\n'));
}

QString replaceLine(const QString &text, int lineNumber, const QString &replacement)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    lines[lineNumber - 1] = replacement;
    return lines.join(QLatin1Char('\n'));
}

} // namespace

class TestDiffDialog : public QObject {
    Q_OBJECT

private slots:
    void identicalFilesProduceNoHunks();
    void pureAddition();
    void pureDeletion();
    void modification();
    void changesAtFileBoundaries();
    void multipleSeparatedChanges();
    void closeChangesMergeIntoOneHunk();
    void contextAndBlankLines();
    void setTextsPopulatesChangesAndNavigates();
};

void TestDiffDialog::identicalFilesProduceNoHunks()
{
    const DiffResult result = DiffDialog::computeDiff(
        QStringLiteral("alpha\nbeta\ngamma"), QStringLiteral("alpha\nbeta\ngamma"));
    QVERIFY(result.hunks.isEmpty());

    const DiffResult empty = DiffDialog::computeDiff(QString(), QString());
    QVERIFY(empty.hunks.isEmpty());
}

void TestDiffDialog::pureAddition()
{
    const DiffResult result = DiffDialog::computeDiff(
        QStringLiteral("a\nb\nc"), QStringLiteral("a\nx\nb\nc"));

    QCOMPARE(result.hunks.size(), 1);
    const DiffHunk &hunk = result.hunks.first();
    QCOMPARE(hunk.oldStart, 1);
    QCOMPARE(hunk.oldCount, 3);
    QCOMPARE(hunk.newStart, 1);
    QCOMPARE(hunk.newCount, 4);
    QCOMPARE(hunk.lines.size(), 4);

    int additions = 0;
    int deletions = 0;
    for (const DiffLine &line : hunk.lines) {
        if (line.type == DiffLine::Type::Addition) {
            ++additions;
            QCOMPARE(line.newLine, QStringLiteral("x"));
            QCOMPARE(line.newLineNumber, 2);
            QVERIFY(line.oldLineNumber < 0);
        } else if (line.type == DiffLine::Type::Deletion) {
            ++deletions;
        }
    }
    QCOMPARE(additions, 1);
    QCOMPARE(deletions, 0);
}

void TestDiffDialog::pureDeletion()
{
    const DiffResult result = DiffDialog::computeDiff(
        QStringLiteral("a\nb\nc"), QStringLiteral("a\nc"));

    QCOMPARE(result.hunks.size(), 1);
    const DiffHunk &hunk = result.hunks.first();
    QCOMPARE(hunk.oldStart, 1);
    QCOMPARE(hunk.oldCount, 3);
    QCOMPARE(hunk.newStart, 1);
    QCOMPARE(hunk.newCount, 2);
    QCOMPARE(hunk.lines.size(), 3);

    int additions = 0;
    int deletions = 0;
    for (const DiffLine &line : hunk.lines) {
        if (line.type == DiffLine::Type::Addition) {
            ++additions;
        } else if (line.type == DiffLine::Type::Deletion) {
            ++deletions;
            QCOMPARE(line.oldLine, QStringLiteral("b"));
            QCOMPARE(line.oldLineNumber, 2);
            QVERIFY(line.newLineNumber < 0);
        }
    }
    QCOMPARE(additions, 0);
    QCOMPARE(deletions, 1);
}

void TestDiffDialog::modification()
{
    const DiffResult result = DiffDialog::computeDiff(
        QStringLiteral("a\nold\nc"), QStringLiteral("a\nnew\nc"));

    QCOMPARE(result.hunks.size(), 1);
    const DiffHunk &hunk = result.hunks.first();
    QCOMPARE(hunk.oldStart, 1);
    QCOMPARE(hunk.oldCount, 3);
    QCOMPARE(hunk.newStart, 1);
    QCOMPARE(hunk.newCount, 3);
    QCOMPARE(hunk.lines.size(), 4);

    bool sawDeletion = false;
    bool sawAddition = false;
    for (const DiffLine &line : hunk.lines) {
        if (line.type == DiffLine::Type::Deletion) {
            sawDeletion = true;
            QCOMPARE(line.oldLine, QStringLiteral("old"));
            QCOMPARE(line.oldLineNumber, 2);
        } else if (line.type == DiffLine::Type::Addition) {
            sawAddition = true;
            QCOMPARE(line.newLine, QStringLiteral("new"));
            QCOMPARE(line.newLineNumber, 2);
        }
    }
    QVERIFY(sawDeletion);
    QVERIFY(sawAddition);
}

void TestDiffDialog::changesAtFileBoundaries()
{
    // Insertion before the first line: context starts at old line 1.
    const DiffResult prefix = DiffDialog::computeDiff(
        QStringLiteral("a\nb"), QStringLiteral("x\na\nb"));
    QCOMPARE(prefix.hunks.size(), 1);
    QCOMPARE(prefix.hunks.first().oldStart, 1);
    QCOMPARE(prefix.hunks.first().oldCount, 2);
    QCOMPARE(prefix.hunks.first().newStart, 1);
    QCOMPARE(prefix.hunks.first().newCount, 3);

    // Deletion of the last line: the new side ends before it.
    const DiffResult suffix = DiffDialog::computeDiff(
        QStringLiteral("a\nb\nc"), QStringLiteral("a\nb"));
    QCOMPARE(suffix.hunks.size(), 1);
    QCOMPARE(suffix.hunks.first().oldStart, 1);
    QCOMPARE(suffix.hunks.first().oldCount, 3);
    QCOMPARE(suffix.hunks.first().newStart, 1);
    QCOMPARE(suffix.hunks.first().newCount, 2);
}

void TestDiffDialog::multipleSeparatedChanges()
{
    const QString oldText = numberedLines(20);
    const QString newText = replaceLine(replaceLine(oldText, 3, QStringLiteral("changed03")),
                                        15, QStringLiteral("changed15"));

    const DiffResult result = DiffDialog::computeDiff(oldText, newText);
    QCOMPARE(result.hunks.size(), 2);

    // First change: two leading context lines, then three trailing ones.
    const DiffHunk &first = result.hunks.at(0);
    QCOMPARE(first.oldStart, 1);
    QCOMPARE(first.oldCount, 6);
    QCOMPARE(first.newStart, 1);
    QCOMPARE(first.newCount, 6);

    // Second change: three lines of context on each side.
    const DiffHunk &second = result.hunks.at(1);
    QCOMPARE(second.oldStart, 12);
    QCOMPARE(second.oldCount, 7);
    QCOMPARE(second.newStart, 12);
    QCOMPARE(second.newCount, 7);

    bool foundSecondChange = false;
    for (const DiffLine &line : second.lines) {
        if (line.type == DiffLine::Type::Addition
            && line.newLine == QStringLiteral("changed15")) {
            foundSecondChange = true;
            QCOMPARE(line.newLineNumber, 15);
        }
    }
    QVERIFY(foundSecondChange);
}

void TestDiffDialog::closeChangesMergeIntoOneHunk()
{
    const QString oldText = numberedLines(12);
    const QString newText = replaceLine(replaceLine(oldText, 3, QStringLiteral("changed03")),
                                        7, QStringLiteral("changed07"));

    const DiffResult result = DiffDialog::computeDiff(oldText, newText);
    QCOMPARE(result.hunks.size(), 1);

    const DiffHunk &hunk = result.hunks.first();
    QCOMPARE(hunk.oldStart, 1);
    QCOMPARE(hunk.oldCount, 10);
    QCOMPARE(hunk.newStart, 1);
    QCOMPARE(hunk.newCount, 10);
}

void TestDiffDialog::contextAndBlankLines()
{
    // A blank line is normal context and must not split or corrupt a hunk.
    const DiffResult contextResult = DiffDialog::computeDiff(
        QStringLiteral("alpha\n\nbeta\ngamma"), QStringLiteral("alpha\n\nbeta\ndelta"));

    QCOMPARE(contextResult.hunks.size(), 1);
    const DiffHunk &contextHunk = contextResult.hunks.first();
    QCOMPARE(contextHunk.oldStart, 1);
    QCOMPARE(contextHunk.oldCount, 4);
    QCOMPARE(contextHunk.newCount, 4);
    QCOMPARE(contextHunk.lines.at(1).type, DiffLine::Type::Context);
    QCOMPARE(contextHunk.lines.at(1).oldLine, QString());
    QCOMPARE(contextHunk.lines.at(1).oldLineNumber, 2);

    // Replacing a blank line with content.
    const DiffResult blankChange = DiffDialog::computeDiff(
        QStringLiteral("a\n\nc"), QStringLiteral("a\nx\nc"));
    QCOMPARE(blankChange.hunks.size(), 1);

    bool sawBlankDeletion = false;
    for (const DiffLine &line : blankChange.hunks.first().lines) {
        if (line.type == DiffLine::Type::Deletion && line.oldLine.isEmpty()) {
            sawBlankDeletion = true;
            QCOMPARE(line.oldLineNumber, 2);
        }
    }
    QVERIFY(sawBlankDeletion);
}

void TestDiffDialog::setTextsPopulatesChangesAndNavigates()
{
    DiffDialog dialog;
    dialog.setTexts(QStringLiteral("one\ntwo\nthree\nfour\nfive\nsix\nseven\neight"),
                    QStringLiteral("one\nTWO\nthree\nfour\nfive\nsix\nSEVEN\neight"));

    // Both changes belong to one merged hunk but remain separate blocks.
    QCOMPARE(dialog.changeCount(), 2);

    const QList<QToolButton *> allButtons = dialog.findChildren<QToolButton *>();
    QList<QToolButton *> navButtons;
    for (QToolButton *b : allButtons) {
        if (b->text() == "Prev" || b->text() == "Next")
            navButtons.append(b);
    }
    QCOMPARE(navButtons.size(), 2);
    for (QToolButton *button : navButtons)
        QVERIFY(button->isEnabled());

    QPlainTextEdit *sideEditor = nullptr;
    for (QPlainTextEdit *editor : dialog.findChildren<QPlainTextEdit *>()) {
        if (editor->toPlainText().contains(QStringLiteral("TWO"))) {
            sideEditor = editor;
            break;
        }
    }
    QVERIFY(sideEditor);
    QVERIFY(sideEditor->textCursor().hasSelection());

    // Wrap around both directions without going out of range.
    for (int step = 0; step < 5; ++step)
        QMetaObject::invokeMethod(&dialog, "onNextChange", Qt::DirectConnection);
    QVERIFY(sideEditor->textCursor().hasSelection());

    for (int step = 0; step < 5; ++step)
        QMetaObject::invokeMethod(&dialog, "onPreviousChange", Qt::DirectConnection);
    QVERIFY(sideEditor->textCursor().hasSelection());

    // Unified mode rebuilds the change list with display block offsets.
    QComboBox *mode = dialog.findChild<QComboBox *>();
    QVERIFY(mode);
    mode->setCurrentIndex(1);
    QCOMPARE(dialog.changeCount(), 2);

    QPlainTextEdit *unified = nullptr;
    for (QPlainTextEdit *editor : dialog.findChildren<QPlainTextEdit *>()) {
        if (editor->toPlainText().contains(QStringLiteral("@@"))) {
            unified = editor;
            break;
        }
    }
    QVERIFY(unified);
    QVERIFY(unified->textCursor().hasSelection());

    for (int step = 0; step < 4; ++step)
        QMetaObject::invokeMethod(&dialog, "onNextChange", Qt::DirectConnection);
    QVERIFY(unified->textCursor().hasSelection());
}

QTEST_MAIN(TestDiffDialog)

#include "test_diffdialog.moc"
