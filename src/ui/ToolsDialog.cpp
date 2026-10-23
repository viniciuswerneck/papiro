#include "ui/ToolsDialog.h"

#include "core/ExternalTool.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace papiro {

ToolsDialog::ToolsDialog(ToolManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_list(new QListWidget(this))
    , m_name(new QLineEdit(this))
    , m_command(new QLineEdit(this))
    , m_arguments(new QLineEdit(this))
    , m_workingDirectory(new QLineEdit(this))
    , m_saveBeforeRun(new QCheckBox(tr("Save the current file before running"), this))
{
    setWindowTitle(tr("External Tools"));
    resize(620, 460);

    m_list->setMinimumWidth(180);
    auto *addButton = new QPushButton(tr("Add"), this);
    auto *removeButton = new QPushButton(tr("Remove"), this);
    auto *applyButton = new QPushButton(tr("Apply"), this);

    auto *listButtons = new QVBoxLayout;
    listButtons->addWidget(addButton);
    listButtons->addWidget(removeButton);
    listButtons->addStretch();
    listButtons->addWidget(applyButton);

    auto *listColumn = new QVBoxLayout;
    listColumn->addWidget(m_list, 1);
    listColumn->addLayout(listButtons);

    m_command->setPlaceholderText(tr("Executable, e.g. g++"));
    m_arguments->setPlaceholderText(tr("Arguments, e.g. -O2 $FILE -o $FILENAME.out"));
    m_workingDirectory->setPlaceholderText(tr("Working directory (empty: folder of the file)"));

    auto *form = new QFormLayout;
    form->addRow(tr("&Name:"), m_name);
    form->addRow(tr("&Command:"), m_command);
    form->addRow(tr("&Arguments:"), m_arguments);
    form->addRow(tr("&Working directory:"), m_workingDirectory);
    form->addRow(QString(), m_saveBeforeRun);

    auto *hint = new QLabel(
        tr("Variables: <b>$FILE</b> full path, <b>$FILEDIR</b> folder, "
           "<b>$FILENAME</b> name without extension, <b>$FILEEXT</b> extension."),
        this);
    hint->setWordWrap(true);

    auto *right = new QVBoxLayout;
    right->addLayout(form);
    right->addWidget(hint);
    right->addStretch();

    auto *columns = new QHBoxLayout;
    columns->addLayout(listColumn);
    columns->addLayout(right, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(columns, 1);
    layout->addWidget(buttons);

    connect(addButton, &QPushButton::clicked, this, &ToolsDialog::addTool);
    connect(removeButton, &QPushButton::clicked, this, &ToolsDialog::removeTool);
    connect(applyButton, &QPushButton::clicked, this, &ToolsDialog::applyChanges);
    connect(m_list, &QListWidget::currentRowChanged, this, &ToolsDialog::selectionChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        applyChanges();
        m_manager->save();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    const QList<ExternalTool> &tools = m_manager->tools();
    if (tools.isEmpty()) {
        m_list->setEnabled(true);
    }
    rebuildList(tools.isEmpty() ? -1 : 0);
}

void ToolsDialog::rebuildList(int selectIndex)
{
    m_updating = true;
    m_list->clear();
    const QList<ExternalTool> &tools = m_manager->tools();
    for (const ExternalTool &tool : tools)
        m_list->addItem(tool.name);
    m_updating = false;

    if (selectIndex >= 0 && selectIndex < m_list->count())
        m_list->setCurrentRow(selectIndex);
    else if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    selectionChanged();
}

int ToolsDialog::currentIndex() const
{
    return m_list->currentRow();
}

void ToolsDialog::populateFrom(int index)
{
    m_updating = true;
    const QList<ExternalTool> &tools = m_manager->tools();
    if (index >= 0 && index < tools.size()) {
        const ExternalTool &tool = tools.at(index);
        m_name->setText(tool.name);
        m_command->setText(tool.command);
        m_arguments->setText(tool.arguments);
        m_workingDirectory->setText(tool.workingDirectory);
        m_saveBeforeRun->setChecked(tool.saveBeforeRun);
        m_name->setEnabled(true);
        m_command->setEnabled(true);
        m_arguments->setEnabled(true);
        m_workingDirectory->setEnabled(true);
        m_saveBeforeRun->setEnabled(true);
    } else {
        m_name->clear();
        m_command->clear();
        m_arguments->clear();
        m_workingDirectory->clear();
        m_saveBeforeRun->setChecked(true);
        m_name->setEnabled(false);
        m_command->setEnabled(false);
        m_arguments->setEnabled(false);
        m_workingDirectory->setEnabled(false);
        m_saveBeforeRun->setEnabled(false);
    }
    m_updating = false;
}

void ToolsDialog::selectionChanged()
{
    if (m_updating)
        return;
    populateFrom(currentIndex());
}

void ToolsDialog::addTool()
{
    ExternalTool tool;
    tool.name = tr("New Tool");
    m_manager->addTool(tool);
    rebuildList(m_manager->tools().size() - 1);
    m_name->setFocus();
    m_name->selectAll();
}

void ToolsDialog::removeTool()
{
    const int index = currentIndex();
    if (index < 0)
        return;
    m_manager->removeTool(index);
    rebuildList(qMin(index, m_manager->tools().size() - 1));
}

void ToolsDialog::applyChanges()
{
    const int index = currentIndex();
    if (index < 0)
        return;
    ExternalTool tool;
    tool.name = m_name->text().trimmed();
    tool.command = m_command->text().trimmed();
    tool.arguments = m_arguments->text();
    tool.workingDirectory = m_workingDirectory->text().trimmed();
    tool.saveBeforeRun = m_saveBeforeRun->isChecked();
    m_manager->replaceTool(index, tool);

    m_updating = true;
    if (QListWidgetItem *item = m_list->item(index))
        item->setText(tool.name.isEmpty() ? tr("(unnamed)") : tool.name);
    m_updating = false;
}

} // namespace papiro
