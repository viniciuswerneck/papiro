#include "ui/MacroManagerDialog.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace papiro {

MacroManagerDialog::MacroManagerDialog(MacroManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Manage Macros"));
    setModal(true);
    resize(720, 420);

    m_macroList = new QListWidget(this);
    m_macroList->setObjectName(QStringLiteral("MacroList"));
    m_macroList->setSelectionMode(QAbstractItemView::SingleSelection);

    m_renameButton = new QPushButton(tr("&Rename..."), this);
    m_renameButton->setObjectName(QStringLiteral("RenameMacroButton"));
    m_deleteButton = new QPushButton(tr("&Delete"), this);
    m_deleteButton->setObjectName(QStringLiteral("DeleteMacroButton"));
    m_playButton = new QPushButton(tr("&Play"), this);
    m_playButton->setObjectName(QStringLiteral("PlayMacroButton"));
    m_editShortcutButton = new QPushButton(tr("Edit &Shortcut..."), this);
    m_editShortcutButton->setObjectName(QStringLiteral("EditMacroShortcutButton"));

    auto *macroButtons = new QHBoxLayout;
    macroButtons->addWidget(m_renameButton);
    macroButtons->addWidget(m_deleteButton);
    macroButtons->addStretch();
    macroButtons->addWidget(m_playButton);
    macroButtons->addWidget(m_editShortcutButton);

    auto *macroLayout = new QVBoxLayout;
    macroLayout->addWidget(m_macroList);
    macroLayout->addLayout(macroButtons);

    auto *macroGroup = new QGroupBox(tr("User Macros"), this);
    macroGroup->setLayout(macroLayout);

    m_slotList = new QListWidget(this);
    m_slotList->setObjectName(QStringLiteral("MacroSlotList"));
    m_slotList->setSelectionMode(QAbstractItemView::SingleSelection);

    m_assignButton = new QPushButton(tr("&Assign to Slot"), this);
    m_assignButton->setObjectName(QStringLiteral("AssignMacroButton"));
    m_clearSlotButton = new QPushButton(tr("&Clear Slot"), this);
    m_clearSlotButton->setObjectName(QStringLiteral("ClearMacroSlotButton"));

    auto *slotButtons = new QHBoxLayout;
    slotButtons->addWidget(m_assignButton);
    slotButtons->addWidget(m_clearSlotButton);
    slotButtons->addStretch();

    auto *slotLayout = new QVBoxLayout;
    slotLayout->addWidget(new QLabel(tr("Shortcut slots: Ctrl+Shift+1 through Ctrl+Shift+9"), this));
    slotLayout->addWidget(m_slotList);
    slotLayout->addLayout(slotButtons);

    auto *slotGroup = new QGroupBox(tr("Shortcut Slots"), this);
    slotGroup->setLayout(slotLayout);

    auto *lists = new QHBoxLayout;
    lists->addWidget(macroGroup, 1);
    lists->addWidget(slotGroup, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(lists, 1);
    layout->addWidget(buttons);

    connect(m_macroList, &QListWidget::currentRowChanged, this,
            &MacroManagerDialog::updateButtons);
    connect(m_slotList, &QListWidget::currentRowChanged, this,
            &MacroManagerDialog::updateButtons);
    connect(m_macroList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { playSelected(); });
    connect(m_slotList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) {
                const int slot = selectedSlot();
                if (slot >= 0) {
                    if (Macro *macro = m_manager->builtinMacro(slot))
                        emit playRequested(macro);
                }
            });
    connect(m_renameButton, &QPushButton::clicked, this, &MacroManagerDialog::renameSelected);
    connect(m_deleteButton, &QPushButton::clicked, this, &MacroManagerDialog::deleteSelected);
    connect(m_playButton, &QPushButton::clicked, this, &MacroManagerDialog::playSelected);
    connect(m_assignButton, &QPushButton::clicked, this, &MacroManagerDialog::assignSelected);
    connect(m_clearSlotButton, &QPushButton::clicked, this,
            &MacroManagerDialog::clearSelectedSlot);
    connect(m_editShortcutButton, &QPushButton::clicked, this, &MacroManagerDialog::editShortcut);

    refresh();
}

void MacroManagerDialog::refresh()
{
    if (!m_manager)
        return;

    Macro *selected = selectedMacro();
    const int selectedSlotIndex = selectedSlot();

    m_macroList->clear();
    for (Macro *macro : m_manager->userMacros()) {
        const QString name = macro->name().isEmpty() ? tr("(unnamed)") : macro->name();
        QString shortcutStr = m_manager->shortcutForMacro(macro).toString(QKeySequence::PortableText);
        QString displayText = tr("%1 (%2 actions)").arg(name).arg(macro->actions().size());
        if (!shortcutStr.isEmpty())
            displayText += tr(" — %1").arg(shortcutStr);
        auto *item = new QListWidgetItem(displayText, m_macroList);
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void *>(macro)));
        if (macro == selected)
            m_macroList->setCurrentItem(item);
    }

    m_slotList->clear();
    for (int i = 0; i < MacroManager::MaxBuiltinMacros; ++i) {
        Macro *macro = m_manager->builtinMacro(i);
        QString text;
        if (macro) {
            const QString name = macro->name().isEmpty() ? tr("(unnamed)") : macro->name();
            text = tr("%1. Ctrl+Shift+%2 — %3 (%4 actions)")
                       .arg(i + 1)
                       .arg(i + 1)
                       .arg(name)
                       .arg(macro->actions().size());
        } else {
            text = tr("%1. Ctrl+Shift+%2 — (empty)").arg(i + 1).arg(i + 1);
        }
        auto *item = new QListWidgetItem(text, m_slotList);
        item->setData(Qt::UserRole, i);
    }
    if (selectedSlotIndex >= 0 && selectedSlotIndex < m_slotList->count())
        m_slotList->setCurrentRow(selectedSlotIndex);

    updateButtons();
}

void MacroManagerDialog::updateButtons()
{
    Macro *macro = selectedMacro();
    const int slot = selectedSlot();
    m_renameButton->setEnabled(macro != nullptr);
    m_deleteButton->setEnabled(macro != nullptr);
    m_playButton->setEnabled(macro != nullptr);
    m_assignButton->setEnabled(macro != nullptr && slot >= 0);
    m_clearSlotButton->setEnabled(slot >= 0);
    m_editShortcutButton->setEnabled(macro != nullptr);
}

Macro *MacroManagerDialog::selectedMacro() const
{
    if (!m_macroList || !m_macroList->currentItem())
        return nullptr;
    return static_cast<Macro *>(
        m_macroList->currentItem()->data(Qt::UserRole).value<void *>());
}

int MacroManagerDialog::selectedSlot() const
{
    if (!m_slotList || m_slotList->currentRow() < 0)
        return -1;
    return m_slotList->currentRow();
}

void MacroManagerDialog::renameSelected()
{
    Macro *macro = selectedMacro();
    if (!macro)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Macro"), tr("Macro name:"),
                                               QLineEdit::Normal, macro->name(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;

    for (Macro *other : m_manager->userMacros()) {
        if (other != macro && other->name() == name) {
            QMessageBox::warning(this, tr("Rename Macro"),
                                 tr("A macro named \"%1\" already exists.").arg(name));
            return;
        }
    }

    macro->setName(name);
    save();
    emit macrosChanged();
    refresh();
}

void MacroManagerDialog::deleteSelected()
{
    Macro *macro = selectedMacro();
    if (!macro)
        return;

    const QString name = macro->name().isEmpty() ? tr("(unnamed)") : macro->name();
    const auto answer = QMessageBox::question(
        this, tr("Delete Macro"),
        tr("Delete macro \"%1\"? It will also be removed from its shortcut slots.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    m_manager->removeUserMacro(macro);
    save();
    emit macrosChanged();
    refresh();
}

void MacroManagerDialog::assignSelected()
{
    Macro *macro = selectedMacro();
    const int slot = selectedSlot();
    if (!macro || slot < 0)
        return;

    m_manager->setBuiltinMacro(slot, macro);
    save();
    emit macrosChanged();
    refresh();
}

void MacroManagerDialog::clearSelectedSlot()
{
    const int slot = selectedSlot();
    if (slot < 0)
        return;

    m_manager->clearBuiltinMacro(slot);
    save();
    emit macrosChanged();
    refresh();
}

void MacroManagerDialog::playSelected()
{
    if (Macro *macro = selectedMacro())
        emit playRequested(macro);
}

void MacroManagerDialog::save()
{
    if (!m_manager)
        return;
    if (!m_manager->saveToDirectory(MacroManager::defaultDirectory())) {
        QMessageBox::warning(this, tr("Manage Macros"),
                             tr("The macros could not be saved to disk."));
    }
}

void MacroManagerDialog::editShortcut()
{
    Macro *macro = selectedMacro();
    if (!macro)
        return;

    QKeySequence current = m_manager->shortcutForMacro(macro);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Shortcut for \"%1\"").arg(macro->name()));
    auto *layout = new QVBoxLayout(&dlg);

    auto *label = new QLabel(tr("Press the desired key combination:"), &dlg);
    layout->addWidget(label);

    auto *kse = new QKeySequenceEdit(current, &dlg);
    kse->setClearButtonEnabled(true);
    layout->addWidget(kse);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        QKeySequence newShortcut = kse->keySequence();

        // Validação de conflito
        if (!newShortcut.isEmpty() && !m_manager->isShortcutAvailable(newShortcut, macro)) {
            QString conflict = m_manager->shortcutConflictDescription(newShortcut);
            QMessageBox::warning(this, tr("Shortcut Conflict"),
                tr("The shortcut \"%1\" is already used by %2.\nPlease choose a different combination.")
                    .arg(newShortcut.toString(QKeySequence::PortableText), conflict));
            return;
        }

        m_manager->setShortcutForMacro(macro, newShortcut);
        save();
        emit macrosChanged();
        refresh();
    }
}

} // namespace papiro
