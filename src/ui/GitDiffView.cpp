#include "ui/GitDiffView.h"

#include "core/SyntaxDefinition.h"
#include "editor/SyntaxHighlighter.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QStyle>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

namespace papiro {

// -----------------------------------------------------------------------------
// GitDiffView Implementation
// -----------------------------------------------------------------------------

GitDiffView::GitDiffView(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
    applyTheme();
}

GitDiffView::~GitDiffView()
{
    delete m_oldHighlighter;
    delete m_newHighlighter;
    delete m_unifiedHighlighter;
}

void GitDiffView::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName("GitDiffToolbar");
    auto* toolbarLayout = new QHBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(4, 4, 4, 4);
    toolbarLayout->setSpacing(4);

    m_fileCombo = new QComboBox(m_toolbar);
    m_fileCombo->setMinimumWidth(200);
    m_fileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarLayout->addWidget(m_fileCombo, 1);

    // Mode button with dropdown
    m_modeButton = new QToolButton(m_toolbar);
    m_modeButton->setText(tr("Side-by-side"));
    m_modeButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_modeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_modeButton->setIcon(QIcon::fromTheme("view-split-horizontal", style()->standardIcon(QStyle::SP_FileDialogContentsView)));

    QMenu* modeMenu = new QMenu(m_modeButton);
    QAction* sideBySideAction = modeMenu->addAction(tr("Side-by-side"));
    sideBySideAction->setData(static_cast<int>(DiffMode::SideBySide));
    QAction* unifiedAction = modeMenu->addAction(tr("Unified"));
    unifiedAction->setData(static_cast<int>(DiffMode::Unified));
    connect(modeMenu, &QMenu::triggered, this, [this](QAction* action) {
        setMode(static_cast<DiffMode>(action->data().toInt()));
    });
    m_modeButton->setMenu(modeMenu);
    toolbarLayout->addWidget(m_modeButton);

    m_prevButton = new QToolButton(m_toolbar);
    m_prevButton->setIcon(QIcon::fromTheme("go-up", style()->standardIcon(QStyle::SP_ArrowUp)));
    m_prevButton->setToolTip(tr("Previous Change (Shift+F7)"));
    m_prevButton->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F7));
    toolbarLayout->addWidget(m_prevButton);

    m_nextButton = new QToolButton(m_toolbar);
    m_nextButton->setIcon(QIcon::fromTheme("go-down", style()->standardIcon(QStyle::SP_ArrowDown)));
    m_nextButton->setToolTip(tr("Next Change (F7)"));
    m_nextButton->setShortcut(QKeySequence(Qt::Key_F7));
    toolbarLayout->addWidget(m_nextButton);

    mainLayout->addWidget(m_toolbar);

    // Stacked content area for different modes
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName("GitDiffSplitter");
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    mainLayout->addWidget(m_splitter, 1);

    // Side-by-side: Old file panel
    auto* oldPanel = new QWidget(m_splitter);
    auto* oldLayout = new QHBoxLayout(oldPanel);
    oldLayout->setContentsMargins(0, 0, 0, 0);
    oldLayout->setSpacing(0);

    m_oldLineNumbers = new QPlainTextEdit(oldPanel);
    m_oldLineNumbers->setReadOnly(true);
    m_oldLineNumbers->setMaximumWidth(50);
    m_oldLineNumbers->setFocusPolicy(Qt::NoFocus);
    m_oldLineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_oldLineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_oldLineNumbers->setFrameShape(QFrame::NoFrame);
    oldLayout->addWidget(m_oldLineNumbers);

    m_oldEditor = new QPlainTextEdit(oldPanel);
    m_oldEditor->setReadOnly(true);
    m_oldEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_oldEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    oldLayout->addWidget(m_oldEditor, 1);

    // Side-by-side: New file panel
    auto* newPanel = new QWidget(m_splitter);
    auto* newLayout = new QHBoxLayout(newPanel);
    newLayout->setContentsMargins(0, 0, 0, 0);
    newLayout->setSpacing(0);

    m_newEditor = new QPlainTextEdit(newPanel);
    m_newEditor->setReadOnly(true);
    m_newEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_newEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    newLayout->addWidget(m_newEditor, 1);

    m_newLineNumbers = new QPlainTextEdit(newPanel);
    m_newLineNumbers->setReadOnly(true);
    m_newLineNumbers->setMaximumWidth(50);
    m_newLineNumbers->setFocusPolicy(Qt::NoFocus);
    m_newLineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_newLineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_newLineNumbers->setFrameShape(QFrame::NoFrame);
    newLayout->addWidget(m_newLineNumbers);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    // Unified mode widgets (initially hidden)
    auto* unifiedPanel = new QWidget(m_splitter);
    auto* unifiedLayout = new QHBoxLayout(unifiedPanel);
    unifiedLayout->setContentsMargins(0, 0, 0, 0);
    unifiedLayout->setSpacing(0);

    m_unifiedLineNumbers = new QPlainTextEdit(unifiedPanel);
    m_unifiedLineNumbers->setReadOnly(true);
    m_unifiedLineNumbers->setMaximumWidth(60);
    m_unifiedLineNumbers->setFocusPolicy(Qt::NoFocus);
    m_unifiedLineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_unifiedLineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_unifiedLineNumbers->setFrameShape(QFrame::NoFrame);
    unifiedLayout->addWidget(m_unifiedLineNumbers);

    m_unifiedEditor = new QPlainTextEdit(unifiedPanel);
    m_unifiedEditor->setReadOnly(true);
    m_unifiedEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_unifiedEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    unifiedLayout->addWidget(m_unifiedEditor, 1);

    unifiedPanel->hide();

    // Set monospace font for all editors
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    m_oldEditor->setFont(font);
    m_newEditor->setFont(font);
    m_oldLineNumbers->setFont(font);
    m_newLineNumbers->setFont(font);
    m_unifiedEditor->setFont(font);
    m_unifiedLineNumbers->setFont(font);

    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    m_oldEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    m_newEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    m_unifiedEditor->setContextMenuPolicy(Qt::CustomContextMenu);
}

void GitDiffView::setupConnections()
{
    connect(m_fileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GitDiffView::onFileSelected);
    connect(m_prevButton, &QToolButton::clicked, this, &GitDiffView::onPreviousChange);
    connect(m_nextButton, &QToolButton::clicked, this, &GitDiffView::onNextChange);

    // Side-by-side scroll sync
    connect(m_oldEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, &GitDiffView::onSyncScroll);
    connect(m_newEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, &GitDiffView::onSyncScroll);
    connect(m_oldLineNumbers->verticalScrollBar(), &QScrollBar::valueChanged, this, &GitDiffView::onSyncScroll);
    connect(m_newLineNumbers->verticalScrollBar(), &QScrollBar::valueChanged, this, &GitDiffView::onSyncScroll);

    // Unified scroll sync
    connect(m_unifiedEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, &GitDiffView::onUnifiedScroll);
    connect(m_unifiedLineNumbers->verticalScrollBar(), &QScrollBar::valueChanged, this, &GitDiffView::onUnifiedScroll);

    // Context menus
    connect(this, &QWidget::customContextMenuRequested, this, &GitDiffView::showContextMenu);
    connect(m_oldEditor, &QWidget::customContextMenuRequested, this, &GitDiffView::showContextMenu);
    connect(m_newEditor, &QWidget::customContextMenuRequested, this, &GitDiffView::showContextMenu);
    connect(m_unifiedEditor, &QWidget::customContextMenuRequested, this, &GitDiffView::showContextMenu);
}

void GitDiffView::applyTheme()
{
    const auto colors = Theme::colors();

    // Base style for editors
    QString editorStyle = QString(
        "QPlainTextEdit {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: none;"
        "    selection-background-color: %3;"
        "}"
        "QPlainTextEdit:focus {"
        "    border: none;"
        "}"
    ).arg(colors.gutterBackground.name(), colors.gutterText.name(), colors.completionSelectionBackground.name());

    QString lineNumberStyle = QString(
        "QPlainTextEdit {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: none;"
        "    padding-right: 4px;"
        "}"
    ).arg(colors.gutterBackground.name(), colors.gutterText.name());

    m_oldEditor->setStyleSheet(editorStyle);
    m_newEditor->setStyleSheet(editorStyle);
    m_unifiedEditor->setStyleSheet(editorStyle);

    m_oldLineNumbers->setStyleSheet(lineNumberStyle);
    m_newLineNumbers->setStyleSheet(lineNumberStyle);
    m_unifiedLineNumbers->setStyleSheet(lineNumberStyle);

    m_toolbar->setStyleSheet(QString(
        "QWidget#GitDiffToolbar {"
        "    background-color: %1;"
        "    border-bottom: 1px solid %2;"
        "}"
        "QToolButton {"
        "    padding: 4px 8px;"
        "    border: 1px solid transparent;"
        "    border-radius: 3px;"
        "}"
        "QToolButton:hover {"
        "    background-color: %3;"
        "    border-color: %4;"
        "}"
        "QToolButton:pressed {"
        "    background-color: %5;"
        "}"
        "QComboBox {"
        "    padding: 4px 8px;"
        "    border: 1px solid %4;"
        "    border-radius: 3px;"
        "    background-color: %1;"
        "    color: %2;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 20px;"
        "}"
    ).arg(colors.gutterBackground.name(),
          colors.gutterText.name(),
          colors.completionSelectionBackground.name(),
          colors.completionBorder.name(),
          colors.completionMatchHighlight.name()));

    // Splitter handle
    m_splitter->setStyleSheet(QString(
        "QSplitter::handle {"
        "    background-color: %1;"
        "    width: 1px;"
        "}"
        "QSplitter::handle:hover {"
        "    background-color: %2;"
        "}"
    ).arg(colors.completionBorder.name(), colors.completionMatchHighlight.name()));

    // Refresh syntax highlighters
    if (m_oldHighlighter) m_oldHighlighter->refreshTheme();
    if (m_newHighlighter) m_newHighlighter->refreshTheme();
    if (m_unifiedHighlighter) m_unifiedHighlighter->refreshTheme();
}

void GitDiffView::clearContent()
{
    m_fileCombo->clear();
    m_changes.clear();
    m_currentChangeIndex = -1;

    m_oldEditor->clear();
    m_newEditor->clear();
    m_oldLineNumbers->clear();
    m_newLineNumbers->clear();
    m_unifiedEditor->clear();
    m_unifiedLineNumbers->clear();

    delete m_oldHighlighter;
    delete m_newHighlighter;
    delete m_unifiedHighlighter;
    m_oldHighlighter = nullptr;
    m_newHighlighter = nullptr;
    m_unifiedHighlighter = nullptr;
}

void GitDiffView::setDiff(const QVector<GitDiffFile>& diffFiles)
{
    m_diffFiles = diffFiles;
    clearContent();

    if (diffFiles.isEmpty()) {
        m_oldEditor->setPlainText(tr("No changes to display."));
        m_newEditor->setPlainText(tr("No changes to display."));
        m_unifiedEditor->setPlainText(tr("No changes to display."));
        return;
    }

    // Populate file combo
    for (const GitDiffFile& diff : diffFiles) {
        QString display = diff.filePath;
        if (diff.status == GitFileStatusType::Renamed) {
            display = QString("%1 → %2").arg(diff.oldFilePath, diff.newFilePath);
        } else if (diff.status == GitFileStatusType::Added) {
            display = QString("+ %1").arg(diff.newFilePath);
        } else if (diff.status == GitFileStatusType::Deleted) {
            display = QString("- %1").arg(diff.oldFilePath);
        }
        m_fileCombo->addItem(display, QVariant::fromValue(diff));
    }

    // Select first file
    if (!diffFiles.isEmpty()) {
        m_fileCombo->setCurrentIndex(0);
        onFileSelected(0);
    }
}

void GitDiffView::setDiffFile(const GitDiffFile& diff)
{
    m_diffFiles.clear();
    m_diffFiles.append(diff);
    setDiff(m_diffFiles);
}

void GitDiffView::setMode(DiffMode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;

    if (mode == DiffMode::SideBySide) {
        m_modeButton->setText(tr("Side-by-side"));
        m_modeButton->setIcon(QIcon::fromTheme("view-split-horizontal", style()->standardIcon(QStyle::SP_FileDialogContentsView)));

        // Show side-by-side panels, hide unified
        m_splitter->widget(0)->show(); // old panel
        m_splitter->widget(1)->show(); // new panel
        m_splitter->widget(2)->hide(); // unified panel

        displaySideBySide(m_currentDiff);
    } else {
        m_modeButton->setText(tr("Unified"));
        m_modeButton->setIcon(QIcon::fromTheme("view-split-vertical", style()->standardIcon(QStyle::SP_FileDialogDetailedView)));

        // Show unified panel, hide side-by-side
        m_splitter->widget(0)->hide();
        m_splitter->widget(1)->hide();
        m_splitter->widget(2)->show();

        displayUnified(m_currentDiff);
    }
}

GitDiffView::DiffMode GitDiffView::mode() const
{
    return m_mode;
}

void GitDiffView::onFileSelected(int index)
{
    if (index < 0 || index >= m_diffFiles.size())
        return;

    m_currentDiff = m_diffFiles.at(index);
    m_changes.clear();
    m_currentChangeIndex = -1;

    if (m_mode == DiffMode::SideBySide) {
        displaySideBySide(m_currentDiff);
    } else {
        displayUnified(m_currentDiff);
    }
}

void GitDiffView::displayDiffFile(const GitDiffFile& diff)
{
    if (m_mode == DiffMode::SideBySide) {
        displaySideBySide(diff);
    } else {
        displayUnified(diff);
    }
}

void GitDiffView::displaySideBySide(const GitDiffFile& diff)
{
    if (diff.isBinary) {
        m_oldEditor->setPlainText(tr("Binary file - cannot display diff."));
        m_newEditor->setPlainText(tr("Binary file - cannot display diff."));
        m_oldLineNumbers->clear();
        m_newLineNumbers->clear();
        return;
    }

    QVector<int> oldLineNumbers;
    QVector<int> newLineNumbers;

    QString oldText = buildSideBySideText(diff, true, oldLineNumbers);
    QString newText = buildSideBySideText(diff, false, newLineNumbers);

    m_oldEditor->setPlainText(oldText);
    m_newEditor->setPlainText(newText);

    updateLineNumbers(m_oldLineNumbers, oldLineNumbers);
    updateLineNumbers(m_newLineNumbers, newLineNumbers);

    // Setup syntax highlighting
    const SyntaxDefinition* def = getSyntaxDefinitionForFile(diff.newFilePath.isEmpty() ? diff.oldFilePath : diff.newFilePath);

    delete m_oldHighlighter;
    delete m_newHighlighter;
    m_oldHighlighter = new SyntaxHighlighter(m_oldEditor->document(), def ? *def : SyntaxDefinition());
    m_newHighlighter = new SyntaxHighlighter(m_newEditor->document(), def ? *def : SyntaxDefinition());

    // Highlight changes
    highlightChanges(m_oldEditor, diff.hunks, true);
    highlightChanges(m_newEditor, diff.hunks, false);

    // Build change positions for navigation
    m_changes.clear();
    int oldBlock = 0, newBlock = 0;

    for (const GitDiffHunk& hunk : diff.hunks) {
        QStringList oldLines = hunk.header.split(' ');
        // Parse @@ -oldStart,oldLines +newStart,newLines @@
        // For simplicity, we track each changed line
        int oldLine = hunk.oldStart;
        int newLine = hunk.newStart;

        for (const QString& line : hunk.lines) {
            if (line.startsWith('-')) {
                // Deletion - only in old
                ChangePosition cp;
                cp.blockNumber = oldBlock;
                cp.lineNumber = oldLine;
                cp.isDeletion = true;
                m_changes.append(cp);
                oldLine++;
                oldBlock++;
            } else if (line.startsWith('+')) {
                // Addition - only in new
                ChangePosition cp;
                cp.blockNumber = newBlock;
                cp.lineNumber = newLine;
                cp.isAddition = true;
                m_changes.append(cp);
                newLine++;
                newBlock++;
            } else {
                // Context line - in both
                oldLine++;
                newLine++;
                oldBlock++;
                newBlock++;
            }
        }
    }

    if (!m_changes.isEmpty()) {
        m_currentChangeIndex = 0;
    }

    updateNavigationButtons();
}

void GitDiffView::displayUnified(const GitDiffFile& diff)
{
    if (diff.isBinary) {
        m_unifiedEditor->setPlainText(tr("Binary file - cannot display diff."));
        m_unifiedLineNumbers->clear();
        return;
    }

    QVector<QPair<int, int>> lineMapping; // (oldLine, newLine) for each displayed line
    QString unifiedText = buildUnifiedText(diff, lineMapping);

    m_unifiedEditor->setPlainText(unifiedText);

    // Build line numbers for unified view
    QVector<int> lineNumbers;
    for (const auto& pair : lineMapping) {
        int displayLine = pair.first > 0 ? pair.first : pair.second;
        lineNumbers.append(displayLine);
    }
    updateLineNumbers(m_unifiedLineNumbers, lineNumbers);

    // Setup syntax highlighting
    const SyntaxDefinition* def = getSyntaxDefinitionForFile(diff.newFilePath.isEmpty() ? diff.oldFilePath : diff.newFilePath);

    delete m_unifiedHighlighter;
    m_unifiedHighlighter = new SyntaxHighlighter(m_unifiedEditor->document(), def ? *def : SyntaxDefinition());

    // Highlight changes
    highlightUnified(m_unifiedEditor, diff.hunks);

    // Build change positions for navigation
    m_changes.clear();
    int blockNumber = 0;

    for (const GitDiffHunk& hunk : diff.hunks) {
        for (const QString& line : hunk.lines) {
            if (line.startsWith('+') || line.startsWith('-')) {
                ChangePosition cp;
                cp.blockNumber = blockNumber;
                cp.lineNumber = line.startsWith('+') ? lineMapping[blockNumber].second : lineMapping[blockNumber].first;
                cp.isAddition = line.startsWith('+');
                cp.isDeletion = line.startsWith('-');
                m_changes.append(cp);
            }
            blockNumber++;
        }
    }

    if (!m_changes.isEmpty()) {
        m_currentChangeIndex = 0;
    }

    updateNavigationButtons();
}

QString GitDiffView::buildSideBySideText(const GitDiffFile& diff, bool isOld, QVector<int>& lineNumbers)
{
    QString result;
    lineNumbers.clear();

    int oldLine = 1;
    int newLine = 1;

    for (const GitDiffHunk& hunk : diff.hunks) {
        // Skip to hunk start
        if (isOld) {
            while (oldLine < hunk.oldStart) {
                result += "\n";
                lineNumbers.append(oldLine);
                oldLine++;
            }
        } else {
            while (newLine < hunk.newStart) {
                result += "\n";
                lineNumbers.append(newLine);
                newLine++;
            }
        }

        for (const QString& line : hunk.lines) {
            if (line.startsWith('-')) {
                if (isOld) {
                    result += line.mid(1) + "\n";
                    lineNumbers.append(oldLine);
                    oldLine++;
                } else {
                    // In new view, show empty line for context alignment
                    result += "\n";
                    lineNumbers.append(0); // No corresponding line
                }
            } else if (line.startsWith('+')) {
                if (!isOld) {
                    result += line.mid(1) + "\n";
                    lineNumbers.append(newLine);
                    newLine++;
                } else {
                    result += "\n";
                    lineNumbers.append(0);
                }
            } else {
                // Context line
                result += line.mid(1) + "\n";
                lineNumbers.append(isOld ? oldLine : newLine);
                oldLine++;
                newLine++;
            }
        }
    }

    // Remove trailing newline
    if (result.endsWith('\n')) {
        result.chop(1);
    }

    return result;
}

QString GitDiffView::buildUnifiedText(const GitDiffFile& diff, QVector<QPair<int, int>>& lineMapping)
{
    QString result;
    lineMapping.clear();

    int oldLine = 1;
    int newLine = 1;

    for (const GitDiffHunk& hunk : diff.hunks) {
        // Add hunk header as comment
        result += hunk.header + "\n";
        lineMapping.append({0, 0}); // Header line, no line number

        // Skip to hunk start
        while (oldLine < hunk.oldStart) {
            result += " " + QString::number(oldLine) + "\n";
            lineMapping.append({oldLine, 0});
            oldLine++;
        }
        while (newLine < hunk.newStart) {
            result += " " + QString::number(newLine) + "\n";
            lineMapping.append({0, newLine});
            newLine++;
        }

        for (const QString& line : hunk.lines) {
            if (line.startsWith('-')) {
                result += line + "\n";
                lineMapping.append({oldLine, 0});
                oldLine++;
            } else if (line.startsWith('+')) {
                result += line + "\n";
                lineMapping.append({0, newLine});
                newLine++;
            } else {
                result += " " + line.mid(1) + "\n";
                lineMapping.append({oldLine, newLine});
                oldLine++;
                newLine++;
            }
        }
    }

    if (result.endsWith('\n')) {
        result.chop(1);
    }

    return result;
}

void GitDiffView::updateLineNumbers(QPlainTextEdit* lineNumberWidget, const QVector<int>& lineNumbers)
{
    QString text;
    for (int num : lineNumbers) {
        if (num > 0) {
            text += QString::number(num) + "\n";
        } else {
            text += "\n"; // Empty line for alignment
        }
    }
    if (!text.isEmpty() && text.endsWith('\n')) {
        text.chop(1);
    }
    lineNumberWidget->setPlainText(text);
}

void GitDiffView::highlightChanges(QPlainTextEdit* editor, const QVector<GitDiffHunk>& hunks, bool isOld)
{
    const auto colors = Theme::colors();

    QTextCharFormat addedFormat;
    addedFormat.setBackground(QColor(0xA5, 0xD6, 0xA7)); // Light green

    QTextCharFormat deletedFormat;
    deletedFormat.setBackground(QColor(0xEF, 0x9A, 0x9A)); // Light red

    QTextCharFormat contextFormat;
    contextFormat.setBackground(Qt::transparent);

    QTextCursor cursor(editor->document());
    int blockNumber = 0;
    int oldLine = 1;
    int newLine = 1;

    for (const GitDiffHunk& hunk : hunks) {
        // Skip to hunk start
        if (isOld) {
            while (oldLine < hunk.oldStart && !cursor.atEnd()) {
                cursor.movePosition(QTextCursor::NextBlock);
                blockNumber++;
                oldLine++;
            }
        } else {
            while (newLine < hunk.newStart && !cursor.atEnd()) {
                cursor.movePosition(QTextCursor::NextBlock);
                blockNumber++;
                newLine++;
            }
        }

        for (const QString& line : hunk.lines) {
            if (cursor.atEnd())
                break;

            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

            if (line.startsWith('-')) {
                if (isOld) {
                    cursor.setCharFormat(deletedFormat);
                } else {
                    cursor.setCharFormat(contextFormat); // Empty line in new view
                }
                if (isOld) oldLine++;
            } else if (line.startsWith('+')) {
                if (!isOld) {
                    cursor.setCharFormat(addedFormat);
                } else {
                    cursor.setCharFormat(contextFormat);
                }
                if (!isOld) newLine++;
            } else {
                cursor.setCharFormat(contextFormat);
                oldLine++;
                newLine++;
            }

            cursor.movePosition(QTextCursor::NextBlock);
            blockNumber++;
        }
    }
}

void GitDiffView::highlightUnified(QPlainTextEdit* editor, const QVector<GitDiffHunk>& hunks)
{
    const auto colors = Theme::colors();

    QTextCharFormat addedFormat;
    addedFormat.setBackground(QColor(0xA5, 0xD6, 0xA7)); // Light green

    QTextCharFormat deletedFormat;
    deletedFormat.setBackground(QColor(0xEF, 0x9A, 0x9A)); // Light red

    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(0x90, 0xA4, 0xAE)); // Blue-gray
    headerFormat.setFontItalic(true);

    QTextCursor cursor(editor->document());
    int blockNumber = 0;

    for (const GitDiffHunk& hunk : hunks) {
        // Hunk header
        if (!cursor.atEnd()) {
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.setCharFormat(headerFormat);
            cursor.movePosition(QTextCursor::NextBlock);
            blockNumber++;
        }

        for (const QString& line : hunk.lines) {
            if (cursor.atEnd())
                break;

            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

            if (line.startsWith('+')) {
                cursor.setCharFormat(addedFormat);
            } else if (line.startsWith('-')) {
                cursor.setCharFormat(deletedFormat);
            } else {
                // Context line - no special formatting
            }

            cursor.movePosition(QTextCursor::NextBlock);
            blockNumber++;
        }
    }
}

const SyntaxDefinition* GitDiffView::getSyntaxDefinitionForFile(const QString& filePath) const
{
    return SyntaxRepository::instance().forFile(filePath);
}

void GitDiffView::onModeChanged()
{
    // Handled by setMode via menu action
}

void GitDiffView::onSyncScroll(int value)
{
    if (m_syncScrolling)
        return;

    m_syncScrolling = true;

    QScrollBar* sender = qobject_cast<QScrollBar*>(QObject::sender());
    QScrollBar* oldScroll = m_oldEditor->verticalScrollBar();
    QScrollBar* newScroll = m_newEditor->verticalScrollBar();
    QScrollBar* oldLineScroll = m_oldLineNumbers->verticalScrollBar();
    QScrollBar* newLineScroll = m_newLineNumbers->verticalScrollBar();

    if (sender == oldScroll || sender == oldLineScroll) {
        newScroll->setValue(value);
        newLineScroll->setValue(value);
    } else if (sender == newScroll || sender == newLineScroll) {
        oldScroll->setValue(value);
        oldLineScroll->setValue(value);
    }

    m_syncScrolling = false;
}

void GitDiffView::onUnifiedScroll(int value)
{
    if (m_syncScrolling)
        return;

    m_syncScrolling = true;

    QScrollBar* sender = qobject_cast<QScrollBar*>(QObject::sender());
    QScrollBar* editorScroll = m_unifiedEditor->verticalScrollBar();
    QScrollBar* lineScroll = m_unifiedLineNumbers->verticalScrollBar();

    if (sender == editorScroll) {
        lineScroll->setValue(value);
    } else if (sender == lineScroll) {
        editorScroll->setValue(value);
    }

    m_syncScrolling = false;
}

void GitDiffView::goToNextChange()
{
    if (m_changes.isEmpty())
        return;

    m_currentChangeIndex = (m_currentChangeIndex + 1) % m_changes.size();
    navigateToChange(m_currentChangeIndex);
}

void GitDiffView::goToPreviousChange()
{
    if (m_changes.isEmpty())
        return;

    m_currentChangeIndex = (m_currentChangeIndex - 1 + m_changes.size()) % m_changes.size();
    navigateToChange(m_currentChangeIndex);
}

void GitDiffView::navigateToChange(int index)
{
    if (index < 0 || index >= m_changes.size())
        return;

    const ChangePosition& change = m_changes.at(index);
    QPlainTextEdit* editor = nullptr;
    QPlainTextEdit* lineNumbers = nullptr;

    if (m_mode == DiffMode::SideBySide) {
        editor = change.isDeletion ? m_oldEditor : m_newEditor;
        lineNumbers = change.isDeletion ? m_oldLineNumbers : m_newLineNumbers;
    } else {
        editor = m_unifiedEditor;
        lineNumbers = m_unifiedLineNumbers;
    }

    if (!editor)
        return;

    QTextBlock block = editor->document()->findBlockByNumber(change.blockNumber);
    if (block.isValid()) {
        QTextCursor cursor(block);
        editor->setTextCursor(cursor);
        editor->ensureCursorVisible();

        // Also scroll line numbers
        QScrollBar* editorScroll = editor->verticalScrollBar();
        QScrollBar* lineScroll = lineNumbers->verticalScrollBar();
        lineScroll->setValue(editorScroll->value());
    }

    updateNavigationButtons();
}

void GitDiffView::onNextChange()
{
    goToNextChange();
}

void GitDiffView::onPreviousChange()
{
    goToPreviousChange();
}

void GitDiffView::updateNavigationButtons()
{
    bool hasChanges = !m_changes.isEmpty();
    m_prevButton->setEnabled(hasChanges);
    m_nextButton->setEnabled(hasChanges);
}

void GitDiffView::openFileAtLine(const QString& filePath, int line)
{
    emit openFileRequested(filePath, line);
}

void GitDiffView::showContextMenu(const QPoint& pos)
{
    QWidget* senderWidget = qobject_cast<QWidget*>(sender());
    QPlainTextEdit* editor = nullptr;

    if (senderWidget == m_oldEditor || senderWidget == m_newEditor || senderWidget == m_unifiedEditor) {
        editor = qobject_cast<QPlainTextEdit*>(senderWidget);
    } else if (senderWidget == this) {
        // Determine which editor is focused
        if (m_oldEditor->hasFocus()) editor = m_oldEditor;
        else if (m_newEditor->hasFocus()) editor = m_newEditor;
        else if (m_unifiedEditor->hasFocus()) editor = m_unifiedEditor;
    }

    QMenu menu(this);

    QAction* copyAction = menu.addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &GitDiffView::onCopy);

    if (editor && editor->textCursor().hasSelection()) {
        // Copy enabled
    } else {
        copyAction->setEnabled(false);
    }

    if (editor && m_currentDiff.filePath.isEmpty() == false) {
        menu.addSeparator();
        QAction* openAction = menu.addAction(tr("Open File at &Line"));
        connect(openAction, &QAction::triggered, this, &GitDiffView::onOpenFileAtCursor);
    }

    menu.exec(senderWidget ? senderWidget->mapToGlobal(pos) : mapToGlobal(pos));
}

void GitDiffView::onCopy()
{
    QWidget* focusedWidget = QApplication::focusWidget();
    QPlainTextEdit* editor = qobject_cast<QPlainTextEdit*>(focusedWidget);

    if (!editor) {
        if (m_oldEditor->hasFocus()) editor = m_oldEditor;
        else if (m_newEditor->hasFocus()) editor = m_newEditor;
        else if (m_unifiedEditor->hasFocus()) editor = m_unifiedEditor;
    }

    if (editor && editor->textCursor().hasSelection()) {
        editor->copy();
    }
}

void GitDiffView::onOpenFileAtCursor()
{
    QWidget* focusedWidget = QApplication::focusWidget();
    QPlainTextEdit* editor = qobject_cast<QPlainTextEdit*>(focusedWidget);

    if (!editor) {
        if (m_oldEditor->hasFocus()) editor = m_oldEditor;
        else if (m_newEditor->hasFocus()) editor = m_newEditor;
        else if (m_unifiedEditor->hasFocus()) editor = m_unifiedEditor;
    }

    if (!editor || m_currentDiff.filePath.isEmpty())
        return;

    QTextCursor cursor = editor->textCursor();
    int blockNumber = cursor.blockNumber();

    // Find the corresponding line in the original file
    int line = -1;
    if (m_mode == DiffMode::SideBySide) {
        // We need to map block number to actual line number
        // This is simplified - in a real implementation we'd track this better
        QTextBlock block = editor->document()->findBlockByNumber(blockNumber);
        if (block.isValid()) {
            // The line numbers are in the line number widget
            QTextBlock lineBlock = m_oldLineNumbers->document()->findBlockByNumber(blockNumber);
            if (lineBlock.isValid()) {
                QString lineText = lineBlock.text().trimmed();
                bool ok = false;
                line = lineText.toInt(&ok);
                if (!ok) line = -1;
            }
        }
    } else {
        // Unified mode - similar approach
        QTextBlock lineBlock = m_unifiedLineNumbers->document()->findBlockByNumber(blockNumber);
        if (lineBlock.isValid()) {
            QString lineText = lineBlock.text().trimmed();
            bool ok = false;
            line = lineText.toInt(&ok);
            if (!ok) line = -1;
        }
    }

    if (line > 0) {
        // Use newFilePath for additions, oldFilePath for deletions
        QString filePath = m_currentDiff.newFilePath.isEmpty() ? m_currentDiff.oldFilePath : m_currentDiff.newFilePath;
        if (!m_currentDiff.oldFilePath.isEmpty() && m_currentDiff.status == GitFileStatusType::Deleted) {
            filePath = m_currentDiff.oldFilePath;
        }
        openFileAtLine(filePath, line);
    }
}

} // namespace papiro