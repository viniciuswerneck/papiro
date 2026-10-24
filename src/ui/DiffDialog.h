#pragma once

#include "core/SyntaxDefinition.h"
#include "editor/SyntaxHighlighter.h"

#include <QDialog>
#include <QVector>
#include <QPair>

class QComboBox;
class QPlainTextEdit;
class QSplitter;
class QToolButton;
class QMenu;
class QPushButton;

namespace papiro {

struct DiffLine {
    enum class Type { Context, Addition, Deletion };
    Type type = Type::Context;
    QString oldLine;  // For side-by-side: left side
    QString newLine;  // For side-by-side: right side
    int oldLineNumber = -1;
    int newLineNumber = -1;
};

struct DiffHunk {
    int oldStart = 0;
    int oldCount = 0;
    int newStart = 0;
    int newCount = 0;
    QVector<DiffLine> lines;
};

struct DiffResult {
    QVector<DiffHunk> hunks;
    QString oldFilePath;
    QString newFilePath;
};

/*! Dialog for comparing two files or text contents. */
class DiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiffDialog(QWidget *parent = nullptr);
    ~DiffDialog() override;

    void setFiles(const QString &oldFilePath, const QString &newFilePath);
    void setTexts(const QString &oldText, const QString &newText,
                  const QString &oldTitle = "Old", const QString &newTitle = "New");

    static DiffResult computeDiff(const QString &oldText, const QString &newText);

private slots:
    void onModeChanged();
    void onNextChange();
    void onPreviousChange();
    void onSwapFiles();
    void onCopy();
    void showContextMenu(const QPoint& pos);

private:
    void setupUI();
    void setupConnections();
    void displayDiff(const DiffResult &diff);
    void displaySideBySide(const DiffResult &diff);
    void displayUnified(const DiffResult &diff);
    void applyTheme();
    void updateLineNumbers(QPlainTextEdit* editor, const QVector<int>& lineNumbers);
    void highlightChanges(QPlainTextEdit* editor, const QVector<DiffHunk>& hunks, bool isOld);
    void highlightUnified(QPlainTextEdit* editor, const QVector<DiffHunk>& hunks);
    QString buildSideBySideText(const DiffResult &diff, bool isOld, QVector<int>& lineNumbers);
    QString buildUnifiedText(const DiffResult &diff, QVector<QPair<int, int>>& lineMapping);
    const SyntaxDefinition* getSyntaxDefinitionForFile(const QString& filePath) const;
    void navigateToChange(int index);

    struct ChangePosition {
        int blockNumber = -1;
        int lineNumber = -1;
        bool isAddition = false;
        bool isDeletion = false;
    };

    // UI components
    QComboBox* m_modeCombo = nullptr;
    QToolButton* m_nextButton = nullptr;
    QToolButton* m_prevButton = nullptr;
    QPushButton* m_swapButton = nullptr;
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
    DiffResult m_diff;
    QString m_oldTitle = "Old";
    QString m_newTitle = "New";
    QVector<ChangePosition> m_changes;
    int m_currentChangeIndex = -1;

    // Syntax highlighting
    SyntaxHighlighter* m_oldHighlighter = nullptr;
    SyntaxHighlighter* m_newHighlighter = nullptr;
    SyntaxHighlighter* m_unifiedHighlighter = nullptr;

    bool m_syncScrolling = false;
};

} // namespace papiro