#pragma once

#include "core/Macro.h"

#include <QDialog>

class QListWidget;
class QPushButton;
class QKeySequenceEdit;

namespace papiro {

/*! Dialog to rename/delete macros and assign them to the Ctrl+Shift+1..9 slots. */
class MacroManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit MacroManagerDialog(MacroManager *manager, QWidget *parent = nullptr);

signals:
    void playRequested(papiro::Macro *macro);
    void macrosChanged();

private:
    void refresh();
    void updateButtons();
    Macro *selectedMacro() const;
    int selectedSlot() const;
    void renameSelected();
    void deleteSelected();
    void assignSelected();
    void clearSelectedSlot();
    void playSelected();
    void editShortcut();
    void save();

    MacroManager *m_manager = nullptr;
    QListWidget *m_macroList = nullptr;
    QListWidget *m_slotList = nullptr;
    QPushButton *m_renameButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_assignButton = nullptr;
    QPushButton *m_clearSlotButton = nullptr;
    QPushButton *m_editShortcutButton = nullptr;
};

} // namespace papiro
