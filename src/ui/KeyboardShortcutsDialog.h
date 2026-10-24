#pragma once

#include "ui/Theme.h"

#include <QDialog>
#include <QKeySequence>
#include <QVector>

class QLineEdit;
class QTableView;
class QPushButton;
class QLabel;
class QAbstractTableModel;
class QSortFilterProxyModel;

namespace papiro {

// Shortcut info structure used by the dialog
struct ShortcutInfo {
    QString category;        // "File", "Edit", "Search", "View", "Git", "Macros", "LSP", "Folding", "Preferences"
    QString actionName;      // "New", "Open", "Save", "Undo", etc.
    QKeySequence shortcut;   // Qt::CTRL + Qt::Key_S
    QString description;     // Optional description
};

/*! Dialog to view and search all keyboard shortcuts in the editor. */
class KeyboardShortcutsDialog : public QDialog {
    Q_OBJECT

public:
    explicit KeyboardShortcutsDialog(QWidget *parent = nullptr);
    ~KeyboardShortcutsDialog() override;

    static QVector<ShortcutInfo> getAllShortcuts();
    void setShortcuts(const QVector<ShortcutInfo>& shortcuts);

private:
    void setupUi();
    void setupModel();
    void applyTheme();
    void exportToCsv();
    void exportToMarkdown();
    void copySelected();
    void showStatusMessage(const QString &message, int timeout = 2000);

    QLineEdit *m_searchEdit = nullptr;
    QTableView *m_tableView = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_exportCsvButton = nullptr;
    QPushButton *m_exportMdButton = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    QAbstractTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;
    QVector<ShortcutInfo> m_shortcuts;
};

} // namespace papiro