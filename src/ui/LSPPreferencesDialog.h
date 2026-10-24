#pragma once

#include "core/LSPManager.h"

#include <QDialog>
#include <QMap>
#include <QVector>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
class QDialogButtonBox;
class QGroupBox;
class QComboBox;
class QCheckBox;
class QFormLayout;

namespace papiro {

/*! Configuration for a single LSP server in the preferences dialog. */
struct LSPServerPrefConfig {
    QString name;
    QString languageId;
    QString command;
    QStringList arguments;
    QString workingDirectory;
    QMap<QString, QString> env;
    bool enabled = true;
    QString fileExtensions;
};

/*! Dialog for configuring Language Server Protocol (LSP) servers. */
class LSPPreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit LSPPreferencesDialog(const QMap<QString, LSPServerConfig> &currentConfigs, QWidget *parent = nullptr);
    ~LSPPreferencesDialog() override;

    QMap<QString, LSPServerConfig> serverConfigs() const;

private:
    void setupUi();
    void populateTree();
    void onAddServer();
    void onEditServer();
    void onRemoveServer();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void updateButtonStates();

    QTreeWidget *m_tree = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QDialogButtonBox *m_buttons = nullptr;

    QMap<QString, LSPServerConfig> m_configs;
};

} // namespace papiro