#include "ui/KeyboardShortcutsDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace papiro {

// ---------------------------------------------------------------------------
// Custom model for shortcuts
// ---------------------------------------------------------------------------

class ShortcutsTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ShortcutsTableModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent) {}

    enum Column {
        CategoryColumn = 0,
        ActionColumn = 1,
        ShortcutColumn = 2,
        DescriptionColumn = 3,
        ColumnCount
    };

    void setShortcuts(const QVector<ShortcutInfo>& shortcuts) {
        beginResetModel();
        m_shortcuts = shortcuts;
        endResetModel();
    }

    const QVector<ShortcutInfo>& shortcuts() const { return m_shortcuts; }
    QVector<ShortcutInfo>& shortcuts() { return m_shortcuts; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return m_shortcuts.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return ColumnCount;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_shortcuts.size())
            return QVariant();

        const ShortcutInfo &info = m_shortcuts[index.row()];

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (index.column()) {
                case CategoryColumn:
                    return info.category;
                case ActionColumn:
                    return info.actionName;
                case ShortcutColumn:
                    return formatShortcut(info.shortcut);
                case DescriptionColumn:
                    return info.description;
            }
        } else if (role == Qt::ToolTipRole) {
            if (index.column() == ShortcutColumn) {
                return tr("Shortcut: %1").arg(formatShortcut(info.shortcut));
            }
            return info.description;
        } else if (role == Qt::TextAlignmentRole) {
            if (index.column() == ShortcutColumn)
                return int(Qt::AlignCenter);
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        } else if (role == Qt::FontRole && index.column() == ShortcutColumn) {
            QFont font;
            font.setFamily("Monospace");
            font.setStyleHint(QFont::Monospace);
            return font;
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            switch (section) {
                case CategoryColumn: return tr("Category");
                case ActionColumn: return tr("Action");
                case ShortcutColumn: return tr("Shortcut");
                case DescriptionColumn: return tr("Description");
            }
        }
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        if (!index.isValid())
            return Qt::NoItemFlags;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    // For sorting support
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override {
        beginResetModel();
        m_sortColumn = column;
        m_sortOrder = order;
        std::sort(m_shortcuts.begin(), m_shortcuts.end(),
                  [this](const ShortcutInfo &a, const ShortcutInfo &b) {
                      bool result = false;
                      switch (m_sortColumn) {
                          case CategoryColumn:
                              result = a.category < b.category;
                              break;
                          case ActionColumn:
                              result = a.actionName < b.actionName;
                              break;
                          case ShortcutColumn:
                              result = formatShortcut(a.shortcut) < formatShortcut(b.shortcut);
                              break;
                          case DescriptionColumn:
                              result = a.description < b.description;
                              break;
                      }
                      return m_sortOrder == Qt::AscendingOrder ? result : !result;
                  });
        endResetModel();
    }

    static QString formatShortcut(const QKeySequence &seq) {
        if (seq.isEmpty())
            return QString();
        return seq.toString(QKeySequence::PortableText);
    }

private:
    QVector<ShortcutInfo> m_shortcuts;
    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

// ---------------------------------------------------------------------------
// Delegate for rendering shortcuts with proper formatting
// ---------------------------------------------------------------------------

class ShortcutDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ShortcutDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        if (index.column() == ShortcutsTableModel::ShortcutColumn) {
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);

            // Draw background
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

            // Draw shortcut with monospace font
            QString text = index.data(Qt::DisplayRole).toString();
            if (!text.isEmpty()) {
                QRect textRect = opt.rect.adjusted(4, 0, -4, 0);
                QFont font = opt.font;
                font.setFamily("Monospace");
                font.setStyleHint(QFont::Monospace);
                painter->setFont(font);
                painter->setPen(opt.palette.color(QPalette::Text));
                painter->drawText(textRect, Qt::AlignCenter | Qt::AlignVCenter, text);
            }
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
};

// ---------------------------------------------------------------------------
// Custom proxy model for filtering
// ---------------------------------------------------------------------------

class ShortcutsFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit ShortcutsFilterProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setFilterText(const QString &text) {
        m_filterText = text.toLower();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
        if (m_filterText.isEmpty())
            return true;

        QModelIndex categoryIndex = sourceModel()->index(sourceRow, ShortcutsTableModel::CategoryColumn, sourceParent);
        QModelIndex actionIndex = sourceModel()->index(sourceRow, ShortcutsTableModel::ActionColumn, sourceParent);
        QModelIndex shortcutIndex = sourceModel()->index(sourceRow, ShortcutsTableModel::ShortcutColumn, sourceParent);
        QModelIndex descIndex = sourceModel()->index(sourceRow, ShortcutsTableModel::DescriptionColumn, sourceParent);

        QString category = sourceModel()->data(categoryIndex).toString().toLower();
        QString action = sourceModel()->data(actionIndex).toString().toLower();
        QString shortcut = sourceModel()->data(shortcutIndex).toString().toLower();
        QString description = sourceModel()->data(descIndex).toString().toLower();

        return category.contains(m_filterText) ||
               action.contains(m_filterText) ||
               shortcut.contains(m_filterText) ||
               description.contains(m_filterText);
    }

private:
    QString m_filterText;
};

// ---------------------------------------------------------------------------
// KeyboardShortcutsDialog implementation
// ---------------------------------------------------------------------------

KeyboardShortcutsDialog::KeyboardShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    setModal(true);
    resize(800, 600);
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    setupUi();
    setupModel();
    applyTheme();

    setShortcuts(getAllShortcuts());
}

KeyboardShortcutsDialog::~KeyboardShortcutsDialog() = default;

void KeyboardShortcutsDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search shortcuts... (action name or key combination)"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setObjectName(QStringLiteral("ShortcutSearchEdit"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_proxyModel)
            static_cast<ShortcutsFilterProxyModel*>(m_proxyModel)->setFilterText(text);
    });
    mainLayout->addWidget(m_searchEdit);

    // Table view
    m_tableView = new QTableView(this);
    m_tableView->setObjectName(QStringLiteral("ShortcutsTableView"));
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setSortingEnabled(true);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionsClickable(true);
    m_tableView->horizontalHeader()->setHighlightSections(false);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setShowGrid(false);
    m_tableView->setItemDelegateForColumn(ShortcutsTableModel::ShortcutColumn, new ShortcutDelegate(m_tableView));

    // Set column widths
    m_tableView->horizontalHeader()->setSectionResizeMode(ShortcutsTableModel::CategoryColumn, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(ShortcutsTableModel::ActionColumn, QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(ShortcutsTableModel::ShortcutColumn, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(ShortcutsTableModel::DescriptionColumn, QHeaderView::Stretch);

    // Set minimum width for shortcut column
    m_tableView->horizontalHeader()->setMinimumSectionSize(120);

    mainLayout->addWidget(m_tableView, 1);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("StatusLabel"));
    m_statusLabel->setMinimumHeight(20);
    m_statusLabel->setTextFormat(Qt::PlainText);
    mainLayout->addWidget(m_statusLabel);

    // Button bar
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(8);

    m_copyButton = new QPushButton(tr("&Copy"), this);
    m_copyButton->setObjectName(QStringLiteral("CopyShortcutButton"));
    m_copyButton->setToolTip(tr("Copy selected shortcut (Ctrl+C)"));
    connect(m_copyButton, &QPushButton::clicked, this, &KeyboardShortcutsDialog::copySelected);
    buttonLayout->addWidget(m_copyButton);

    buttonLayout->addStretch();

    m_exportCsvButton = new QPushButton(tr("Export &CSV"), this);
    m_exportCsvButton->setObjectName(QStringLiteral("ExportCsvButton"));
    connect(m_exportCsvButton, &QPushButton::clicked, this, &KeyboardShortcutsDialog::exportToCsv);
    buttonLayout->addWidget(m_exportCsvButton);

    m_exportMdButton = new QPushButton(tr("Export &Markdown"), this);
    m_exportMdButton->setObjectName(QStringLiteral("ExportMdButton"));
    connect(m_exportMdButton, &QPushButton::clicked, this, &KeyboardShortcutsDialog::exportToMarkdown);
    buttonLayout->addWidget(m_exportMdButton);

    buttonLayout->addStretch();

    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setObjectName(QStringLiteral("CloseShortcutsButton"));
    m_closeButton->setDefault(true);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    // Shortcut for copy
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &KeyboardShortcutsDialog::copySelected);

    // Focus search on open
    connect(this, &QDialog::finished, this, [this] {
        m_searchEdit->clear();
    });
}

void KeyboardShortcutsDialog::setupModel() {
    m_model = new ShortcutsTableModel(this);
    m_proxyModel = new ShortcutsFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1); // Search all columns
    m_proxyModel->setDynamicSortFilter(true);

    m_tableView->setModel(m_proxyModel);
    m_tableView->sortByColumn(ShortcutsTableModel::CategoryColumn, Qt::AscendingOrder);
}

void KeyboardShortcutsDialog::applyTheme() {
    const auto colors = Theme::colors(Theme::mode());
    const bool isDark = Theme::mode() == Theme::Mode::Dark;

    QString styleSheet = QString(
        "KeyboardShortcutsDialog {"
        "    background-color: %1;"
        "}"
        "QLineEdit#ShortcutSearchEdit {"
        "    padding: 6px 10px;"
        "    border: 1px solid %2;"
        "    border-radius: 4px;"
        "    background-color: %3;"
        "    color: %4;"
        "    selection-background-color: %5;"
        "}"
        "QLineEdit#ShortcutSearchEdit:focus {"
        "    border-color: %5;"
        "}"
        "QTableView#ShortcutsTableView {"
        "    background-color: %3;"
        "    alternate-background-color: %6;"
        "    color: %4;"
        "    gridline-color: %2;"
        "    border: 1px solid %2;"
        "    border-radius: 4px;"
        "    selection-background-color: %5;"
        "    selection-color: %7;"
        "}"
        "QHeaderView::section {"
        "    background-color: %6;"
        "    color: %4;"
        "    padding: 6px 8px;"
        "    border: none;"
        "    border-bottom: 1px solid %2;"
        "    border-right: 1px solid %2;"
        "    font-weight: bold;"
        "}"
        "QHeaderView::section:pressed {"
        "    background-color: %5;"
        "}"
        "QPushButton {"
        "    padding: 6px 14px;"
        "    border: 1px solid %2;"
        "    border-radius: 4px;"
        "    background-color: %6;"
        "    color: %4;"
        "}"
        "QPushButton:hover {"
        "    background-color: %5;"
        "    border-color: %5;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %5;"
        "}"
        "QPushButton:default {"
        "    border-color: %5;"
        "    font-weight: bold;"
        "}"
        "QScrollBar:vertical {"
        "    background: %1;"
        "    width: 10px;"
        "    border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: %8;"
        "    min-height: 30px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: %9;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0;"
        "}"
        "QLabel#StatusLabel {"
        "    color: %4;"
        "    padding: 4px 8px;"
        "    background-color: %6;"
        "    border: 1px solid %2;"
        "    border-radius: 4px;"
        "}"
    ).arg(
        colors.gutterBackground.name(),      // 1 - dialog background
        colors.completionBorder.name(),      // 2 - border color
        colors.completionBackground.name(),  // 3 - base background
        colors.completionText.name(),        // 4 - text color
        colors.completionSelectionBackground.name(), // 5 - highlight/accent
        colors.gutterBackground.name(),      // 6 - alternate base
        colors.completionSelectionText.name(),       // 7 - highlighted text
        colors.scrollbarHandle.name(),       // 8 - scrollbar handle
        colors.scrollbarHandleHover.name()   // 9 - scrollbar handle hover
    );

    setStyleSheet(styleSheet);
}

QVector<ShortcutInfo> KeyboardShortcutsDialog::getAllShortcuts() {
    QVector<ShortcutInfo> shortcuts;

    // File
    shortcuts.append({tr("File"), tr("New"), QKeySequence::New, tr("Create a new document")});
    shortcuts.append({tr("File"), tr("Open..."), QKeySequence::Open, tr("Open an existing file")});
    shortcuts.append({tr("File"), tr("Save"), QKeySequence::Save, tr("Save the current document")});
    shortcuts.append({tr("File"), tr("Save As..."), QKeySequence::SaveAs, tr("Save the current document with a new name")});
    shortcuts.append({tr("File"), tr("Save All"), QKeySequence(), tr("Save all open documents")});
    shortcuts.append({tr("File"), tr("Close Tab"), QKeySequence::Close, tr("Close the current tab")});
    shortcuts.append({tr("File"), tr("Print..."), QKeySequence::Print, tr("Print the current document")});
    shortcuts.append({tr("File"), tr("Print Preview..."), QKeySequence(), tr("Preview the document before printing")});
    shortcuts.append({tr("File"), tr("Exit"), QKeySequence::Quit, tr("Quit the application")});

    // Edit
    shortcuts.append({tr("Edit"), tr("Undo"), QKeySequence::Undo, tr("Undo the last action")});
    shortcuts.append({tr("Edit"), tr("Redo"), QKeySequence::Redo, tr("Redo the last undone action")});
    shortcuts.append({tr("Edit"), tr("Cut"), QKeySequence::Cut, tr("Cut the selection to clipboard")});
    shortcuts.append({tr("Edit"), tr("Copy"), QKeySequence::Copy, tr("Copy the selection to clipboard")});
    shortcuts.append({tr("Edit"), tr("Paste"), QKeySequence::Paste, tr("Paste from clipboard")});
    shortcuts.append({tr("Edit"), tr("Select All"), QKeySequence::SelectAll, tr("Select the entire document")});
    shortcuts.append({tr("Edit"), tr("Duplicate Line"), QKeySequence(Qt::CTRL | Qt::Key_D), tr("Duplicate the current line")});
    shortcuts.append({tr("Edit"), tr("Delete Line"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K), tr("Delete the current line")});
    shortcuts.append({tr("Edit"), tr("Go to Line..."), QKeySequence(Qt::CTRL | Qt::Key_G), tr("Navigate to a specific line number")});
    shortcuts.append({tr("Edit"), tr("Preferences..."), QKeySequence(Qt::CTRL | Qt::Key_Comma), tr("Open preferences dialog")});

    // Search
    shortcuts.append({tr("Search"), tr("Find / Replace..."), QKeySequence::Find, tr("Open find and replace dialog")});
    shortcuts.append({tr("Search"), tr("Find Next"), QKeySequence(Qt::Key_F3), tr("Find the next occurrence")});
    shortcuts.append({tr("Search"), tr("Find Previous"), QKeySequence(Qt::SHIFT | Qt::Key_F3), tr("Find the previous occurrence")});
    shortcuts.append({tr("Search"), tr("Replace..."), QKeySequence(Qt::CTRL | Qt::Key_H), tr("Open replace dialog")});

    // View
    shortcuts.append({tr("View"), tr("Word Wrap"), QKeySequence(), tr("Toggle word wrapping")});
    shortcuts.append({tr("View"), tr("Line Numbers"), QKeySequence(), tr("Toggle line number display")});
    shortcuts.append({tr("View"), tr("Spell Check"), QKeySequence(Qt::Key_F7), tr("Toggle spell checking")});
    shortcuts.append({tr("View"), tr("Zoom In"), QKeySequence::ZoomIn, tr("Increase font size")});
    shortcuts.append({tr("View"), tr("Zoom Out"), QKeySequence::ZoomOut, tr("Decrease font size")});
    shortcuts.append({tr("View"), tr("Reset Zoom"), QKeySequence(Qt::CTRL | Qt::Key_0), tr("Reset font size to default")});
    shortcuts.append({tr("View"), tr("Font..."), QKeySequence(), tr("Change editor font")});
    shortcuts.append({tr("View"), tr("File Explorer"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E), tr("Toggle file explorer panel")});
    shortcuts.append({tr("View"), tr("Clip Library"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L), tr("Toggle clip library panel")});
    shortcuts.append({tr("View"), tr("Output"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O), tr("Toggle output panel")});
    shortcuts.append({tr("View"), tr("Search Results"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F), tr("Toggle search results panel")});
    shortcuts.append({tr("View"), tr("Language"), QKeySequence(), tr("Select document language")});
    shortcuts.append({tr("View"), tr("Theme"), QKeySequence(), tr("Select application theme")});

    // Git
    shortcuts.append({tr("Git"), tr("Commit..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C), tr("Commit staged changes")});
    shortcuts.append({tr("Git"), tr("Push"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P), tr("Push commits to remote")});
    shortcuts.append({tr("Git"), tr("Pull"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L), tr("Pull changes from remote")});
    shortcuts.append({tr("Git"), tr("Branch..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B), tr("Create or switch branches")});
    shortcuts.append({tr("Git"), tr("Log..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G), tr("View commit history")});
    shortcuts.append({tr("Git"), tr("Blame"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B), tr("Show git blame annotations")});
    shortcuts.append({tr("Git"), tr("Diff"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D), tr("Show diff view")});

    // Macros
    shortcuts.append({tr("Macros"), tr("Start Recording"), QKeySequence(Qt::CTRL | Qt::Key_F9), tr("Start recording a macro")});
    shortcuts.append({tr("Macros"), tr("Stop Recording"), QKeySequence(Qt::CTRL | Qt::Key_F10), tr("Stop recording a macro")});
    shortcuts.append({tr("Macros"), tr("Play Macro"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9), tr("Play the recorded macro")});
    shortcuts.append({tr("Macros"), tr("Manage Macros..."), QKeySequence(), tr("Open macro manager dialog")});
    for (int i = 1; i <= 9; ++i) {
        shortcuts.append({tr("Macros"),
                          tr("Macro Slot %1").arg(i),
                          QKeySequence(Qt::CTRL | Qt::SHIFT | (Qt::Key_0 + i)),
                          tr("Play macro assigned to slot %1").arg(i)});
    }

    // LSP
    shortcuts.append({tr("LSP"), tr("Go to Definition"), QKeySequence(Qt::Key_F12), tr("Go to symbol definition")});
    shortcuts.append({tr("LSP"), tr("Find References"), QKeySequence(Qt::SHIFT | Qt::Key_F12), tr("Find all references to symbol")});
    shortcuts.append({tr("LSP"), tr("Rename Symbol"), QKeySequence(Qt::Key_F2), tr("Rename symbol across project")});
    shortcuts.append({tr("LSP"), tr("Trigger Completion"), QKeySequence(Qt::CTRL | Qt::Key_Space), tr("Trigger code completion")});
    shortcuts.append({tr("LSP"), tr("Show Hover"), QKeySequence(Qt::CTRL | Qt::Key_H), tr("Show hover information")});

    // Folding
    shortcuts.append({tr("Folding"), tr("Toggle Fold"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Period), tr("Toggle fold at cursor")});
    shortcuts.append({tr("Folding"), tr("Fold All"), QKeySequence(), tr("Fold all regions")});
    shortcuts.append({tr("Folding"), tr("Unfold All"), QKeySequence(), tr("Unfold all regions")});

    return shortcuts;
}

void KeyboardShortcutsDialog::setShortcuts(const QVector<ShortcutInfo>& shortcuts) {
    m_shortcuts = shortcuts;
    if (m_model) {
        static_cast<ShortcutsTableModel*>(m_model)->setShortcuts(shortcuts);
    }
}

void KeyboardShortcutsDialog::copySelected() {
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    // Map to source model
    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    if (!sourceIndex.isValid())
        return;

    const ShortcutsTableModel *model = static_cast<const ShortcutsTableModel*>(m_model);
    const ShortcutInfo &info = model->shortcuts()[sourceIndex.row()];

    QString text = QString("%1\t%2\t%3\t%4")
        .arg(info.category)
        .arg(info.actionName)
        .arg(ShortcutsTableModel::formatShortcut(info.shortcut))
        .arg(info.description);

    QApplication::clipboard()->setText(text);
    showStatusMessage(tr("Copied to clipboard"), 2000);
}

void KeyboardShortcutsDialog::exportToCsv() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Shortcuts to CSV"),
                                                     QString(), tr("CSV Files (*.csv);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not open file for writing: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Write header
    out << "Category,Action,Shortcut,Description\n";

    const ShortcutsTableModel *model = static_cast<const ShortcutsTableModel*>(m_model);
    for (const ShortcutInfo &info : model->shortcuts()) {
        QString shortcutStr = ShortcutsTableModel::formatShortcut(info.shortcut);
        // Escape commas and quotes for CSV
        auto escapeCsv = [](const QString &s) -> QString {
            QString result = s;
            result.replace("\"", "\"\"");
            if (result.contains(',') || result.contains('"') || result.contains('\n'))
                result = "\"" + result + "\"";
            return result;
        };

        out << escapeCsv(info.category) << ','
            << escapeCsv(info.actionName) << ','
            << escapeCsv(shortcutStr) << ','
            << escapeCsv(info.description) << '\n';
    }

    file.close();
    showStatusMessage(tr("Exported to CSV: %1").arg(fileName), 3000);
}

void KeyboardShortcutsDialog::exportToMarkdown() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Shortcuts to Markdown"),
                                                     QString(), tr("Markdown Files (*.md);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not open file for writing: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    const ShortcutsTableModel *model = static_cast<const ShortcutsTableModel*>(m_model);
    const QVector<ShortcutInfo> &shortcuts = model->shortcuts();

    // Group by category
    QString currentCategory;
    for (const ShortcutInfo &info : shortcuts) {
        if (info.category != currentCategory) {
            currentCategory = info.category;
            if (!currentCategory.isEmpty()) {
                out << "\n## " << currentCategory << "\n\n";
                out << "| Action | Shortcut | Description |\n";
                out << "|--------|----------|-------------|\n";
            }
        }

        QString shortcutStr = ShortcutsTableModel::formatShortcut(info.shortcut);
        if (shortcutStr.isEmpty())
            shortcutStr = "—";

        // Escape pipes in markdown
        auto escapeMd = [](const QString &s) -> QString {
            QString result = s;
            result.replace("|", "\\|");
            return result;
        };

        out << "| " << escapeMd(info.actionName) << " | `"
            << escapeMd(shortcutStr) << "` | "
            << escapeMd(info.description) << " |\n";
    }

    file.close();
    showStatusMessage(tr("Exported to Markdown: %1").arg(fileName), 3000);
}

void KeyboardShortcutsDialog::showStatusMessage(const QString &message, int timeout) {
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(message);
    if (timeout > 0) {
        QTimer::singleShot(timeout, this, [this]() {
            if (m_statusLabel)
                m_statusLabel->clear();
        });
    }
}

} // namespace papiro

#include "KeyboardShortcutsDialog.moc"