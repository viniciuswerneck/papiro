# Git Integration Plan for Papiro

## Executive Summary

This document outlines a phased implementation plan for adding Git version control integration to Papiro, a Qt6-based text editor. The plan covers architecture, technology choices, data structures, UI components, and a 5-phase implementation roadmap.

---

## 1. Current Architecture Overview

### Existing Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `MainWindow` | `src/app/MainWindow.{h,cpp}` | Main window, menus, toolbars, status bar, dock widgets |
| `Document` | `src/app/Document.{h,cpp}` | Single document: editor + file path + encoding + EOL |
| `Editor` | `src/editor/Editor.{h,cpp}` | QPlainTextEdit with syntax highlighting, line numbers, folding |
| `FileExplorerWidget` | `src/ui/FileExplorerWidget.{h,cpp}` | QFileSystemModel-based project tree |
| `Workspace` | `src/core/Workspace.{h,cpp}` | Session persistence (open files, active tab, root path) |

### Build System
- **CMake 3.21+**, C++17
- Static library `papiro_core` (no GUI deps) + executable `Papiro`
- Qt6 Components: Core, Widgets, PrintSupport, LinguistTools
- Optional: Hunspell (spell checking)

### Key Integration Points
1. **Status Bar** (`MainWindow::createStatusBar`) — add branch label + dirty indicator
2. **File Explorer** (`FileExplorerWidget`) — add git status decorators (M, A, D, ?, !)
3. **Dock Widgets** (`MainWindow::createDockWidgets`) — add Git side panel
4. **Editor Context Menu** (`Editor::contextMenuEvent`) — add "Git Blame" action
5. **Document** — track git repository root for each open file

---

## 2. Technology Decision: libgit2 vs git CLI

### Option A: libgit2 (C++ Native Library)

| Pros | Cons |
|------|------|
| Native C API, no process spawn overhead | New dependency (libgit2-dev / vcpkg) |
| Rich API: status, diff, blame, log, commit, branch, remote | Learning curve; C API needs C++ wrappers |
| No parsing of text output | Static linking increases binary size (~2-3 MB) |
| Thread-safe, async operations possible | Platform-specific build config (Windows/macOS/Linux) |
| Direct access to git objects (commits, trees, blobs) | License: GPLv2 with linking exception |

**Integration effort**: Medium-High (write C++ wrappers, handle errors, memory management)

### Option B: git CLI via QProcess

| Pros | Cons |
|------|------|
| Zero new dependencies | Process spawn latency (~10-50ms per call) |
| Simple implementation: `git status --porcelain`, `git diff`, `git log --oneline`, `git blame -L` | Parsing text output is fragile (locale, git version differences) |
| Leverages user's git config (aliases, credentials, SSH) | No fine-grained control; limited progress reporting |
| Works out of the box on all platforms | Cannot easily cancel long-running operations |
| Familiar to all developers | Race conditions if repo changes externally |

**Integration effort**: Low-Medium (QProcess wrappers, output parsers)

### Recommendation: **Option B (git CLI) for MVP**

**Rationale**:
1. Faster time-to-MVP (2-3 weeks vs 6-8 weeks)
2. No additional system dependencies for users
3. Papiro is a text editor, not a Git client — CLI coverage is sufficient for MVP
4. Can migrate to libgit2 later for performance-critical paths (blame, large diffs)
5. CLI approach aligns with Unix philosophy and existing Qt patterns (QProcess)

**Migration path**: Design `GitRepo` interface abstractly; swap implementation later.

---

## 3. Core Data Structures (`src/core/GitTypes.h`)

```cpp
#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMap>

namespace papiro {

// ---------------------------------------------------------------------------
// Repository status
// ---------------------------------------------------------------------------
enum class GitFileStatus {
    Unmodified  = 0,
    Modified    = 1 << 0,  // M
    Added       = 1 << 1,  // A
    Deleted     = 1 << 2,  // D
    Renamed     = 1 << 3,  // R
    Copied      = 1 << 4,  // C
    Untracked   = 1 << 5,  // ?
    Ignored     = 1 << 6,  // !
    Conflicted  = 1 << 7,  // U
};

Q_DECLARE_FLAGS(GitFileStatusFlags, GitFileStatus)

struct GitFileEntry {
    QString path;                 // Relative to repo root
    GitFileStatusFlags status;    // Combined status (index + worktree)
    GitFileStatusFlags indexStatus;
    GitFileStatusFlags worktreeStatus;
    QString originalPath;         // For renames/copies
};

// ---------------------------------------------------------------------------
// Diff
// ---------------------------------------------------------------------------
enum class GitDiffLineType {
    Context,
    Addition,
    Deletion,
    Header,       // @@ -1,5 +1,6 @@
    FileHeader,   // --- a/file.cpp / +++ b/file.cpp
};

struct GitDiffLine {
    GitDiffLineType type;
    QString text;
    int oldLineNumber = -1;  // For context/deletion lines
    int newLineNumber = -1;  // For context/addition lines
};

struct GitDiffHunk {
    int oldStart = 0;
    int oldLines = 0;
    int newStart = 0;
    int newLines = 0;
    QString header;           // @@ -oldStart,oldLines +newStart,newLines @@
    QVector<GitDiffLine> lines;
};

struct GitDiffFile {
    QString oldPath;
    QString newPath;
    bool isNew = false;
    bool isDeleted = false;
    bool isRenamed = false;
    QVector<GitDiffHunk> hunks;
};

// ---------------------------------------------------------------------------
// Commit / Log
// ---------------------------------------------------------------------------
struct GitCommit {
    QString hash;             // Full SHA-1
    QString shortHash;        // First 8 chars
    QString authorName;
    QString authorEmail;
    QDateTime authorDate;
    QString committerName;
    QString committerEmail;
    QDateTime committerDate;
    QString subject;          // First line of message
    QString body;             // Remaining lines
    QVector<QString> parents; // Parent hashes
};

// ---------------------------------------------------------------------------
// Blame
// ---------------------------------------------------------------------------
struct GitBlameLine {
    int lineNumber = 0;       // 1-based
    QString commitHash;
    QString authorName;
    QString authorEmail;
    QDateTime authorDate;
    QString summary;          // Commit subject
    QString code;             // Source line text
};

// ---------------------------------------------------------------------------
// Branch / Remote
// ---------------------------------------------------------------------------
struct GitBranch {
    QString name;
    QString upstream;         // e.g. "origin/main"
    QString upstreamRemote;   // "origin"
    QString upstreamBranch;   // "main"
    QString commitHash;       // Tip commit
    bool isCurrent = false;
    bool isRemote = false;
};

struct GitRemote {
    QString name;
    QString url;
    QString fetchRefspec;
    QString pushRefspec;
};

// ---------------------------------------------------------------------------
// Repository info
// ---------------------------------------------------------------------------
struct GitRepoInfo {
    QString rootPath;         // Absolute path to .git parent
    QString workTreePath;     // Usually same as rootPath
    QString gitDirPath;       // .git or .git/worktrees/xxx
    QString currentBranch;    // Short name (e.g. "main")
    QString currentCommit;    // HEAD commit hash
    bool isBare = false;
    bool isShallow = false;
    bool hasUncommittedChanges = false;
    int aheadCount = 0;       // Commits ahead of upstream
    int behindCount = 0;      // Commits behind upstream
};

} // namespace papiro
```

---

## 4. Core Layer: `GitRepo` (`src/core/GitRepo.h/cpp`)

### Interface Design

```cpp
#pragma once

#include "core/GitTypes.h"
#include <QObject>
#include <QString>
#include <functional>

namespace papiro {

class GitRepo : public QObject {
    Q_OBJECT
public:
    explicit GitRepo(const QString &repoPath, QObject *parent = nullptr);
    ~GitRepo() override;

    // Repository validation
    bool isValid() const;
    QString repoPath() const;
    GitRepoInfo info() const;

    // Async operations with callbacks (run in worker thread via QtConcurrent)
    using StatusCallback = std::function<void(const QVector<GitFileEntry> &)>;
    using DiffCallback = std::function<void(const QVector<GitDiffFile> &)>;
    using LogCallback = std::function<void(const QVector<GitCommit> &)>;
    using BlameCallback = std::function<void(const QVector<GitBlameLine> &)>;
    using BranchesCallback = std::function<void(const QVector<GitBranch> &)>;
    using RemotesCallback = std::function<void(const QVector<GitRemote> &)>;
    using VoidCallback = std::function<void(bool success, const QString &error)>;

    // Status
    void refreshStatus(StatusCallback callback);
    void refreshStatusSync(QVector<GitFileEntry> *out = nullptr);

    // Diff
    void diffFile(const QString &filePath, DiffCallback callback);
    void diffStaged(const QString &filePath, DiffCallback callback);
    void diffCommit(const QString &commitHash, DiffCallback callback);

    // Log/History
    void log(int limit = 100, LogCallback callback);
    void logFile(const QString &filePath, int limit = 50, LogCallback callback);
    void logRange(const QString &fromCommit, const QString &toCommit, LogCallback callback);

    // Blame
    void blameFile(const QString &filePath, BlameCallback callback);
    void blameFileRange(const QString &filePath, int startLine, int endLine, BlameCallback callback);

    // Staging
    void stageFile(const QString &filePath, VoidCallback callback);
    void unstageFile(const QString &filePath, VoidCallback callback);
    void stageAll(VoidCallback callback);
    void unstageAll(VoidCallback callback);

    // Commit
    void commit(const QString &message, const QString &authorName = QString(),
                const QString &authorEmail = QString(), VoidCallback callback);
    void commitAmend(const QString &message, VoidCallback callback);

    // Branch operations
    void fetchBranches(BranchesCallback callback);
    void createBranch(const QString &name, const QString &startPoint = QString(), VoidCallback callback);
    void deleteBranch(const QString &name, bool force = false, VoidCallback callback);
    void checkoutBranch(const QString &name, VoidCallback callback);
    void mergeBranch(const QString &name, VoidCallback callback);

    // Remote operations
    void fetchRemotes(RemotesCallback callback);
    void fetch(const QString &remote = "origin", VoidCallback callback);
    void pull(const QString &remote = "origin", const QString &branch = QString(), VoidCallback callback);
    void push(const QString &remote = "origin", const QString &branch = QString(), bool force = false, VoidCallback callback);

    // Utility
    QString resolveRevision(const QString &rev) const;  // HEAD, main, v1.0, etc.
    bool isIgnored(const QString &filePath) const;
    QString getGitDir() const;

signals:
    void statusChanged();           // Emitted when repo status changes
    void branchesChanged();         // Branch list updated
    void remotesChanged();          // Remote list updated
    void errorOccurred(const QString &message);

private:
    class Private;
    Private *d;
};

} // namespace papiro
```

### Implementation Strategy (CLI Backend)

```cpp
// src/core/GitRepo.cpp - Key implementation patterns

#include "core/GitRepo.h"
#include <QProcess>
#include <QDir>
#include <QRegularExpression>
#include <QtConcurrent>

namespace papiro {

class GitRepo::Private {
public:
    QString m_repoPath;
    QString m_gitDir;
    bool m_valid = false;
    GitRepoInfo m_info;

    // Run git command in repo directory
    QString runGit(const QStringList &args, int timeoutMs = 30000, QString *error = nullptr) {
        QProcess proc;
        proc.setWorkingDirectory(m_repoPath);
        proc.start("git", args);
        if (!proc.waitForStarted(5000)) {
            if (error) *error = "Failed to start git";
            return QString();
        }
        if (!proc.waitForFinished(timeoutMs)) {
            proc.kill();
            proc.waitForFinished(1000);
            if (error) *error = QString("git %1 timed out").args(args.join(' '));
            return QString();
        }
        if (proc.exitCode() != 0) {
            if (error) *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            return QString();
        }
        return QString::fromUtf8(proc.readAllStandardOutput());
    }

    // Parse: git status --porcelain=v1
    QVector<GitFileEntry> parseStatus(const QString &output) { ... }

    // Parse: git diff --unified=3 [--staged] [file]
    QVector<GitDiffFile> parseDiff(const QString &output) { ... }

    // Parse: git log --pretty=format:"%H|%h|%an|%ae|%ad|%cn|%ce|%cd|%s|%b|%P" --date=iso-strict
    QVector<GitCommit> parseLog(const QString &output) { ... }

    // Parse: git blame -L start,end --porcelain file
    QVector<GitBlameLine> parseBlame(const QString &output) { ... }

    // Parse: git branch -vv --format="%(refname:short)|%(upstream:short)|%(objectname:short)|%(HEAD)"
    QVector<GitBranch> parseBranches(const QString &output) { ... }
};

// Async wrapper using QtConcurrent
void GitRepo::refreshStatus(StatusCallback callback) {
    QtConcurrent::run([this, callback]() {
        QVector<GitFileEntry> entries;
        refreshStatusSync(&entries);
        QMetaObject::invokeMethod(this, [callback, entries]() {
            callback(entries);
        }, Qt::QueuedConnection);
    });
}

} // namespace papiro
```

### Key Parsing Functions

| Git Command | Output Format | Parser |
|-------------|---------------|--------|
| `git status --porcelain=v1` | `XY PATH` (X=index, Y=worktree) | `parseStatus()` |
| `git diff --unified=3 [--staged] [--] file` | Unified diff | `parseDiff()` |
| `git log --pretty=format:"%H\|%h\|%an\|%ae\|%ad\|%cn\|%ce\|%cd\|%s\|%b\|%P" --date=iso-strict -n` | Pipe-delimited | `parseLog()` |
| `git blame -L start,end --porcelain file` | Porcelain format | `parseBlame()` |
| `git branch -vv --format="%(refname:short)\|%(upstream:short)\|%(objectname:short)\|%(HEAD)"` | Pipe-delimited | `parseBranches()` |
| `git remote -v` | `name	url (fetch/push)` | `parseRemotes()` |

---

## 5. Coordination Layer: `GitManager` (`src/app/GitManager.h/cpp`)

### Responsibilities
- Track `GitRepo` instances per workspace/document
- Watch for file system changes (QFileSystemWatcher) to auto-refresh status
- Coordinate between UI and core
- Cache current branch for status bar
- Handle repository discovery (walk up from file to find `.git`)

```cpp
#pragma once

#include "core/GitRepo.h"
#include "core/GitTypes.h"
#include <QObject>
#include <QMap>
#include <QFileSystemWatcher>

namespace papiro {

class Document;
class Editor;

class GitManager : public QObject {
    Q_OBJECT
public:
    explicit GitManager(QObject *parent = nullptr);
    ~GitManager() override;

    // Get/create repo for a document's file
    GitRepo *repoForDocument(Document *document);
    GitRepo *repoForPath(const QString &filePath);

    // Current repository (for status bar)
    GitRepo *currentRepo() const;
    void setCurrentRepo(GitRepo *repo);

    // File explorer integration
    GitFileStatusFlags statusForFile(const QString &filePath) const;

    // Editor integration
    void requestBlame(Editor *editor, int startLine, int endLine);
    void requestDiff(const QString &filePath, bool staged);

    // Auto-refresh
    void startWatching(const QString &repoPath);
    void stopWatching(const QString &repoPath);

signals:
    void repoChanged(GitRepo *repo);           // Active repo changed
    void statusChanged(const QString &repoPath, const QVector<GitFileEntry> &entries);
    void blameReady(const QString &filePath, const QVector<GitBlameLine> &lines);
    void diffReady(const QVector<GitDiffFile> &diffs);
    void logReady(const QVector<GitCommit> &commits);
    void branchesReady(const QVector<GitBranch> &branches);
    void operationFinished(bool success, const QString &message);

private:
    QMap<QString, GitRepo *> m_repos;          // repoPath -> GitRepo
    QFileSystemWatcher m_watcher;
    GitRepo *m_currentRepo = nullptr;

    QString findRepoRoot(const QString &path) const;
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
};

} // namespace papiro
```

---

## 6. UI Components

### 6.1 Git Status Bar (`MainWindow` extension)

```cpp
// In MainWindow.h - add to private section:
QLabel *m_gitBranchLabel = nullptr;
QLabel *m_gitDirtyIndicator = nullptr;  // ● when uncommitted changes

// In MainWindow::createStatusBar():
m_gitBranchLabel = new QLabel(this);
m_gitBranchLabel->setMinimumWidth(150);
m_gitDirtyIndicator = new QLabel(this);
m_gitDirtyIndicator->setMinimumWidth(20);
statusBar()->addPermanentWidget(m_gitDirtyIndicator);
statusBar()->addPermanentWidget(m_gitBranchLabel);

// Connect to GitManager:
connect(m_gitManager, &GitManager::repoChanged, this, &MainWindow::updateGitStatusBar);
connect(m_gitManager, &GitManager::statusChanged, this, &MainWindow::updateGitDirtyIndicator);

void MainWindow::updateGitStatusBar(GitRepo *repo) {
    if (!repo || !repo->isValid()) {
        m_gitBranchLabel->clear();
        m_gitDirtyIndicator->clear();
        return;
    }
    const auto info = repo->info();
    m_gitBranchLabel->setText(QString("🌿 %1").arg(info.currentBranch));
    updateGitDirtyIndicator(info.hasUncommittedChanges);
}

void MainWindow::updateGitDirtyIndicator(bool dirty) {
    m_gitDirtyIndicator->setText(dirty ? "●" : "");
    m_gitDirtyIndicator->setStyleSheet(dirty ? "color: #ff6b6b;" : "");
}
```

### 6.2 File Explorer Decorators (`FileExplorerWidget` extension)

**Approach**: Subclass `QFileSystemModel` → `GitFileSystemModel` with custom `data()` for `Qt::DecorationRole`.

```cpp
// src/ui/GitFileSystemModel.h
#pragma once
#include <QFileSystemModel>
#include "core/GitTypes.h"

namespace papiro {

class GitManager;

class GitFileSystemModel : public QFileSystemModel {
    Q_OBJECT
public:
    explicit GitFileSystemModel(GitManager *gitManager, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;

    void setRepoPath(const QString &repoPath);
    void refreshStatus();

private:
    GitManager *m_gitManager = nullptr;
    QString m_repoPath;
    QMap<QString, GitFileStatusFlags> m_statusCache;  // path -> status
};

} // namespace papiro
```

```cpp
// src/ui/GitFileSystemModel.cpp - data() implementation
QVariant GitFileSystemModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QFileSystemModel::data(index, role);

    if (role == Qt::DecorationRole && index.column() == 0) {
        const QString path = filePath(index);
        if (m_statusCache.contains(path)) {
            const auto status = m_statusCache[path];
            // Return colored icon based on status
            return statusIcon(status);
        }
    }
    if (role == Qt::ForegroundRole && index.column() == 0) {
        const QString path = filePath(index);
        if (m_statusCache.contains(path)) {
            const auto status = m_statusCache[path];
            if (status & GitFileStatus::Conflicted)
                return QBrush(Qt::red);
            if (status & GitFileStatus::Untracked)
                return QBrush(Qt::gray);
        }
    }
    return QFileSystemModel::data(index, role);
}
```

**Status Icon Mapping**:
| Status | Icon | Color |
|--------|------|-------|
| Modified (M) | 📝 / pencil | Orange |
| Added (A) | ➕ / plus | Green |
| Deleted (D) | 🗑️ / trash | Red |
| Renamed (R) | ↔️ / arrow | Blue |
| Untracked (?) | ❓ / question | Gray |
| Ignored (!) | 🚫 / banned | Dark gray |
| Conflicted (U) | ⚠️ / warning | Red |

### 6.3 Git Dock Widget (`src/ui/GitWidget.h/cpp`)

```cpp
// src/ui/GitWidget.h
#pragma once
#include <QWidget>
#include "core/GitTypes.h"

class QTreeWidget;
class QLineEdit;
class QPushButton;
class QComboBox;
class QTextEdit;

namespace papiro {

class GitManager;
class GitDiffView;

class GitWidget : public QWidget {
    Q_OBJECT
public:
    explicit GitWidget(GitManager *gitManager, QWidget *parent = nullptr);

    void setRepo(GitRepo *repo);

signals:
    void diffRequested(const QString &filePath, bool staged);
    void blameRequested(const QString &filePath);
    void commitRequested(const QString &message);

private slots:
    void onRepoChanged(GitRepo *repo);
    void onStatusChanged(const QVector<GitFileEntry> &entries);
    void onBranchChanged(const QVector<GitBranch> &branches);
    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onStageClicked();
    void onUnstageClicked();
    void onStageAllClicked();
    void onUnstageAllClicked();
    void onCommitClicked();
    void onBranchActivated(QTreeWidgetItem *item, int column);
    void onRefreshClicked();

private:
    void setupUI();
    void populateStatus(const QVector<GitFileEntry> &entries);
    void populateBranches(const QVector<GitBranch> &branches);
    void updateActions();

    GitManager *m_gitManager = nullptr;
    GitRepo *m_currentRepo = nullptr;

    // UI
    QComboBox *m_branchCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QTreeWidget *m_changesTree = nullptr;      // Files with checkboxes for staging
    QTextEdit *m_commitMessageEdit = nullptr;
    QPushButton *m_commitBtn = nullptr;
    QPushButton *m_stageBtn = nullptr;
    QPushButton *m_unstageBtn = nullptr;
    QPushButton *m_stageAllBtn = nullptr;
    QPushButton *m_unstageAllBtn = nullptr;

    // State
    QSet<QString> m_stagedFiles;
    QSet<QString> m_unstagedFiles;
};

} // namespace papiro
```

**Layout**:
```
┌─────────────────────────────────────┐
│ 🌿 Branch: [main           ▼] [🔄]  │  ← Branch selector + refresh
├─────────────────────────────────────┤
│ Changes (3)                         │  ← Header with count
│ ☐  src/main.cpp           M  ●      │  ← Checkbox = staged, M/A/D = status
│ ☑  src/core/GitRepo.h      A  ●     │
│ ☐  README.md               ?  ○     │  ← Untracked
├─────────────────────────────────────┤
│ [Stage] [Unstage] [Stage All] [Unst.│  ← Actions
├─────────────────────────────────────┤
│ Commit Message                      │
│ [_______________________________]   │  ← QTextEdit (multi-line)
│ [Commit]                            │
└─────────────────────────────────────┘
```

### 6.4 Diff Viewer (`src/ui/GitDiffView.h/cpp`)

```cpp
// src/ui/GitDiffView.h
#pragma once
#include <QWidget>
#include "core/GitTypes.h"

class QSplitter;
class QTreeWidget;
class QPlainTextEdit;
class QToolBar;

namespace papiro {

class GitDiffView : public QWidget {
    Q_OBJECT
public:
    enum class Mode { SideBySide, Unified };
    explicit GitDiffView(QWidget *parent = nullptr);

    void setDiff(const QVector<GitDiffFile> &diffs);
    void setMode(Mode mode);

signals:
    void openFileRequested(const QString &path, int lineNumber);

private:
    void setupUI();
    void populateFileList(const QVector<GitDiffFile> &diffs);
    void showFileDiff(const GitDiffFile &diff);
    void renderSideBySide(const GitDiffFile &diff);
    void renderUnified(const GitDiffFile &diff);
    QString formatLine(const GitDiffLine &line, bool isLeft) const;

    Mode m_mode = Mode::SideBySide;
    QVector<GitDiffFile> m_diffs;

    QSplitter *m_splitter = nullptr;
    QTreeWidget *m_fileList = nullptr;
    QWidget *m_diffContainer = nullptr;
    QPlainTextEdit *m_leftView = nullptr;   // Original
    QPlainTextEdit *m_rightView = nullptr;  // Modified
    QToolBar *m_toolbar = nullptr;
};

} // namespace papiro
```

**Features**:
- Side-by-side (default) and unified toggle
- Syntax highlighting preserved in diff views
- Line numbers on both sides
- Click line number → open file at that line
- Color coding: green (additions), red (deletions), yellow (context)
- Keyboard navigation: `n`/`p` next/prev hunk, `j`/`k` line up/down

### 6.5 Blame View (`src/ui/GitBlameView.h/cpp`)

```cpp
// src/ui/GitBlameView.h
#pragma once
#include <QWidget>
#include "core/GitTypes.h"

class QTableView;
class QStandardItemModel;
class QHeaderView;

namespace papiro {

class GitBlameView : public QWidget {
    Q_OBJECT
public:
    explicit GitBlameView(QWidget *parent = nullptr);

    void setBlame(const QVector<GitBlameLine> &lines);
    void setSourceCode(const QString &code);  // For inline annotations

signals:
    void commitClicked(const QString &commitHash);
    void openFileAtLineRequested(const QString &filePath, int line);

private:
    void setupUI();
    void populateModel(const QVector<GitBlameLine> &lines);
    QString formatTime(const QDateTime &dt) const;

    QTableView *m_table = nullptr;
    QStandardItemModel *m_model = nullptr;
};

} // namespace papiro
```

**Inline Blame (Editor Integration)**:
- Right-click in Editor → "Show Git Blame"
- Adds a narrow column left of line numbers showing: `author • short-hash • relative-time`
- Hover → tooltip with full commit message
- Click → open commit in history log
- Toggle with `Ctrl+Shift+B` or toolbar button

```cpp
// In Editor.h - add:
void setBlameAnnotations(const QVector<GitBlameLine> &blame);
void clearBlameAnnotations();
bool hasBlameAnnotations() const;

// In Editor.cpp - paintEvent():
// Draw blame column in line number area (extends lineNumberAreaWidth)
```

---

## 7. Integration Points & Modifications

### 7.1 Modified Files

| File | Changes |
|------|---------|
| `CMakeLists.txt` | Add `src/core/GitRepo.cpp`, `src/ui/GitWidget.cpp`, `src/ui/GitDiffView.cpp`, `src/ui/GitBlameView.cpp`, `src/app/GitManager.cpp` to `PAPIRO_APP_SOURCES` |
| `src/app/MainWindow.h` | Add `GitManager *m_gitManager`, `GitWidget *m_gitWidget`, `GitDiffView *m_diffView`, `GitBlameView *m_blameView`, dock widgets, status bar labels |
| `src/app/MainWindow.cpp` | Initialize `GitManager`, create Git dock, connect signals, update status bar |
| `src/ui/FileExplorerWidget.h` | Change `m_model` to `GitFileSystemModel *`, add `setGitManager()` |
| `src/ui/FileExplorerWidget.cpp` | Use `GitFileSystemModel`, connect status updates |
| `src/editor/Editor.h` | Add blame annotation methods, context menu action |
| `src/editor/Editor.cpp` | Implement blame column painting, context menu |
| `src/app/Document.h` | Add `gitRepoRoot()` method |
| `src/app/Document.cpp` | Track repo root on file load |

### 7.2 New Files

```
src/core/
├── GitTypes.h          # Data structures (this plan)
├── GitRepo.h/cpp       # Core git operations (CLI backend)
├── GitManager.h/cpp    # Coordination layer

src/ui/
├── GitFileSystemModel.h/cpp  # Decorated file system model
├── GitWidget.h/cpp           # Main Git dock widget
├── GitDiffView.h/cpp         # Diff viewer
├── GitBlameView.h/cpp        # Blame viewer (standalone)
```

---

## 8. Phased Implementation Plan

### Phase 1: Core Foundation (Week 1-2)
**Goal**: `GitRepo` working with CLI backend, basic status/diff/log/blame

| Task | Details |
|------|---------|
| 1.1 | Create `GitTypes.h` with all data structures |
| 1.2 | Implement `GitRepo` class with QProcess-based CLI wrapper |
| 1.3 | Implement parsers: `git status --porcelain`, `git diff`, `git log`, `git blame`, `git branch` |
| 1.4 | Add `GitManager` for repo discovery and coordination |
| 1.5 | Unit tests for parsers (test data in `tests/git_testdata/`) |
| 1.6 | Integration test: open repo, get status, diff, log, blame |

**Deliverable**: `GitRepo` and `GitManager` functional, tested in isolation

### Phase 2: Status Bar + File Explorer Decorators (Week 2-3)
**Goal**: Visual git status in existing UI

| Task | Details |
|------|---------|
| 2.1 | Add branch label + dirty indicator to `MainWindow::createStatusBar()` |
| 2.2 | Create `GitFileSystemModel` subclass with decoration/foreground roles |
| 2.3 | Modify `FileExplorerWidget` to use `GitFileSystemModel` |
| 2.4 | Connect `GitManager::statusChanged` → update model cache |
| 2.5 | Auto-detect repo when folder opened in File Explorer |
| 2.6 | Context menu: "Open in Terminal", "Copy Git Hash" |

**Deliverable**: Status bar shows branch; File Explorer shows M/A/D/?/! icons

### Phase 3: Git Dock Widget (Week 3-4)
**Goal**: Full staging/commit UI in side panel

| Task | Details |
|------|---------|
| 3.1 | Create `GitWidget` with branch selector, file list, commit message, buttons |
| 3.2 | Implement staging/unstaging (checkbox toggle + buttons) |
| 3.3 | Commit action with message validation |
| 3.4 | Branch switching via combo box |
| 3.5 | Add dock to `MainWindow::createDockWidgets()` |
| 3.6 | Connect `GitManager` signals to `GitWidget` slots |

**Deliverable**: Functional Git side panel for stage/commit/branch

### Phase 4: Diff Viewer (Week 4-5)
**Goal**: Visual diff for modified files

| Task | Details |
|------|---------|
| 4.1 | Create `GitDiffView` with side-by-side + unified modes |
| 4.2 | Syntax highlighting in diff panes (reuse `SyntaxHighlighter`) |
| 4.3 | Double-click file in `GitWidget` → open diff in new tab/dock |
| 4.4 | Keyboard navigation (hunk/line) |
| 4.5 | "Open File at Line" from diff |
| 4.6 | Staged vs unstaged diff toggle |

**Deliverable**: Integrated diff viewer accessible from Git dock

### Phase 5: Blame + History (Week 5-6)
**Goal**: Line-level annotation and commit history

| Task | Details |
|------|---------|
| 5.1 | Create `GitBlameView` (standalone table widget) |
| 5.2 | Implement inline blame annotations in `Editor` (gutter column) |
| 5.3 | Editor context menu: "Git Blame" → toggle inline annotations |
| 5.4 | Blame hover tooltip with commit details |
| 5.5 | Click blame → open commit in log view |
| 5.6 | History log view: `GitWidget` tab or separate dialog with commit graph |

**Deliverable**: Full blame + history experience

---

## 9. CMake Integration

```cmake
# In CMakeLists.txt - add to PAPIRO_APP_SOURCES:
set(PAPIRO_APP_SOURCES
    ...
    src/core/GitRepo.cpp
    src/core/GitManager.cpp
    src/ui/GitFileSystemModel.cpp
    src/ui/GitWidget.cpp
    src/ui/GitDiffView.cpp
    src/ui/GitBlameView.cpp
)

# Optional: Find libgit2 for future migration
# find_package(libgit2 QUIET)
# if(libgit2_FOUND)
#     target_link_libraries(papiro_core PRIVATE git2)
#     target_compile_definitions(papiro_core PRIVATE PAPIRO_HAVE_LIBGIT2=1)
# endif()
```

---

## 10. Testing Strategy

### Unit Tests (`tests/test_git.cpp`)
- Parser tests for each git command output format
- Edge cases: empty repo, submodules, worktrees, conflicted merges
- Mock `QProcess` for deterministic testing

### Integration Tests
- Real git repo in temp directory
- Test full workflow: init → add → commit → modify → stage → commit → diff → blame

### Manual Test Checklist
- [ ] Open non-git folder → no git UI
- [ ] Open git repo → status bar shows branch
- [ ] Modify file → dirty indicator appears
- [ ] File Explorer shows M/A/D/? icons
- [ ] Stage/unstage works
- [ ] Commit with message works
- [ ] Diff viewer opens on double-click
- [ ] Blame shows correct author/date
- [ ] Branch switching works
- [ ] Handles large repos (>10k files) performantly

---

## 11. Performance Considerations

| Operation | Optimization |
|-----------|--------------|
| `git status` on large repo | Run async, debounce file watcher (500ms), cache results |
| `git blame` on large file | Limit to visible lines + context (±50 lines), paginate |
| `git diff` on large files | Stream parse, virtual scrolling in diff view |
| File watcher | Watch `.git/index` + repo root only, not whole tree |
| Repo discovery | Cache root path per document, invalidate on folder change |

---

## 12. Future Enhancements (Post-MVP)

| Feature | Effort | Notes |
|---------|--------|-------|
| libgit2 backend | High | Swap `GitRepo` implementation |
| Interactive rebase UI | High | Complex |
| Merge conflict resolver | Medium | 3-way diff editor |
| GitHub/GitLab integration | Medium | PR view, issues |
| Submodule support | Low | Recursive status |
| Worktree support | Low | Multiple working dirs |
| GPG signing | Low | `git commit -S` |
| LFS support | Medium | `git lfs` commands |
| Visual commit graph | Medium | Graph layout algorithm |

---

## 13. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| CLI parsing breaks with git version | Medium | High | Test with git 2.30+, 2.40+; use `--porcelain` formats |
| Large repo performance | High | Medium | Async, caching, pagination, debouncing |
| Non-UTF8 filenames/output | Low | Medium | Use `QProcess::setStandardOutputProcessChannelMode` |
| Windows git path differences | Medium | Medium | Use `git` from PATH, handle CRLF |
| Concurrent repo modifications | Low | Medium | Refresh on focus-in, file watcher |

---

## 14. Estimated Timeline

| Phase | Duration | Cumulative |
|-------|----------|------------|
| Phase 1: Core | 2 weeks | 2 weeks |
| Phase 2: Status Bar + Explorer | 1.5 weeks | 3.5 weeks |
| Phase 3: Git Dock | 1.5 weeks | 5 weeks |
| Phase 4: Diff Viewer | 1.5 weeks | 6.5 weeks |
| Phase 5: Blame + History | 1.5 weeks | 8 weeks |
| **Total MVP** | **~8 weeks** | |

**Buffer**: +2 weeks for testing, bug fixes, polish → **10 weeks total**

---

## 15. Appendix: Git Command Reference

### Status
```bash
git status --porcelain=v1 --untracked-files=all
# XY PATH
# X = index status, Y = worktree status
# M=modified, A=added, D=deleted, R=renamed, C=copied, ?=untracked, !=ignored, U=unmerged
```

### Diff
```bash
# Unstaged changes
git diff --unified=3 -- [file]

# Staged changes
git diff --staged --unified=3 -- [file]

# Between commits
git diff <commit1> <commit2> -- [file]
```

### Log
```bash
git log --pretty=format:"%H|%h|%an|%ae|%ad|%cn|%ce|%cd|%s|%b|%P" --date=iso-strict -n 100
# Full hash | short hash | author name | author email | author date (ISO) | committer name | committer email | committer date | subject | body | parents
```

### Blame
```bash
git blame -L <start>,<end> --porcelain -- [file]
# Porcelain format: one block per line with commit info + code
```

### Branch
```bash
git branch -vv --format="%(refname:short)|%(upstream:short)|%(objectname:short)|%(HEAD)"
# name|upstream|commit|*
```

### Stage/Commit
```bash
git add [file]
git reset HEAD [file]           # unstage
git commit -m "message"
git commit --amend -m "message"
```

### Remote
```bash
git remote -v
git fetch [remote]
git pull [remote] [branch]
git push [remote] [branch]
```

---

*End of Plan*