#pragma once

#include "core/GitTypes.h"
#include "core/SyntaxDefinition.h"
#include "editor/SyntaxHighlighter.h"

#include <QWidget>
#include <QVector>

class QComboBox;
class QPlainTextEdit;
class QSplitter;
class QToolButton;
class QMenu;

namespace papiro {

/*! Git Diff Viewer widget - supports side-by-side and unified diff modes. */
class GitDiffView : public QWidget
{
    Q_OBJECT

public:
    enum class DiffMode { SideBySide, Unified };

    explicit GitDiffView(QWidget* parent = nullptr);
    ~GitDiffView() override;

    void setDiff(const QVector<GitDiffFile>& diffFiles);
    void setDiffFile(const GitDiffFile& diff);
    void setMode(DiffMode mode);
    DiffMode mode() const;

    void goToNextChange();
    void goToPreviousChange();
    void openFileAtLine(const QString& filePath, int line);

signals:
    void openFileRequested(const QString& filePath, int line);

private slots:
    void onFileSelected(int index);
    void onModeChanged();
    void onNextChange();
    void onPreviousChange();
    void onSyncScroll(int value);
    void onUnifiedScroll(int value);
    void showContextMenu(const QPoint& pos);
    void onCopy();
    void onOpenFileAtCursor();
    void navigateToChange(int index);
    void updateNavigationButtons();

private:
    void setupUI();
    void setupConnections();
    void clearContent();
    void displayDiffFile(const GitDiffFile& diff);
    void displaySideBySide(const GitDiffFile& diff);
    void displayUnified(const GitDiffFile& diff);
    void applyTheme();
    void updateLineNumbers(QPlainTextEdit* editor, const QVector<int>& lineNumbers);
    void highlightChanges(QPlainTextEdit* editor, const QVector<GitDiffHunk>& hunks, bool isOld);
    void highlightUnified(QPlainTextEdit* editor, const QVector<GitDiffHunk>& hunks);
    QString buildSideBySideText(const GitDiffFile& diff, bool isOld, QVector<int>& lineNumbers);
    QString buildUnifiedText(const GitDiffFile& diff, QVector<QPair<int, int>>& lineMapping);
    const SyntaxDefinition* getSyntaxDefinitionForFile(const QString& filePath) const;

    struct ChangePosition {
        int blockNumber = -1;
        int lineNumber = -1;
        bool isAddition = false;
        bool isDeletion = false;
    };

    // UI components
    QComboBox* m_fileCombo = nullptr;
    QToolButton* m_modeButton = nullptr;
    QToolButton* m_nextButton = nullptr;
    QToolButton* m_prevButton = nullptr;
    QWidget* m_toolbar = nullptr;

    // Side-by-side mode
    QSplitter* m_splitter = nullptr;
    QPlainTextEdit* m_oldEditor = nullptr;
    QPlainTextEdit* m_newEditor = nullptr;
    QPlainTextEdit* m_oldLineNumbers = nullptr;
    QPlainTextEdit* m_newLineNumbers = nullptr;

    // Unified mode
    QPlainTextEdit* m_unifiedEditor = nullptr;
    QPlainTextEdit* m_unifiedLineNumbers = nullptr;

    // Data
    QVector<GitDiffFile> m_diffFiles;
    GitDiffFile m_currentDiff;
    DiffMode m_mode = DiffMode::SideBySide;
    QVector<ChangePosition> m_changes;
    int m_currentChangeIndex = -1;

    // Syntax highlighting
    SyntaxHighlighter* m_oldHighlighter = nullptr;
    SyntaxHighlighter* m_newHighlighter = nullptr;
    SyntaxHighlighter* m_unifiedHighlighter = nullptr;

    // Line number widgets need special handling for scroll sync
    bool m_syncScrolling = false;
};

} // namespace papiro