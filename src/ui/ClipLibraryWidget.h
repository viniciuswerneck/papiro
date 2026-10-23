#pragma once

#include <QWidget>

class QTreeView;
class QLineEdit;
class QComboBox;
class QPushButton;
class QMenu;
class QStandardItemModel;

namespace papiro {

class ClipLibrary;
class Editor;
struct Clip;

/*! Dockable widget for managing and inserting clips/snippets. */
class ClipLibraryWidget : public QWidget {
    Q_OBJECT

public:
    explicit ClipLibraryWidget(ClipLibrary *library, QWidget *parent = nullptr);
    ~ClipLibraryWidget() override = default;

    void setEditor(Editor *editor);

signals:
    void clipInserted(const QString &clipId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onCategoryChanged(int index);
    void onNewClip();
    void onEditClip();
    void onDeleteClip();
    void onInsertClip();
    void onTreeDoubleClicked(const QModelIndex &index);
    void onCustomContextMenu(const QPoint &pos);
    void refreshModel();

private:
    void setupUi();
    void populateModel(const QString &filter = QString(), const QString &category = QString());
    Clip *currentClip() const;

    ClipLibrary *m_library = nullptr;
    Editor *m_editor = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_categoryCombo = nullptr;
    QTreeView *m_treeView = nullptr;
    QStandardItemModel *m_model = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QPushButton *m_insertBtn = nullptr;

    QMenu *m_contextMenu = nullptr;
};

} // namespace papiro