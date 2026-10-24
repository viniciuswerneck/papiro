#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QListWidget;

namespace papiro {

class ToolManager;

/*! Editor for the list of external tools. */
class ToolsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ToolsDialog(ToolManager *manager, QWidget *parent = nullptr);

private slots:
    void addTool();
    void removeTool();
    void applyChanges();
    void selectionChanged();

private:
    void rebuildList(int selectIndex = -1);
    void populateFrom(int index);
    int currentIndex() const;

    ToolManager *m_manager = nullptr;
    QListWidget *m_list = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_command = nullptr;
    QLineEdit *m_arguments = nullptr;
    QLineEdit *m_workingDirectory = nullptr;
    QCheckBox *m_saveBeforeRun = nullptr;
    QLineEdit *m_errorPattern = nullptr;
    QLineEdit *m_errorFilePattern = nullptr;
    QLineEdit *m_errorLinePattern = nullptr;
    QLineEdit *m_errorColumnPattern = nullptr;
    bool m_updating = false;
};

} // namespace papiro
