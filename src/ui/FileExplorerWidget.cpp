#include "ui/FileExplorerWidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

namespace papiro {

FileExplorerWidget::FileExplorerWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new QFileSystemModel(this))
    , m_tree(new QTreeView(this))
    , m_filterEdit(new QLineEdit(this))
{
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_model->setReadOnly(true);
    m_model->setResolveSymlinks(true);

    m_filterEdit->setPlaceholderText(tr("Filter files..."));
    m_filterEdit->setClearButtonEnabled(true);

    m_tree->setModel(m_model);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::AscendingOrder);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    for (int column = 1; column < m_model->columnCount(); ++column)
        m_tree->hideColumn(column);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_filterEdit);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeView::doubleClicked, this, &FileExplorerWidget::handleDoubleClick);
    connect(m_tree, &QTreeView::customContextMenuRequested, this,
            &FileExplorerWidget::showContextMenu);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FileExplorerWidget::setFilterText);
}

void FileExplorerWidget::setRootPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir())
        return;
    const QString absolute = info.absoluteFilePath();
    m_tree->setRootIndex(m_model->setRootPath(absolute));
    m_tree->setCurrentIndex(m_tree->rootIndex());
    m_tree->expand(m_tree->rootIndex());
}

QString FileExplorerWidget::rootPath() const
{
    return m_model->rootPath();
}

void FileExplorerWidget::setFilterText(const QString &text)
{
    // QFileSystemModel name filters are case-insensitive on most platforms.
    if (text.trimmed().isEmpty()) {
        m_model->setNameFilters(QStringList());
        m_model->setNameFilterDisables(false);
        return;
    }
    m_model->setNameFilters(QStringList{QStringLiteral("*%1*").arg(text.trimmed())});
    m_model->setNameFilterDisables(false);
}

void FileExplorerWidget::handleDoubleClick(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    const QFileInfo info = m_model->fileInfo(index);
    if (info.isFile())
        emit fileActivated(info.absoluteFilePath());
}

void FileExplorerWidget::showContextMenu(const QPoint &position)
{
    const QModelIndex index = m_tree->indexAt(position);
    const QFileInfo info = index.isValid() ? m_model->fileInfo(index)
                                           : QFileInfo(m_model->rootPath());
    if (!info.exists())
        return;

    QMenu menu(this);
    if (info.isFile()) {
        QAction *open = menu.addAction(tr("Open"));
        connect(open, &QAction::triggered, this, [this, info] {
            emit fileActivated(info.absoluteFilePath());
        });
    }
    QAction *copyPath = menu.addAction(tr("Copy Path"));
    connect(copyPath, &QAction::triggered, this, [info] {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(info.absoluteFilePath()));
    });
    QAction *findInFolder = menu.addAction(tr("Find in &Folder..."));
    connect(findInFolder, &QAction::triggered, this, [this, info] {
        emit findInFolderRequested(info.isDir() ? info.absoluteFilePath()
                                                : info.absolutePath());
    });
    menu.exec(m_tree->viewport()->mapToGlobal(position));
}

} // namespace papiro
