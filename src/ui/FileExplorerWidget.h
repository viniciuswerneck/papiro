#pragma once

#include <QWidget>

class QFileSystemModel;
class QLineEdit;
class QTreeView;

namespace papiro {

/*! Simple folder tree used as a project/file explorer dock. */
class FileExplorerWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileExplorerWidget(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    QString rootPath() const;

    void setFilterText(const QString &text);

signals:
    void fileActivated(const QString &path);

private slots:
    void handleDoubleClick(const QModelIndex &index);
    void showContextMenu(const QPoint &position);

private:
    QFileSystemModel *m_model = nullptr;
    QTreeView *m_tree = nullptr;
    QLineEdit *m_filterEdit = nullptr;
};

} // namespace papiro
