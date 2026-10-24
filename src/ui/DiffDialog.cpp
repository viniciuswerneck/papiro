#include "ui/DiffDialog.h"

#include "core/SyntaxDefinition.h"
#include "editor/SyntaxHighlighter.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTextBlock>
#include <QRegularExpression>

namespace papiro {

static QVector<QString> splitLines(const QString &text)
{
    return text.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::KeepEmptyParts);
}

DiffResult DiffDialog::computeDiff(const QString &oldText, const QString &newText)
{
    DiffResult result;
    const QVector<QString> oldLines = splitLines(oldText);
    const QVector<QString> newLines = splitLines(newText);

    const int oldLen = oldLines.size();
    const int newLen = newLines.size();

    // Simple O(N*M) LCS-based diff for clarity
    QVector<QVector<int>> dp(oldLen + 1, QVector<int>(newLen + 1, 0));
    for (int i = oldLen - 1; i >= 0; --i) {
        for (int j = newLen - 1; j >= 0; --j) {
            if (oldLines[i] == newLines[j]) {
                dp[i][j] = 1 + dp[i + 1][j + 1];
            } else {
                dp[i][j] = qMax(dp[i + 1][j], dp[i][j + 1]);
            }
        }
    }

    struct Edit {
        DiffLine::Type type;
        int oldIndex;
        int newIndex;
    };

    // Reconstruct the full edit script. Deletions come before additions so a
    // modified line is shown as "-old" followed by "+new".
    QVector<Edit> edits;
    edits.reserve(oldLen + newLen);
    int i = 0;
    int j = 0;
    while (i < oldLen || j < newLen) {
        if (i < oldLen && j < newLen && oldLines[i] == newLines[j]) {
            edits.append({DiffLine::Type::Context, i, j});
            ++i;
            ++j;
        } else if (i < oldLen && (j >= newLen || dp[i + 1][j] >= dp[i][j + 1])) {
            edits.append({DiffLine::Type::Deletion, i, -1});
            ++i;
        } else {
            edits.append({DiffLine::Type::Addition, -1, j});
            ++j;
        }
    }

    // Locate contiguous runs of changed lines (a run may contain both
    // deletions and additions, e.g. a modified line).
    QVector<QPair<int, int>> runs;
    for (int index = 0; index < edits.size();) {
        if (edits[index].type == DiffLine::Type::Context) {
            ++index;
            continue;
        }
        const int start = index;
        while (index + 1 < edits.size() && edits[index + 1].type != DiffLine::Type::Context)
            ++index;
        runs.append({start, index});
        ++index;
    }

    // Expand each run by three context lines and merge runs whose unchanged
    // gap is at most six lines. Larger gaps start a new hunk.
    const int contextLines = 3;
    QVector<QPair<int, int>> ranges;
    for (const QPair<int, int> &run : runs) {
        const int start = qMax(0, run.first - contextLines);
        const int end = qMin(edits.size() - 1, run.second + contextLines);
        if (!ranges.isEmpty() && start <= ranges.last().second + 1) {
            ranges.last().second = qMax(ranges.last().second, end);
        } else {
            ranges.append({start, end});
        }
    }

    // Number of old/new lines consumed before each edit.
    QVector<int> oldPosition(edits.size() + 1, 0);
    QVector<int> newPosition(edits.size() + 1, 0);
    for (int index = 0; index < edits.size(); ++index) {
        oldPosition[index + 1] = oldPosition[index]
            + (edits[index].type != DiffLine::Type::Addition ? 1 : 0);
        newPosition[index + 1] = newPosition[index]
            + (edits[index].type != DiffLine::Type::Deletion ? 1 : 0);
    }

    for (const QPair<int, int> &range : ranges) {
        DiffHunk hunk;
        const Edit &first = edits[range.first];
        // When the hunk starts with an insertion/deletion there is no line on
        // the other side: use the position just before the insertion point.
        hunk.oldStart = (first.type == DiffLine::Type::Addition)
            ? oldPosition[range.first] : oldPosition[range.first] + 1;
        hunk.newStart = (first.type == DiffLine::Type::Deletion)
            ? newPosition[range.first] : newPosition[range.first] + 1;

        for (int index = range.first; index <= range.second; ++index) {
            const Edit &edit = edits[index];
            DiffLine line;
            line.type = edit.type;
            if (edit.type == DiffLine::Type::Context) {
                line.oldLine = oldLines[edit.oldIndex];
                line.newLine = newLines[edit.newIndex];
                line.oldLineNumber = edit.oldIndex + 1;
                line.newLineNumber = edit.newIndex + 1;
                hunk.oldCount++;
                hunk.newCount++;
            } else if (edit.type == DiffLine::Type::Deletion) {
                line.oldLine = oldLines[edit.oldIndex];
                line.oldLineNumber = edit.oldIndex + 1;
                hunk.oldCount++;
            } else {
                line.newLine = newLines[edit.newIndex];
                line.newLineNumber = edit.newIndex + 1;
                hunk.newCount++;
            }
            hunk.lines.append(line);
        }

        result.hunks.append(hunk);
    }

    return result;
}

// --- DiffDialog Implementation ---

DiffDialog::DiffDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Diff Viewer"));
    setModal(false);
    resize(1200, 800);
    setupUI();
    setupConnections();
}

DiffDialog::~DiffDialog()
{
    delete m_oldHighlighter;
    delete m_newHighlighter;
    delete m_unifiedHighlighter;
}

void DiffDialog::setFiles(const QString &oldFilePath, const QString &newFilePath)
{
    QFile oldFile(oldFilePath);
    QFile newFile(newFilePath);

    if (!oldFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file: %1").arg(oldFilePath));
        return;
    }
    if (!newFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file: %1").arg(newFilePath));
        return;
    }

    m_oldText = QString::fromUtf8(oldFile.readAll());
    m_newText = QString::fromUtf8(newFile.readAll());
    m_hasData = true;

    m_diff = computeDiff(m_oldText, m_newText);
    m_diff.oldFilePath = oldFilePath;
    m_diff.newFilePath = newFilePath;
    m_oldTitle = QFileInfo(oldFilePath).fileName();
    m_newTitle = QFileInfo(newFilePath).fileName();

    displayDiff(m_diff);
}

void DiffDialog::setTexts(const QString &oldText, const QString &newText,
                          const QString &oldTitle, const QString &newTitle)
{
    m_oldText = oldText;
    m_newText = newText;
    m_hasData = true;

    m_diff = computeDiff(m_oldText, m_newText);
    m_diff.oldFilePath.clear();
    m_diff.newFilePath.clear();
    m_oldTitle = oldTitle;
    m_newTitle = newTitle;
    displayDiff(m_diff);
}

void DiffDialog::setupUI()
{
    // Toolbar
    m_toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(4, 4, 4, 4);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Side by Side"), 0);
    m_modeCombo->addItem(tr("Unified"), 1);
    toolbarLayout->addWidget(new QLabel(tr("Mode:"), this));
    toolbarLayout->addWidget(m_modeCombo);

    toolbarLayout->addStretch();

    m_swapButton = new QPushButton(tr("Swap"), this);
    m_swapButton->setToolTip(tr("Swap left/right files"));
    toolbarLayout->addWidget(m_swapButton);

    m_prevButton = new QToolButton(this);
    m_prevButton->setText(tr("Prev"));
    m_prevButton->setEnabled(false);
    toolbarLayout->addWidget(m_prevButton);

    m_nextButton = new QToolButton(this);
    m_nextButton->setText(tr("Next"));
    m_nextButton->setEnabled(false);
    toolbarLayout->addWidget(m_nextButton);

    // Side-by-side view
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // Old side
    QWidget *oldWidget = new QWidget(this);
    auto *oldLayout = new QVBoxLayout(oldWidget);
    oldLayout->setContentsMargins(0, 0, 0, 0);
    oldLayout->setSpacing(0);

    m_oldLineNumbers = new QPlainTextEdit(oldWidget);
    m_oldLineNumbers->setReadOnly(true);
    m_oldLineNumbers->setMaximumWidth(50);
    m_oldLineNumbers->setFrameShape(QFrame::NoFrame);
    m_oldLineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_oldLineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_oldLineNumbers->setFocusPolicy(Qt::NoFocus);

    m_oldEditor = new QPlainTextEdit(oldWidget);
    m_oldEditor->setReadOnly(true);
    m_oldEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_oldEditor->setFrameShape(QFrame::NoFrame);
    m_oldEditor->setContextMenuPolicy(Qt::CustomContextMenu);

    QSplitter *oldSplitter = new QSplitter(Qt::Horizontal, oldWidget);
    oldSplitter->addWidget(m_oldLineNumbers);
    oldSplitter->addWidget(m_oldEditor);
    oldSplitter->setSizes({50, 500});
    oldSplitter->setHandleWidth(0);
    oldSplitter->setChildrenCollapsible(false);

    oldLayout->addWidget(oldSplitter);

    // New side
    QWidget *newWidget = new QWidget(this);
    auto *newLayout = new QVBoxLayout(newWidget);
    newLayout->setContentsMargins(0, 0, 0, 0);
    newLayout->setSpacing(0);

    m_newLineNumbers = new QPlainTextEdit(newWidget);
    m_newLineNumbers->setReadOnly(true);
    m_newLineNumbers->setMaximumWidth(50);
    m_newLineNumbers->setFrameShape(QFrame::NoFrame);
    m_newLineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_newLineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_newLineNumbers->setFocusPolicy(Qt::NoFocus);

    m_newEditor = new QPlainTextEdit(newWidget);
    m_newEditor->setReadOnly(true);
    m_newEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_newEditor->setFrameShape(QFrame::NoFrame);
    m_newEditor->setContextMenuPolicy(Qt::CustomContextMenu);

    QSplitter *newSplitter = new QSplitter(Qt::Horizontal, newWidget);
    newSplitter->addWidget(m_newLineNumbers);
    newSplitter->addWidget(m_newEditor);
    newSplitter->setSizes({50, 500});
    newSplitter->setHandleWidth(0);
    newSplitter->setChildrenCollapsible(false);

    newLayout->addWidget(newSplitter);

    m_splitter->addWidget(oldWidget);
    m_splitter->addWidget(newWidget);
    m_splitter->setSizes({600, 600});

    // Unified view
    QWidget *unifiedWidget = new QWidget(this);
    auto *unifiedLayout = new QVBoxLayout(unifiedWidget);
    unifiedLayout->setContentsMargins(0, 0, 0, 0);
    unifiedLayout->setSpacing(0);

    m_unifiedLineNumbers = new QPlainTextEdit(unifiedWidget);
    m_unifiedLineNumbers->setReadOnly(true);
    m_unifiedLineNumbers->setMaximumWidth(50);
    m_unifiedLineNumbers->setFrameShape(QFrame::NoFrame);
    m_unifiedLineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_unifiedLineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_unifiedLineNumbers->setFocusPolicy(Qt::NoFocus);

    m_unifiedEditor = new QPlainTextEdit(unifiedWidget);
    m_unifiedEditor->setReadOnly(true);
    m_unifiedEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_unifiedEditor->setFrameShape(QFrame::NoFrame);
    m_unifiedEditor->setContextMenuPolicy(Qt::CustomContextMenu);

    QSplitter *unifiedSplitter = new QSplitter(Qt::Horizontal, unifiedWidget);
    unifiedSplitter->addWidget(m_unifiedLineNumbers);
    unifiedSplitter->addWidget(m_unifiedEditor);
    unifiedSplitter->setSizes({50, 1150});
    unifiedSplitter->setHandleWidth(0);
    unifiedSplitter->setChildrenCollapsible(false);

    unifiedLayout->addWidget(unifiedSplitter);

    // Stacked layout for modes
    QWidget *diffContainer = new QWidget(this);
    auto *diffLayout = new QVBoxLayout(diffContainer);
    diffLayout->setContentsMargins(0, 0, 0, 0);
    diffLayout->addWidget(m_splitter);
    diffLayout->addWidget(unifiedWidget);
    unifiedWidget->hide();

    // "Files are identical" state
    m_identicalLabel = new QLabel(tr("Files are identical"), this);
    m_identicalLabel->setAlignment(Qt::AlignCenter);
    m_identicalLabel->hide();

    // Main layout
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_toolbar);
    mainLayout->addWidget(m_identicalLabel);
    mainLayout->addWidget(diffContainer, 1);

    applyTheme();
}

void DiffDialog::setupConnections()
{
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiffDialog::onModeChanged);
    connect(m_nextButton, &QToolButton::clicked, this, &DiffDialog::onNextChange);
    connect(m_prevButton, &QToolButton::clicked, this, &DiffDialog::onPreviousChange);
    connect(m_swapButton, &QPushButton::clicked, this, &DiffDialog::onSwapFiles);

    // Sync scrolling
    connect(m_oldEditor->verticalScrollBar(), &QScrollBar::valueChanged, [this](int value) {
        if (!m_syncScrolling) {
            m_syncScrolling = true;
            m_newEditor->verticalScrollBar()->setValue(value);
            m_oldLineNumbers->verticalScrollBar()->setValue(value);
            m_newLineNumbers->verticalScrollBar()->setValue(value);
            m_syncScrolling = false;
        }
    });
    connect(m_newEditor->verticalScrollBar(), &QScrollBar::valueChanged, [this](int value) {
        if (!m_syncScrolling) {
            m_syncScrolling = true;
            m_oldEditor->verticalScrollBar()->setValue(value);
            m_oldLineNumbers->verticalScrollBar()->setValue(value);
            m_newLineNumbers->verticalScrollBar()->setValue(value);
            m_syncScrolling = false;
        }
    });

    // Context menus
    m_oldEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    m_newEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    m_unifiedEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_oldEditor, &QWidget::customContextMenuRequested, this, &DiffDialog::showContextMenu);
    connect(m_newEditor, &QWidget::customContextMenuRequested, this, &DiffDialog::showContextMenu);
    connect(m_unifiedEditor, &QWidget::customContextMenuRequested, this, &DiffDialog::showContextMenu);
}

void DiffDialog::onModeChanged()
{
    // Rebuild the active view so change positions and highlighting match it.
    displayDiff(m_diff);
}

void DiffDialog::onNextChange()
{
    if (!m_changes.isEmpty()) {
        m_currentChangeIndex = (m_currentChangeIndex + 1) % m_changes.size();
        navigateToChange(m_currentChangeIndex);
    }
}

void DiffDialog::onPreviousChange()
{
    if (!m_changes.isEmpty()) {
        m_currentChangeIndex = (m_currentChangeIndex - 1 + m_changes.size()) % m_changes.size();
        navigateToChange(m_currentChangeIndex);
    }
}

void DiffDialog::onSwapFiles()
{
    qSwap(m_oldText, m_newText);
    qSwap(m_oldTitle, m_newTitle);
    qSwap(m_diff.oldFilePath, m_diff.newFilePath);

    const QString oldFilePath = m_diff.oldFilePath;
    const QString newFilePath = m_diff.newFilePath;
    m_diff = computeDiff(m_oldText, m_newText);
    m_diff.oldFilePath = oldFilePath;
    m_diff.newFilePath = newFilePath;

    displayDiff(m_diff);
}

void DiffDialog::onCopy()
{
    QWidget *focused = QApplication::focusWidget();
    if (focused == m_oldEditor || focused == m_newEditor || focused == m_unifiedEditor) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit *>(focused);
        if (editor)
            editor->copy();
    }
}

void DiffDialog::showContextMenu(const QPoint &pos)
{
    QWidget *focused = QApplication::focusWidget();
    QPlainTextEdit *editor = qobject_cast<QPlainTextEdit *>(focused);
    if (!editor)
        return;

    QMenu menu(this);
    QAction *copyAction = menu.addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &DiffDialog::onCopy);
    menu.addSeparator();
    QAction *selectAllAction = menu.addAction(tr("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, editor, &QPlainTextEdit::selectAll);
    menu.exec(editor->mapToGlobal(pos));
}

void DiffDialog::displayDiff(const DiffResult &diff)
{
    m_changes.clear();
    m_currentChangeIndex = -1;

    const int mode = m_modeCombo->currentIndex();
    if (mode == 0) {
        displaySideBySide(diff);
    } else {
        displayUnified(diff);
    }

    const bool hasChanges = !m_changes.isEmpty();
    m_nextButton->setEnabled(hasChanges);
    m_prevButton->setEnabled(hasChanges);
    m_identicalLabel->setVisible(m_hasData && diff.hunks.isEmpty());

    if (hasChanges) {
        m_currentChangeIndex = 0;
        navigateToChange(0);
    }
}

void DiffDialog::displaySideBySide(const DiffResult &diff)
{
    m_splitter->show();
    m_unifiedEditor->parentWidget()->hide();
    m_swapButton->show();

    QVector<int> oldLineNumbers, newLineNumbers;
    const QString oldText = buildSideBySideText(diff, true, oldLineNumbers);
    const QString newText = buildSideBySideText(diff, false, newLineNumbers);

    m_oldEditor->setPlainText(oldText);
    m_newEditor->setPlainText(newText);
    updateLineNumbers(m_oldLineNumbers, oldLineNumbers);
    updateLineNumbers(m_newLineNumbers, newLineNumbers);

    // Syntax highlighting (recreated whenever the files change).
    delete m_oldHighlighter;
    delete m_newHighlighter;
    m_oldHighlighter = nullptr;
    m_newHighlighter = nullptr;
    const SyntaxDefinition *oldDef = getSyntaxDefinitionForFile(m_diff.oldFilePath);
    const SyntaxDefinition *newDef = getSyntaxDefinitionForFile(m_diff.newFilePath);
    if (oldDef)
        m_oldHighlighter = new SyntaxHighlighter(m_oldEditor->document(), *oldDef);
    if (newDef)
        m_newHighlighter = new SyntaxHighlighter(m_newEditor->document(), *newDef);

    highlightChanges(m_oldEditor, diff.hunks, true);
    highlightChanges(m_newEditor, diff.hunks, false);

    // One entry per contiguous change block, using the display row shared by
    // both panes (every diff line occupies a row in each pane).
    int row = 0;
    for (const DiffHunk &hunk : diff.hunks) {
        int index = 0;
        while (index < hunk.lines.size()) {
            if (hunk.lines[index].type == DiffLine::Type::Context) {
                ++row;
                ++index;
                continue;
            }

            const int startRow = row;
            const int startIndex = index;
            ChangePosition change;
            while (index < hunk.lines.size()
                   && hunk.lines[index].type != DiffLine::Type::Context) {
                const DiffLine &line = hunk.lines[index];
                if (line.type == DiffLine::Type::Addition) {
                    change.isAddition = true;
                    if (change.newLineNumber < 0)
                        change.newLineNumber = line.newLineNumber;
                } else if (line.type == DiffLine::Type::Deletion) {
                    change.isDeletion = true;
                    if (change.oldLineNumber < 0)
                        change.oldLineNumber = line.oldLineNumber;
                }
                ++row;
                ++index;
            }

            change.blockNumber = startRow;
            change.lineCount = index - startIndex;
            change.lineNumber = change.isAddition ? change.newLineNumber
                                                  : change.oldLineNumber;
            m_changes.append(change);
        }
    }
}

void DiffDialog::displayUnified(const DiffResult &diff)
{
    m_splitter->hide();
    m_unifiedEditor->parentWidget()->show();
    m_swapButton->hide();

    QVector<QPair<int, int>> lineMapping;
    const QString unifiedText = buildUnifiedText(diff, lineMapping);
    m_unifiedEditor->setPlainText(unifiedText);

    // Extract line numbers for unified view
    QVector<int> unifiedLineNumbers;
    for (const auto &pair : lineMapping)
        unifiedLineNumbers.append(qMax(pair.first, pair.second));
    updateLineNumbers(m_unifiedLineNumbers, unifiedLineNumbers);

    // Syntax highlighting (recreated whenever the files change).
    delete m_unifiedHighlighter;
    m_unifiedHighlighter = nullptr;
    const SyntaxDefinition *definition = getSyntaxDefinitionForFile(m_diff.newFilePath);
    if (!definition)
        definition = getSyntaxDefinitionForFile(m_diff.oldFilePath);
    if (definition)
        m_unifiedHighlighter = new SyntaxHighlighter(m_unifiedEditor->document(), *definition);

    highlightUnified(m_unifiedEditor, diff.hunks);

    // One entry per contiguous change block. Hunk headers are display blocks
    // too, so they must be counted when computing block numbers.
    int row = 0;
    for (const DiffHunk &hunk : diff.hunks) {
        ++row;  // "@@ ... @@" header
        int index = 0;
        while (index < hunk.lines.size()) {
            if (hunk.lines[index].type == DiffLine::Type::Context) {
                ++row;
                ++index;
                continue;
            }

            const int startRow = row;
            const int startIndex = index;
            ChangePosition change;
            while (index < hunk.lines.size()
                   && hunk.lines[index].type != DiffLine::Type::Context) {
                const DiffLine &line = hunk.lines[index];
                if (line.type == DiffLine::Type::Addition) {
                    change.isAddition = true;
                    if (change.newLineNumber < 0)
                        change.newLineNumber = line.newLineNumber;
                } else if (line.type == DiffLine::Type::Deletion) {
                    change.isDeletion = true;
                    if (change.oldLineNumber < 0)
                        change.oldLineNumber = line.oldLineNumber;
                }
                ++row;
                ++index;
            }

            change.blockNumber = startRow;
            change.lineCount = index - startIndex;
            change.lineNumber = change.isAddition ? change.newLineNumber
                                                  : change.oldLineNumber;
            m_changes.append(change);
        }
    }
}

void DiffDialog::applyTheme()
{
    const auto colors = Theme::colors();
    const QString style = QString(
        "QPlainTextEdit { background: %1; color: %2; border: none; font-family: monospace; }"
        "QPlainTextEdit:focus { border: 1px solid %3; }"
        "QSplitter::handle { background: %4; }"
    ).arg(colors.gutterBackground.name(), colors.gutterText.name(),
          colors.bracketMatch.name(), colors.gutterBackground.darker(110).name());

    m_oldEditor->setStyleSheet(style);
    m_newEditor->setStyleSheet(style);
    m_unifiedEditor->setStyleSheet(style);
    m_oldLineNumbers->setStyleSheet(style);
    m_newLineNumbers->setStyleSheet(style);
    m_unifiedLineNumbers->setStyleSheet(style);

    m_identicalLabel->setStyleSheet(
        QString("QLabel { color: %1; padding: 12px; }").arg(colors.gutterText.name()));
}

void DiffDialog::updateLineNumbers(QPlainTextEdit* editor, const QVector<int>& lineNumbers)
{
    QString text;
    for (int num : lineNumbers) {
        if (num > 0)
            text += QString::number(num);
        text += QLatin1Char('\n');
    }
    editor->setPlainText(text);
    // Match the line height
    editor->setFont(m_oldEditor->font());
}

void DiffDialog::highlightChanges(QPlainTextEdit* editor, const QVector<DiffHunk>& hunks, bool isOld)
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat addFormat;
    addFormat.setBackground(QColor(0x2A, 0x5A, 0x2A));  // Dark green
    addFormat.setFontWeight(QFont::Bold);

    QTextCharFormat delFormat;
    delFormat.setBackground(QColor(0x5A, 0x2A, 0x2A));  // Dark red
    delFormat.setFontWeight(QFont::Bold);
    delFormat.setFontStrikeOut(true);

    QTextDocument *doc = editor->document();
    int blockIdx = 0;

    for (const DiffHunk &hunk : hunks) {
        for (const DiffLine &line : hunk.lines) {
            if ((isOld && line.type == DiffLine::Type::Deletion) ||
                (!isOld && line.type == DiffLine::Type::Addition)) {
                QTextBlock block = doc->findBlockByNumber(blockIdx);
                if (block.isValid()) {
                    QTextEdit::ExtraSelection sel;
                    sel.format = isOld ? delFormat : addFormat;
                    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                    sel.cursor = QTextCursor(block);
                    selections.append(sel);
                }
            }
            // Side-by-side pans are aligned: every diff line (including the
            // blank placeholder for the other side) occupies one block.
            ++blockIdx;
        }
    }
    editor->setExtraSelections(selections);
}

void DiffDialog::highlightUnified(QPlainTextEdit* editor, const QVector<DiffHunk>& hunks)
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat addFormat;
    addFormat.setBackground(QColor(0x2A, 0x5A, 0x2A));

    QTextCharFormat delFormat;
    delFormat.setBackground(QColor(0x5A, 0x2A, 0x2A));
    delFormat.setFontStrikeOut(true);

    QTextDocument *doc = editor->document();
    int blockIdx = 0;

    for (const DiffHunk &hunk : hunks) {
        ++blockIdx;  // Hunk header line
        for (const DiffLine &line : hunk.lines) {
            if (line.type != DiffLine::Type::Context) {
                QTextBlock block = doc->findBlockByNumber(blockIdx);
                if (block.isValid()) {
                    QTextEdit::ExtraSelection sel;
                    sel.format = (line.type == DiffLine::Type::Addition) ? addFormat : delFormat;
                    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                    sel.cursor = QTextCursor(block);
                    selections.append(sel);
                }
            }
            ++blockIdx;
        }
    }
    editor->setExtraSelections(selections);
}

QString DiffDialog::buildSideBySideText(const DiffResult &diff, bool isOld, QVector<int>& lineNumbers)
{
    QString text;
    lineNumbers.clear();

    for (const DiffHunk &hunk : diff.hunks) {
        for (const DiffLine &line : hunk.lines) {
            if (isOld) {
                if (line.type == DiffLine::Type::Deletion || line.type == DiffLine::Type::Context) {
                    text += line.oldLine + QLatin1Char('\n');
                    lineNumbers.append(line.oldLineNumber);
                } else if (line.type == DiffLine::Type::Addition) {
                    text += QLatin1Char('\n');
                    lineNumbers.append(-1);  // Empty line for addition
                }
            } else {
                if (line.type == DiffLine::Type::Addition || line.type == DiffLine::Type::Context) {
                    text += line.newLine + QLatin1Char('\n');
                    lineNumbers.append(line.newLineNumber);
                } else if (line.type == DiffLine::Type::Deletion) {
                    text += QLatin1Char('\n');
                    lineNumbers.append(-1);  // Empty line for deletion
                }
            }
        }
    }
    return text;
}

QString DiffDialog::buildUnifiedText(const DiffResult &diff, QVector<QPair<int, int>>& lineMapping)
{
    QString text;
    lineMapping.clear();

    for (const DiffHunk &hunk : diff.hunks) {
        // Hunk header
        text += QString("@@ -%1,%2 +%3,%4 @@\n").arg(hunk.oldStart).arg(hunk.oldCount).arg(hunk.newStart).arg(hunk.newCount);
        lineMapping.append({-1, -1});  // Header line

        for (const DiffLine &line : hunk.lines) {
            if (line.type == DiffLine::Type::Context) {
                text += " " + line.oldLine + QLatin1Char('\n');
                lineMapping.append({line.oldLineNumber, line.newLineNumber});
            } else if (line.type == DiffLine::Type::Addition) {
                text += "+" + line.newLine + QLatin1Char('\n');
                lineMapping.append({-1, line.newLineNumber});
            } else if (line.type == DiffLine::Type::Deletion) {
                text += "-" + line.oldLine + QLatin1Char('\n');
                lineMapping.append({line.oldLineNumber, -1});
            }
        }
    }
    return text;
}

const SyntaxDefinition* DiffDialog::getSyntaxDefinitionForFile(const QString& filePath) const
{
    if (filePath.trimmed().isEmpty())
        return nullptr;
    return SyntaxRepository::instance().forFile(filePath);
}

void DiffDialog::navigateToChange(int index)
{
    if (index < 0 || index >= m_changes.size())
        return;

    const ChangePosition &change = m_changes[index];

    auto selectRange = [](QPlainTextEdit *editor, int firstBlock, int lineCount) {
        if (!editor)
            return;
        QTextDocument *document = editor->document();
        const int startNumber = qMax(0, firstBlock);
        const int endNumber = qMax(startNumber, firstBlock + qMax(1, lineCount) - 1);
        const QTextBlock start = document->findBlockByNumber(startNumber);
        if (!start.isValid())
            return;
        QTextBlock end = document->findBlockByNumber(endNumber);
        if (!end.isValid())
            end = start;

        QTextCursor cursor(document);
        cursor.setPosition(start.position());
        cursor.setPosition(end.position() + qMax(0, end.length() - 1),
                           QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    };

    if (m_modeCombo->currentIndex() == 0) {
        selectRange(m_oldEditor, change.blockNumber, change.lineCount);
        selectRange(m_newEditor, change.blockNumber, change.lineCount);
        selectRange(m_oldLineNumbers, change.blockNumber, change.lineCount);
        selectRange(m_newLineNumbers, change.blockNumber, change.lineCount);
    } else {
        selectRange(m_unifiedEditor, change.blockNumber, change.lineCount);
        selectRange(m_unifiedLineNumbers, change.blockNumber, change.lineCount);
    }
}

} // namespace papiro