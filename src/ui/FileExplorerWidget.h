#pragma once

#include "core/GitRepo.h"
#include "core/GitTypes.h"

#include <QWidget>
#include <QFileSystemModel>
#include <QMap>
#include <QVector>

class QLineEdit;
class QTreeView;

namespace papiro {

/*! Custom file system model that shows Git status decorators. */
class GitFileSystemModel : public QFileSystemModel {
    Q_OBJECT
public:
    explicit GitFileSystemModel(GitRepo* repo, QObject* parent = nullptr);
    
    QVariant data(const QModelIndex& index, int role) const override;
    void setGitStatus(const QVector<GitFileStatus>& status);
    void setRepo(GitRepo* repo);

private:
    GitRepo* m_repo = nullptr;
    QMap<QString, GitFileStatus> m_statusMap;
};

/*! Simple folder tree used as a project/file explorer dock. */
class FileExplorerWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileExplorerWidget(QWidget *parent = nullptr);
    ~FileExplorerWidget() override;

    void setRootPath(const QString &path);
    QString rootPath() const;

    void setFilterText(const QString &text);
    void setGitRepo(GitRepo* repo);

signals:
    void fileActivated(const QString &path);
    void findInFolderRequested(const QString &path);
    void diffRequested(const QString &relativePath);
    void rootPathChanged(const QString &path);

private slots:
    void handleDoubleClick(const QModelIndex &index);
    void showContextMenu(const QPoint &position);
    void onGitRepositoryChanged();

private:
    GitFileSystemModel *m_model = nullptr;
    QTreeView *m_tree = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    GitRepo* m_gitRepo = nullptr;
};

} // namespace papiro
