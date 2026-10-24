#include "ui/LSPPreferencesDialog.h"

#include "core/LSPManager.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace papiro {

namespace {

/*! Dialog for adding/editing a single LSP server configuration. */
class LSPServerEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit LSPServerEditDialog(const LSPServerConfig *existing = nullptr, QWidget *parent = nullptr);
    ~LSPServerEditDialog() override;

    LSPServerConfig config() const;

private:
    void setupUi();
    void fillFromConfig(const LSPServerConfig &config);

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_languageIdEdit = nullptr;
    QLineEdit *m_commandEdit = nullptr;
    QLineEdit *m_argumentsEdit = nullptr;
    QLineEdit *m_workingDirEdit = nullptr;
    QLineEdit *m_envEdit = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QLineEdit *m_extensionsEdit = nullptr;
    QDialogButtonBox *m_buttons = nullptr;

    bool m_isEditing = false;
};

LSPServerEditDialog::LSPServerEditDialog(const LSPServerConfig *existing, QWidget *parent)
    : QDialog(parent)
    , m_isEditing(existing != nullptr)
{
    setWindowTitle(m_isEditing ? tr("Edit LSP Server") : tr("Add LSP Server"));
    setModal(true);
    resize(500, 450);
    setupUi();

    if (existing) {
        fillFromConfig(*existing);
        m_languageIdEdit->setReadOnly(true); // Language ID is the key, don't allow changing
    }
}

LSPServerEditDialog::~LSPServerEditDialog() = default;

void LSPServerEditDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Display name (e.g., clangd, pyright)"));
    form->addRow(tr("&Name:"), m_nameEdit);

    m_languageIdEdit = new QLineEdit(this);
    m_languageIdEdit->setPlaceholderText(tr("Language ID (e.g., cpp, python, typescript)"));
    form->addRow(tr("Language &ID:"), m_languageIdEdit);

    m_commandEdit = new QLineEdit(this);
    m_commandEdit->setPlaceholderText(tr("Executable name or full path"));
    form->addRow(tr("&Command:"), m_commandEdit);

    m_argumentsEdit = new QLineEdit(this);
    m_argumentsEdit->setPlaceholderText(tr("Arguments separated by spaces (e.g., --background-index --clang-tidy)"));
    form->addRow(tr("&Arguments:"), m_argumentsEdit);

    m_workingDirEdit = new QLineEdit(this);
    m_workingDirEdit->setPlaceholderText(tr("Working directory (empty = project root)"));
    form->addRow(tr("Working &Directory:"), m_workingDirEdit);

    m_envEdit = new QLineEdit(this);
    m_envEdit->setPlaceholderText(tr("Environment variables as KEY=VALUE pairs separated by semicolons"));
    form->addRow(tr("Environment &Variables:"), m_envEdit);

    m_enabledCheck = new QCheckBox(tr("&Enabled"), this);
    m_enabledCheck->setChecked(true);
    form->addRow(m_enabledCheck);

    m_extensionsEdit = new QLineEdit(this);
    m_extensionsEdit->setPlaceholderText(tr("Comma-separated (e.g., .cpp,.h,.hpp)"));
    form->addRow(tr("File &Extensions:"), m_extensionsEdit);

    layout->addLayout(form);

    // Help text
    auto *helpLabel = new QLabel(
        tr("Language ID must match the LSP specification (e.g., 'cpp' for C/C++, 'python' for Python, 'typescript' for TypeScript). "
           "The server executable must be in PATH or provide a full path. "
           "Changes take effect for newly opened files."),
        this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("QLabel { color: gray; font-size: 11px; padding: 8px; }");
    layout->addWidget(helpLabel);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttons);
}

void LSPServerEditDialog::fillFromConfig(const LSPServerConfig &config)
{
    m_nameEdit->setText(config.name);
    m_languageIdEdit->setText(config.languageId);
    m_commandEdit->setText(config.command);
    m_argumentsEdit->setText(config.arguments.join(' '));
    m_workingDirEdit->setText(config.workingDirectory);

    QStringList envPairs;
    for (auto it = config.env.constBegin(); it != config.env.constEnd(); ++it) {
        envPairs.append(it.key() + "=" + it.value());
    }
    m_envEdit->setText(envPairs.join(';'));

    m_enabledCheck->setChecked(config.enabled);
    m_extensionsEdit->setText(config.fileExtensions);
}

LSPServerConfig LSPServerEditDialog::config() const
{
    LSPServerConfig config;
    config.name = m_nameEdit->text().trimmed();
    config.languageId = m_languageIdEdit->text().trimmed().toLower();
    config.command = m_commandEdit->text().trimmed();

    // Parse arguments (split by spaces, respecting quotes)
    QString argsText = m_argumentsEdit->text().trimmed();
    config.arguments.clear();
    if (!argsText.isEmpty()) {
        bool inQuotes = false;
        QString current;
        for (QChar ch : argsText) {
            if (ch == '"' || ch == '\'') {
                inQuotes = !inQuotes;
            } else if (ch.isSpace() && !inQuotes) {
                if (!current.isEmpty()) {
                    config.arguments.append(current);
                    current.clear();
                }
            } else {
                current.append(ch);
            }
        }
        if (!current.isEmpty()) {
            config.arguments.append(current);
        }
    }

    config.workingDirectory = m_workingDirEdit->text().trimmed();

    config.env.clear();
    QString envText = m_envEdit->text().trimmed();
    if (!envText.isEmpty()) {
        QStringList pairs = envText.split(';', Qt::SkipEmptyParts);
        for (const QString &pair : pairs) {
            QStringList kv = pair.split('=', Qt::KeepEmptyParts);
            if (kv.size() == 2) {
                config.env[kv[0].trimmed()] = kv[1].trimmed();
            }
        }
    }

    config.enabled = m_enabledCheck->isChecked();
    config.fileExtensions = m_extensionsEdit->text().trimmed();

    return config;
}

} // anonymous namespace

// ============================================================================
// LSPPreferencesDialog Implementation
// ============================================================================

LSPPreferencesDialog::LSPPreferencesDialog(const QMap<QString, LSPServerConfig> &currentConfigs, QWidget *parent)
    : QDialog(parent)
    , m_configs(currentConfigs)
{
    setWindowTitle(tr("LSP Servers"));
    setModal(true);
    resize(700, 500);
    setupUi();
    populateTree();
}

LSPPreferencesDialog::~LSPPreferencesDialog() = default;

void LSPPreferencesDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    m_addButton = new QPushButton(tr("&Add..."), this);
    m_editButton = new QPushButton(tr("&Edit..."), this);
    m_removeButton = new QPushButton(tr("&Remove"), this);

    m_editButton->setEnabled(false);
    m_removeButton->setEnabled(false);

    connect(m_addButton, &QPushButton::clicked, this, &LSPPreferencesDialog::onAddServer);
    connect(m_editButton, &QPushButton::clicked, this, &LSPPreferencesDialog::onEditServer);
    connect(m_removeButton, &QPushButton::clicked, this, &LSPPreferencesDialog::onRemoveServer);

    toolbar->addWidget(m_addButton);
    toolbar->addWidget(m_editButton);
    toolbar->addWidget(m_removeButton);
    toolbar->addStretch();

    layout->addLayout(toolbar);

    // Tree widget
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("LSPServerTree"));
    m_tree->setHeaderLabels({tr("Language"), tr("Name"), tr("Command"), tr("Enabled"), tr("Extensions")});
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &LSPPreferencesDialog::updateButtonStates);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &LSPPreferencesDialog::onItemDoubleClicked);

    layout->addWidget(m_tree, 1);

    // Buttons
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this]() {
        // Recreate default configs
        m_configs = LSPManager::defaultServerConfigs();
        populateTree();
    });
    layout->addWidget(m_buttons);
}

void LSPPreferencesDialog::populateTree()
{
    m_tree->clear();

    for (auto it = m_configs.constBegin(); it != m_configs.constEnd(); ++it) {
        const LSPServerConfig &config = it.value();

        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, config.languageId);
        item->setText(1, config.name);
        item->setText(2, config.command + (config.arguments.isEmpty() ? "" : " " + config.arguments.join(' ')));
        item->setText(3, config.enabled ? tr("Yes") : tr("No"));
        item->setText(4, config.fileExtensions);
        item->setData(0, Qt::UserRole, config.languageId);

        if (!config.enabled) {
            item->setForeground(0, QBrush(Qt::gray));
            item->setForeground(1, QBrush(Qt::gray));
            item->setForeground(2, QBrush(Qt::gray));
            item->setForeground(3, QBrush(Qt::gray));
            item->setForeground(4, QBrush(Qt::gray));
        }
    }
}

void LSPPreferencesDialog::onAddServer()
{
    LSPServerEditDialog dialog(nullptr, this);
    if (dialog.exec() == QDialog::Accepted) {
        LSPServerConfig config = dialog.config();
        if (config.languageId.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Configuration"), tr("Language ID cannot be empty."));
            return;
        }
        if (config.command.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Configuration"), tr("Command cannot be empty."));
            return;
        }
        if (m_configs.contains(config.languageId)) {
            QMessageBox::warning(this, tr("Duplicate Language ID"),
                                 tr("A server for language '%1' already exists.").arg(config.languageId));
            return;
        }
        m_configs[config.languageId] = config;
        populateTree();
    }
}

void LSPPreferencesDialog::onEditServer()
{
    auto *item = m_tree->currentItem();
    if (!item)
        return;

    const QString langId = item->data(0, Qt::UserRole).toString();
    auto it = m_configs.find(langId);
    if (it == m_configs.end())
        return;

    LSPServerEditDialog dialog(&it.value(), this);
    if (dialog.exec() == QDialog::Accepted) {
        LSPServerConfig config = dialog.config();
        if (config.languageId.isEmpty() || config.command.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Configuration"), tr("Language ID and Command cannot be empty."));
            return;
        }
        it.value() = config;
        populateTree();
    }
}

void LSPPreferencesDialog::onRemoveServer()
{
    auto *item = m_tree->currentItem();
    if (!item)
        return;

    const QString langId = item->data(0, Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("Remove LSP Server"),
                              tr("Remove the LSP server configuration for '%1'?").arg(langId),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_configs.remove(langId);
        populateTree();
    }
}

void LSPPreferencesDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    onEditServer();
}

void LSPPreferencesDialog::updateButtonStates()
{
    bool hasSelection = m_tree->currentItem() != nullptr;
    m_editButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
}

QMap<QString, LSPServerConfig> LSPPreferencesDialog::serverConfigs() const
{
    return m_configs;
}

} // namespace papiro

#include "ui/LSPPreferencesDialog.moc"