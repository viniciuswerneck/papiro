#include "ui/FileExplorerWidget.h"

#include "core/GitRepo.h"
#include "core/GitTypes.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QStyle>
#include <QTreeView>
#include <QVBoxLayout>

namespace papiro {

// ============================================================================
// GitFileSystemModel Implementation
// ============================================================================

GitFileSystemModel::GitFileSystemModel(GitRepo* repo, QObject* parent)
    : QFileSystemModel(parent)
    , m_repo(repo)
{
    setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    setReadOnly(true);
    setResolveSymlinks(true);
}

void GitFileSystemModel::setRepo(GitRepo* repo)
{
    m_repo = repo;
}

void GitFileSystemModel::setGitStatus(const QVector<GitFileStatus>& status)
{
    m_statusMap.clear();
    for (const GitFileStatus &s : status) {
        m_statusMap[s.filePath] = s;
    }
    // Trigger a refresh of the view
    layoutChanged();
}

QVariant GitFileSystemModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    // Get the file path relative to the repo root
    QString path = QFileSystemModel::filePath(index);
    QFileInfo info(path);
    
    // Check if we have git status for this file
    QString relativePath;
    if (m_repo && m_repo->isValid()) {
        relativePath = m_repo->relativeToRepo(path);
    }
    
    GitFileStatus status;
    bool hasStatus = false;
    if (!relativePath.isEmpty() && m_statusMap.contains(relativePath)) {
        status = m_statusMap[relativePath];
        hasStatus = true;
    }
    
    // Determine the combined status for display
    GitFileStatusType displayStatus = GitFileStatusType::Unmodified;
    if (hasStatus) {
        // Priority: staged > unstaged > untracked > ignored > unmerged
        if (status.isStaged()) {
            displayStatus = status.indexStatus;
        } else if (status.isUnstaged()) {
            displayStatus = status.workTreeStatus;
        }
    }
    
    switch (role) {
        case Qt::DecorationRole: {
            // Return custom icon based on git status
            if (hasStatus && displayStatus != GitFileStatusType::Unmodified) {
                QIcon icon;
                QColor color;
                QString symbol;
                
                switch (displayStatus) {
                    case GitFileStatusType::Modified:
                        color = QColor("#ffaa00"); // Yellow
                        symbol = "●";
                        break;
                    case GitFileStatusType::Added:
                        color = QColor("#4caf50"); // Green
                        symbol = "+";
                        break;
                    case GitFileStatusType::Deleted:
                        color = QColor("#f44336"); // Red
                        symbol = "×";
                        break;
                    case GitFileStatusType::Untracked:
                        color = QColor("#9e9e9e"); // Gray
                        symbol = "?";
                        break;
                    case GitFileStatusType::Ignored:
                        color = QColor(Qt::transparent);
                        symbol = "";
                        break;
                    case GitFileStatusType::Unmerged:
                        color = QColor("#f44336"); // Red
                        symbol = "!";
                        break;
                    case GitFileStatusType::Renamed:
                        color = QColor("#2196f3"); // Blue
                        symbol = "→";
                        break;
                    case GitFileStatusType::Copied:
                        color = QColor("#9c27b0"); // Purple
                        symbol = "⎘";
                        break;
                    case GitFileStatusType::TypeChange:
                        color = QColor("#ff9800"); // Orange
                        symbol = "T";
                        break;
                    default:
                        break;
                }
                
                if (!symbol.isEmpty() && color != Qt::transparent) {
                    // Create a small pixmap with the symbol
                    QPixmap pm(16, 16);
                    pm.fill(Qt::transparent);
                    QPainter painter(&pm);
                    painter.setPen(color);
                    painter.setFont(QFont("Noto Sans", 10, QFont::Bold));
                    painter.drawText(pm.rect(), Qt::AlignCenter, symbol);
                    painter.end();
                    icon = QIcon(pm);
                    return icon;
                }
            }
            // Fall back to default file/folder icon
            return QFileSystemModel::data(index, role);
        }
        
        case Qt::ToolTipRole: {
            QString tooltip = QFileSystemModel::data(index, Qt::ToolTipRole).toString();
            if (hasStatus) {
                QString statusStr;
                if (status.isStaged()) {
                    statusStr += tr("Staged: %1\n").arg(gitStatusTypeToString(status.indexStatus));
                }
                if (status.isUnstaged()) {
                    statusStr += tr("Unstaged: %1\n").arg(gitStatusTypeToString(status.workTreeStatus));
                }
                if (!status.renamedFrom.isEmpty()) {
                    statusStr += tr("Renamed from: %1\n").arg(status.renamedFrom);
                }
                if (!tooltip.isEmpty()) {
                    tooltip += "\n\n";
                }
                tooltip += tr("Git Status:\n%1").arg(statusStr.trimmed());
            }
            return tooltip;
        }
        
        case Qt::ForegroundRole: {
            // Color the filename based on status
            if (hasStatus && displayStatus != GitFileStatusType::Unmodified) {
                QColor color;
                switch (displayStatus) {
                    case GitFileStatusType::Modified:
                        color = QColor("#ffaa00");
                        break;
                    case GitFileStatusType::Added:
                        color = QColor("#4caf50");
                        break;
                    case GitFileStatusType::Deleted:
                        color = QColor("#f44336");
                        break;
                    case GitFileStatusType::Untracked:
                        color = QColor("#9e9e9e");
                        break;
                    case GitFileStatusType::Ignored:
                        color = QColor("#757575");
                        break;
                    case GitFileStatusType::Unmerged:
                        color = QColor("#f44336");
                        break;
                    case GitFileStatusType::Renamed:
                        color = QColor("#2196f3");
                        break;
                    case GitFileStatusType::Copied:
                        color = QColor("#9c27b0");
                        break;
                    case GitFileStatusType::TypeChange:
                        color = QColor("#ff9800");
                        break;
                    default:
                        break;
                }
                if (color.isValid()) {
                    return QBrush(color);
                }
            }
            return QFileSystemModel::data(index, role);
        }
        
        default:
            return QFileSystemModel::data(index, role);
    }
}

// ============================================================================
// FileExplorerWidget Implementation
// ============================================================================

FileExplorerWidget::FileExplorerWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new GitFileSystemModel(nullptr, this))
    , m_tree(new QTreeView(this))
    , m_filterEdit(new QLineEdit(this))
{
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

FileExplorerWidget::~FileExplorerWidget()
{
    // m_model is parented to this, so it will be deleted automatically
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
    
    // Trigger git status update if we have a repo
    if (m_gitRepo && m_gitRepo->isValid()) {
        onGitRepositoryChanged();
    }
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

void FileExplorerWidget::setGitRepo(GitRepo* repo)
{
    if (m_gitRepo) {
        disconnect(m_gitRepo, &GitRepo::repositoryChanged, this, &FileExplorerWidget::onGitRepositoryChanged);
    }
    m_gitRepo = repo;
    if (m_gitRepo) {
        connect(m_gitRepo, &GitRepo::repositoryChanged, this, &FileExplorerWidget::onGitRepositoryChanged);
        // Update the model with the repo
        static_cast<GitFileSystemModel*>(m_model)->setRepo(m_gitRepo);
        // Trigger initial status update
        onGitRepositoryChanged();
    }
}

void FileExplorerWidget::onGitRepositoryChanged()
{
    if (!m_gitRepo || !m_gitRepo->isValid())
        return;
    
    QVector<GitFileStatus> status = m_gitRepo->status(true, true);
    static_cast<GitFileSystemModel*>(m_model)->setGitStatus(status);
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
    
    // Add Git actions if we have a repo and a file selected
    if (m_gitRepo && m_gitRepo->isValid() && index.isValid() && info.isFile()) {
        menu.addSeparator();
        QString relativePath = m_gitRepo->relativeToRepo(info.absoluteFilePath());
        
        QVector<GitFileStatus> status = m_gitRepo->status(true, true);
        GitFileStatus fileStatus;
        for (const GitFileStatus &s : status) {
            if (s.filePath == relativePath) {
                fileStatus = s;
                break;
            }
        }
        
        if (!fileStatus.isClean()) {
            if (fileStatus.isStaged()) {
                QAction *unstage = menu.addAction(tr("Unstage"));
                connect(unstage, &QAction::triggered, this, [this, relativePath] {
                    m_gitRepo->unstage(relativePath);
                });
            } else {
                QAction *stage = menu.addAction(tr("Stage"));
                connect(stage, &QAction::triggered, this, [this, relativePath] {
                    m_gitRepo->stage(relativePath);
                });
            }
        } else if (fileStatus.workTreeStatus == GitFileStatusType::Untracked) {
            QAction *stage = menu.addAction(tr("Stage (Add)"));
            connect(stage, &QAction::triggered, this, [this, relativePath] {
                m_gitRepo->stage(relativePath);
            });
        }
        
        QAction *diff = menu.addAction(tr("Diff"));
        connect(diff, &QAction::triggered, this, [this, relativePath, fileName = info.fileName()] {
            QVector<GitDiffFile> diffs = m_gitRepo->diff(relativePath);
            if (!diffs.isEmpty()) {
                QString output;
                for (const GitDiffFile &df : diffs) {
                    for (const GitDiffHunk &hunk : df.hunks) {
                        output += hunk.header + QLatin1Char('\n');
                        for (const QString &line : hunk.lines) {
                            output += line + QLatin1Char('\n');
                        }
                    }
                }
                QDialog *dialog = new QDialog(this);
                dialog->setWindowTitle(tr("Git Diff - %1").arg(fileName));
                dialog->resize(800, 600);
                
                QVBoxLayout *layout = new QVBoxLayout(dialog);
                QPlainTextEdit *diffView = new QPlainTextEdit(dialog);
                diffView->setReadOnly(true);
                diffView->setPlainText(output);
                QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
                font.setPointSize(10);
                diffView->setFont(font);
                layout->addWidget(diffView);
                
                QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
                connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
                layout->addWidget(buttons);
                
                dialog->exec();
                dialog->deleteLater();
            }
        });
    }
    
    menu.exec(m_tree->viewport()->mapToGlobal(position));
}

} // namespace papiro