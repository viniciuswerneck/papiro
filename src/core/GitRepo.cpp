#include "GitRepo.h"

#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>

namespace papiro {

// ============================================================================
// GitFileStatus::parseStatusChar
// ============================================================================

GitFileStatusType GitFileStatus::parseStatusChar(QChar c) {
    switch (c.toLatin1()) {
        case ' ': return GitFileStatusType::Unmodified;
        case 'A': return GitFileStatusType::Added;
        case 'M': return GitFileStatusType::Modified;
        case 'D': return GitFileStatusType::Deleted;
        case 'R': return GitFileStatusType::Renamed;
        case 'C': return GitFileStatusType::Copied;
        case '?': return GitFileStatusType::Untracked;
        case '!': return GitFileStatusType::Ignored;
        case 'U': return GitFileStatusType::Unmerged;
        case 'T': return GitFileStatusType::TypeChange;
        default: return GitFileStatusType::Unmodified;
    }
}

// ============================================================================
// GitRepo Implementation
// ============================================================================

GitRepo::GitRepo(const QString& repoPath, QObject* parent)
    : QObject(parent)
    , m_repoPath(QDir::cleanPath(repoPath))
{
    // Validate and find git directory
    QDir dir(m_repoPath);
    if (!dir.exists()) {
        m_valid = false;
        return;
    }

    // Check if it's a git repository
    QString gitDirCandidate = m_repoPath + "/.git";
    if (QFileInfo::exists(gitDirCandidate)) {
        if (QFileInfo(gitDirCandidate).isDir()) {
            m_gitDir = gitDirCandidate;
            m_workTree = m_repoPath;
        } else {
            // .git is a file (submodule or worktree)
            QFile file(gitDirCandidate);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString content = file.readAll().trimmed();
                QRegularExpression re("^gitdir:\\s*(.+)$");
                QRegularExpressionMatch match = re.match(content);
                if (match.hasMatch()) {
                    m_gitDir = QDir(m_repoPath).absoluteFilePath(match.captured(1));
                    m_workTree = m_repoPath;
                }
            }
        }
    } else {
        // Check if repoPath itself is the .git dir (bare repo)
        QFileInfo fi(m_repoPath);
        if (fi.fileName() == ".git" || fi.fileName().endsWith(".git")) {
            m_gitDir = m_repoPath;
            m_workTree = QDir(m_repoPath).absolutePath();
            // For bare repo, worktree is parent
            if (!isBare()) {
                m_workTree = QDir(m_workTree).absolutePath();
            }
        }
    }

    // Validate by running git rev-parse
    QString output, error;
    m_valid = executeGit({"rev-parse", "--git-dir"}, &output, &error, 5000);
    if (m_valid && output.isEmpty()) {
        m_valid = false;
    }

    if (m_valid) {
        m_gitDir = QDir::cleanPath(output.trimmed());
        // Get work tree
        executeGit({"rev-parse", "--show-toplevel"}, &output, &error, 5000);
        if (!output.isEmpty()) {
            m_workTree = QDir::cleanPath(output.trimmed());
        }
    }
}

GitRepo::~GitRepo() = default;

bool GitRepo::isValid() const {
    return m_valid;
}

QString GitRepo::repoPath() const {
    return m_repoPath;
}

QString GitRepo::gitDir() const {
    return m_gitDir;
}

QString GitRepo::workTree() const {
    return m_workTree;
}

// ============================================================================
// Execute helpers
// ============================================================================

bool GitRepo::executeGit(const QStringList& args, QString* output, QString* error, int timeoutMs) const {
    return executeGitInDir(m_workTree, args, output, error, timeoutMs);
}

bool GitRepo::executeGitInDir(const QString& workingDir, const QStringList& args, QString* output, QString* error, int timeoutMs) const {
    QProcess process;
    process.setWorkingDirectory(workingDir);
    process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    // Ensure git uses English output for parsing
    QProcessEnvironment env = process.processEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("LANG", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    process.setProcessEnvironment(env);

    process.start("git", args);
    if (!process.waitForStarted(5000)) {
        if (error) *error = "Failed to start git process";
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        if (error) *error = QString("Git command timed out after %1ms").arg(timeoutMs);
        return false;
    }

    QString out = QString::fromUtf8(process.readAllStandardOutput());
    QString err = QString::fromUtf8(process.readAllStandardError());

    if (output) *output = out;
    if (error) *error = err;

    return process.exitCode() == 0;
}

void GitRepo::executeAsync(const QStringList& args, GitCallback callback) {
    // Simple async implementation using a temporary process
    // In a production app, you might want a persistent process pool
    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_workTree);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("LANG", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    process->setProcessEnvironment(env);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [process, callback](int exitCode, QProcess::ExitStatus) {
                QString out = QString::fromUtf8(process->readAllStandardOutput());
                QString err = QString::fromUtf8(process->readAllStandardError());
                callback(out, err, exitCode == 0);
                process->deleteLater();
            });

    process->start("git", args);
}

// ============================================================================
// Cache helpers
// ============================================================================

void GitRepo::invalidateStatusCache() const {
    m_statusCache.clear();
    m_statusCacheValid = false;
}

QString GitRepo::normalizePath(const QString& path) const {
    return QDir::cleanPath(path);
}

QString GitRepo::relativeToRepo(const QString& path) const {
    QFileInfo fi(path);
    if (fi.isRelative()) {
        return path;
    }
    return QDir(m_workTree).relativeFilePath(path);
}

// ============================================================================
// Status
// ============================================================================

QVector<GitFileStatus> GitRepo::status(bool includeUntracked, bool includeIgnored) {
    if (!m_valid) return {};

    QStringList args = {"status", "--porcelain=v1"};
    if (includeUntracked) args << "-u";
    else args << "-uno";
    if (includeIgnored) args << "--ignored";

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        emit errorOccurred("status", error);
        return {};
    }

    QVector<GitFileStatus> result = parseStatus(output);
    invalidateStatusCache();
    return result;
}

GitFileStatus GitRepo::fileStatus(const QString& filePath) {
    QString relPath = relativeToRepo(filePath);
    QStringList args = {"status", "--porcelain=v1", "--", relPath};

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        emit errorOccurred("status", error);
        return {};
    }

    QVector<GitFileStatus> result = parseStatus(output);
    return result.isEmpty() ? GitFileStatus() : result.first();
}

QVector<GitFileStatus> GitRepo::parseStatus(const QString& output) {
    QVector<GitFileStatus> result;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        if (line.length() < 3) continue;

        GitFileStatus status;
        status.filePath = line.mid(3).trimmed();

        // Parse index status (first char)
        status.indexStatus = GitFileStatus::parseStatusChar(line[0]);
        // Parse worktree status (second char)
        status.workTreeStatus = GitFileStatus::parseStatusChar(line[1]);

        // Handle renames (format: "R  old -> new")
        if (status.indexStatus == GitFileStatusType::Renamed ||
            status.workTreeStatus == GitFileStatusType::Renamed) {
            QString path = status.filePath;
            int arrowPos = path.indexOf(" -> ");
            if (arrowPos > 0) {
                status.renamedFrom = path.left(arrowPos).trimmed();
                status.renamedTo = path.mid(arrowPos + 4).trimmed();
                status.filePath = status.renamedTo;
            }
        }

        result.append(status);
    }

    return result;
}

// ============================================================================
// Branch
// ============================================================================

QVector<GitBranch> GitRepo::branches() {
    if (!m_valid) return {};

    QString output, error;
    if (!executeGit({"branch", "-v", "-a"}, &output, &error)) {
        emit errorOccurred("branch", error);
        return {};
    }

    return parseBranches(output);
}

GitBranch GitRepo::currentBranch() {
    QVector<GitBranch> all = branches();
    for (const GitBranch& branch : all) {
        if (branch.isCurrent) return branch;
    }
    return {};
}

bool GitRepo::checkoutBranch(const QString& branchName, bool create) {
    if (!m_valid) return false;

    QStringList args = {"checkout"};
    if (create) args << "-b";
    args << branchName;

    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("checkout", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::createBranch(const QString& branchName, const QString& startPoint) {
    if (!m_valid) return false;

    QStringList args = {"branch", branchName};
    if (!startPoint.isEmpty()) args << startPoint;

    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("branch", error);
    }
    return success;
}

bool GitRepo::deleteBranch(const QString& branchName, bool force) {
    if (!m_valid) return false;

    QStringList args = {"branch"};
    args << (force ? "-D" : "-d");
    args << branchName;

    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("branch", error);
    }
    return success;
}

QVector<GitBranch> GitRepo::parseBranches(const QString& output) {
    QVector<GitBranch> result;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    // First pass: collect all branches
    QMap<QString, GitBranch> branchMap;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        GitBranch branch;
        bool isCurrent = trimmed.startsWith('*');
        if (isCurrent) {
            trimmed = trimmed.mid(1).trimmed();
            branch.isCurrent = true;
        }

        // Parse: branchName commitHash [subject] [upstream]
        QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        branch.name = parts[0];
        if (parts.size() > 1) {
            branch.commitHash = parts[1];
            branch.commitShortHash = branch.commitHash.left(7);
        }

        // Check if remote branch
        if (branch.name.startsWith("remotes/")) {
            branch.type = GitBranchType::Remote;
            branch.isRemote = true;
            branch.remoteName = branch.name.split('/')[1];
            branch.displayName = branch.name.mid(8); // Remove "remotes/"
        } else {
            branch.type = GitBranchType::Local;
            branch.displayName = branch.name;
        }

        branchMap[branch.name] = branch;
    }

    // Second pass: get upstream info for local branches
    QString output2, error;
    if (executeGit({"for-each-ref", "--format=%(refname:short)|%(upstream:short)|%(upstream:track)"},
                   &output2, &error)) {
        QStringList lines2 = output2.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines2) {
            QStringList parts = line.split('|');
            if (parts.size() >= 2) {
                QString refName = parts[0];
                QString upstream = parts[1];
                if (branchMap.contains(refName)) {
                    branchMap[refName].upstreamBranch = upstream;
                    // Parse ahead/behind from track info
                    if (parts.size() >= 3) {
                        QString track = parts[2];
                        QRegularExpression re("ahead (\\d+),?\\s*behind (\\d+)");
                        QRegularExpressionMatch match = re.match(track);
                        if (match.hasMatch()) {
                            branchMap[refName].ahead = match.captured(1).toInt();
                            branchMap[refName].behind = match.captured(2).toInt();
                        }
                    }
                }
            }
        }
    }

    result = branchMap.values();
    return result;
}

// ============================================================================
// Diff
// ============================================================================

QVector<GitDiffFile> GitRepo::diff(const QString& filePath, bool staged) {
    if (!m_valid) return {};

    QStringList args = {"diff", "--no-color"};
    if (staged) args << "--cached";
    if (!filePath.isEmpty()) args << "--" << relativeToRepo(filePath);

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        emit errorOccurred("diff", error);
        return {};
    }

    return parseDiff(output);
}

QVector<GitDiffFile> GitRepo::diffCached(const QString& filePath) {
    return diff(filePath, true);
}

QVector<GitDiffFile> GitRepo::diffHEAD(const QString& filePath) {
    if (!m_valid) return {};

    QStringList args = {"diff", "--no-color", "HEAD"};
    if (!filePath.isEmpty()) args << "--" << relativeToRepo(filePath);

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        emit errorOccurred("diff", error);
        return {};
    }

    return parseDiff(output);
}

QVector<GitDiffFile> GitRepo::parseDiff(const QString& output) {
    QVector<GitDiffFile> result;
    QStringList lines = output.split('\n');

    GitDiffFile* currentFile = nullptr;
    GitDiffHunk* currentHunk = nullptr;

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];

        // File header: diff --git a/file b/file
        if (line.startsWith("diff --git ")) {
            if (currentFile) result.append(*currentFile);
            currentFile = new GitDiffFile();
            currentHunk = nullptr;

            // Parse file paths
            QRegularExpression re("diff --git a/(.+) b/(.+)");
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                currentFile->oldFilePath = match.captured(1);
                currentFile->newFilePath = match.captured(2);
                currentFile->filePath = currentFile->newFilePath;
            }
        }
        // Index line
        else if (line.startsWith("index ") && currentFile) {
            // index hash..hash mode
        }
        // File mode changes
        else if (line.startsWith("new file mode ") && currentFile) {
            currentFile->status = GitFileStatusType::Added;
        }
        else if (line.startsWith("deleted file mode ") && currentFile) {
            currentFile->status = GitFileStatusType::Deleted;
        }
        // Rename/copy detection
        else if (line.startsWith("rename from ") && currentFile) {
            currentFile->oldFilePath = line.mid(12).trimmed();
            currentFile->status = GitFileStatusType::Renamed;
        }
        else if (line.startsWith("rename to ") && currentFile) {
            currentFile->newFilePath = line.mid(10).trimmed();
            currentFile->filePath = currentFile->newFilePath;
            currentFile->status = GitFileStatusType::Renamed;
        }
        else if (line.startsWith("copy from ") && currentFile) {
            currentFile->oldFilePath = line.mid(10).trimmed();
            currentFile->status = GitFileStatusType::Copied;
        }
        else if (line.startsWith("copy to ") && currentFile) {
            currentFile->newFilePath = line.mid(8).trimmed();
            currentFile->filePath = currentFile->newFilePath;
            currentFile->status = GitFileStatusType::Copied;
        }
        // Binary file
        else if (line.startsWith("Binary files ") && currentFile) {
            currentFile->isBinary = true;
        }
        // Hunk header: @@ -oldStart,oldLines +newStart,newLines @@
        else if (line.startsWith("@@ ") && currentFile) {
            if (currentHunk) currentFile->hunks.append(*currentHunk);
            currentHunk = new GitDiffHunk();
            currentHunk->header = line;

            QRegularExpression re("@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@");
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                currentHunk->oldStart = match.captured(1).toInt();
                currentHunk->oldLines = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
                currentHunk->newStart = match.captured(3).toInt();
                currentHunk->newLines = match.captured(4).isEmpty() ? 1 : match.captured(4).toInt();
            }
        }
        // Diff content lines
        else if (currentHunk && !line.isEmpty()) {
            currentHunk->lines.append(line);
            if (line.startsWith('+') && !line.startsWith("+++")) {
                currentFile->additions++;
            } else if (line.startsWith('-') && !line.startsWith("---")) {
                currentFile->deletions++;
            }
        }
    }

    if (currentFile) result.append(*currentFile);

    // Clean up
    delete currentFile;
    delete currentHunk;

    return result;
}

// ============================================================================
// Log/History
// ============================================================================

QVector<GitCommit> GitRepo::log(int limit, const QString& since, const QString& until, const QString& filePath) {
    if (!m_valid) return {};

    QStringList args = {
        "log",
        "--pretty=format:%H|%h|%an|%ae|%ad|%s|%b|%P",
        "--date=iso-strict",
        QString("-%1").arg(limit)
    };

    if (!since.isEmpty()) args << QString("--since=%1").arg(since);
    if (!until.isEmpty()) args << QString("--until=%1").arg(until);
    if (!filePath.isEmpty()) args << "--" << relativeToRepo(filePath);

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        emit errorOccurred("log", error);
        return {};
    }

    return parseLog(output);
}

QVector<GitCommit> GitRepo::logAll(int limit) {
    return log(limit);
}

QVector<GitCommit> GitRepo::parseLog(const QString& output) {
    QVector<GitCommit> result;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QStringList parts = line.split('|');
        if (parts.size() < 8) continue;

        GitCommit commit;
        commit.hash = parts[0];
        commit.shortHash = parts[1];
        commit.authorName = parts[2];
        commit.authorEmail = parts[3];
        commit.authorDate = QDateTime::fromString(parts[4], Qt::ISODate);
        commit.subject = parts[5];
        commit.body = parts[6];
        commit.parentHashes = parts[7].split(' ', Qt::SkipEmptyParts);

        // Get refs (branches/tags)
        QString output2, error;
        if (executeGit({"log", "-1", "--pretty=format:%D", commit.hash}, &output2, &error)) {
            QString refs = output2.trimmed();
            if (!refs.isEmpty() && refs != "HEAD") {
                commit.refs = refs.split(", ", Qt::SkipEmptyParts);
            }
        }

        result.append(commit);
    }

    return result;
}

// ============================================================================
// Blame
// ============================================================================

QVector<GitBlameLine> GitRepo::blame(const QString& filePath, int startLine, int endLine) {
    if (!m_valid) return {};

    QStringList args = {"blame", "--porcelain"};
    if (startLine > 0 || endLine > 0) {
        QString range;
        if (endLine > 0) range = QString("%1,%2").arg(startLine).arg(endLine);
        else range = QString("%1").arg(startLine);
        args << "-L" << range;
    }
    args << "--" << relativeToRepo(filePath);

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        emit errorOccurred("blame", error);
        return {};
    }

    return parseBlame(output);
}

QVector<GitBlameLine> GitRepo::parseBlame(const QString& output) {
    QVector<GitBlameLine> result;
    QStringList lines = output.split('\n');

    GitBlameLine currentLine;
    bool inHeader = true;
    QString currentCommitHash;

    for (const QString& line : lines) {
        if (line.isEmpty()) continue;

        if (inHeader) {
            if (line.startsWith('\t')) {
                // Content line
                currentLine.lineContent = line.mid(1);
                currentLine.commitHash = currentCommitHash;
                currentLine.commitShortHash = currentCommitHash.left(7);
                result.append(currentLine);
                currentLine = GitBlameLine();
                inHeader = true;
            } else {
                // Header line
                QStringList parts = line.split(' ');
                if (parts.size() >= 2) {
                    if (parts[0].length() == 40) {
                        // First line of a group: commit hash
                        currentCommitHash = parts[0];
                        currentLine.lineNumber = parts[1].toInt();
                    } else if (parts[0] == "author") {
                        currentLine.authorName = line.mid(7);
                    } else if (parts[0] == "author-mail") {
                        currentLine.authorEmail = line.mid(12).trimmed();
                        currentLine.authorEmail.remove('<').remove('>');
                    } else if (parts[0] == "author-time") {
                        bool ok;
                        qint64 timestamp = parts[1].toLongLong(&ok);
                        if (ok) currentLine.authorDate = QDateTime::fromSecsSinceEpoch(timestamp);
                    } else if (parts[0] == "summary") {
                        currentLine.summary = line.mid(8);
                    } else if (parts[0] == "boundary") {
                        currentLine.isBoundary = true;
                    }
                }
            }
        }
    }

    return result;
}

// ============================================================================
// Stage/Unstage
// ============================================================================

bool GitRepo::stage(const QString& filePath) {
    if (!m_valid) return false;

    QStringList args = {"add", "--", relativeToRepo(filePath)};
    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("add", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::stageAll() {
    if (!m_valid) return false;

    QString output, error;
    bool success = executeGit({"add", "-A"}, &output, &error);
    if (!success) {
        emit errorOccurred("add", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::unstage(const QString& filePath) {
    if (!m_valid) return false;

    QStringList args = {"reset", "HEAD", "--", relativeToRepo(filePath)};
    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("reset", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::unstageAll() {
    if (!m_valid) return false;

    QString output, error;
    bool success = executeGit({"reset", "HEAD"}, &output, &error);
    if (!success) {
        emit errorOccurred("reset", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

// ============================================================================
// Commit
// ============================================================================

bool GitRepo::commit(const QString& message, const QString& author) {
    if (!m_valid) return false;

    QStringList args = {"commit", "-m", message};
    if (!author.isEmpty()) {
        args << "--author" << author;
    }

    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("commit", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::commitAmend(const QString& message) {
    if (!m_valid) return false;

    QStringList args = {"commit", "--amend", "-m", message, "--no-edit"};
    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("commit", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

// ============================================================================
// Remote
// ============================================================================

QVector<GitRemote> GitRepo::remotes() {
    if (!m_valid) return {};

    QString output, error;
    if (!executeGit({"remote", "-v"}, &output, &error)) {
        emit errorOccurred("remote", error);
        return {};
    }

    return parseRemotes(output);
}

bool GitRepo::fetch(const QString& remote, const QString& refspec) {
    if (!m_valid) return false;

    QStringList args = {"fetch", remote};
    if (!refspec.isEmpty()) args << refspec;

    QString output, error;
    bool success = executeGit(args, &output, &error, 60000);
    if (!success) {
        emit errorOccurred("fetch", error);
    } else {
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::pull(const QString& remote, const QString& branch) {
    if (!m_valid) return false;

    QStringList args = {"pull", remote};
    if (!branch.isEmpty()) args << branch;

    QString output, error;
    bool success = executeGit(args, &output, &error, 60000);
    if (!success) {
        emit errorOccurred("pull", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::push(const QString& remote, const QString& branch, bool force) {
    if (!m_valid) return false;

    QStringList args = {"push", remote};
    if (!branch.isEmpty()) args << branch;
    if (force) args << "--force";

    QString output, error;
    bool success = executeGit(args, &output, &error, 60000);
    if (!success) {
        emit errorOccurred("push", error);
    } else {
        emit repositoryChanged();
    }
    return success;
}

QVector<GitRemote> GitRepo::parseRemotes(const QString& output) {
    QVector<GitRemote> result;
    QMap<QString, GitRemote> remoteMap;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            QString name = parts[0];
            QString url = parts[1];
            QString type = parts[2]; // (fetch) or (push)

            if (!remoteMap.contains(name)) {
                remoteMap[name] = GitRemote();
                remoteMap[name].name = name;
            }

            if (type == "(fetch)") {
                remoteMap[name].fetchUrl = url;
            } else if (type == "(push)") {
                remoteMap[name].pushUrl = url;
            }
        }
    }

    result = remoteMap.values();
    return result;
}

// ============================================================================
// Stash
// ============================================================================

QVector<GitStashEntry> GitRepo::stashList() {
    if (!m_valid) return {};

    QString output, error;
    if (!executeGit({"stash", "list", "--pretty=format:%gd|%gs|%h|%ad"}, &output, &error)) {
        emit errorOccurred("stash", error);
        return {};
    }

    return parseStashList(output);
}

bool GitRepo::stashPush(const QString& message) {
    if (!m_valid) return false;

    QStringList args = {"stash", "push"};
    if (!message.isEmpty()) {
        args << "-m" << message;
    }
    args << "--include-untracked";

    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("stash", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::stashPop(int index) {
    if (!m_valid) return false;

    QStringList args = {"stash", "pop", QString("stash@{%1}").arg(index)};
    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("stash", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

bool GitRepo::stashDrop(int index) {
    if (!m_valid) return false;

    QStringList args = {"stash", "drop", QString("stash@{%1}").arg(index)};
    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("stash", error);
    }
    return success;
}

QVector<GitStashEntry> GitRepo::parseStashList(const QString& output) {
    QVector<GitStashEntry> result;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (int i = 0; i < lines.size(); ++i) {
        QStringList parts = lines[i].split('|');
        if (parts.size() >= 4) {
            GitStashEntry entry;
            entry.index = i;
            entry.ref = parts[0];
            entry.message = parts[1];
            entry.commitHash = parts[2];
            entry.commitShortHash = entry.commitHash.left(7);
            entry.date = QDateTime::fromString(parts[3], Qt::ISODate);

            // Get branch name from stash ref
            QRegularExpression re("stash@\\{(\\d+)\\}");
            QRegularExpressionMatch match = re.match(entry.ref);
            if (match.hasMatch()) {
                // We'd need git stash show to get branch, skip for now
            }

            result.append(entry);
        }
    }

    return result;
}

// ============================================================================
// Reset
// ============================================================================

bool GitRepo::reset(const QString& target, bool hard) {
    if (!m_valid) return false;

    QStringList args = {"reset"};
    if (hard) args << "--hard";
    else args << "--mixed";
    args << target;

    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("reset", error);
    } else {
        invalidateStatusCache();
        emit repositoryChanged();
    }
    return success;
}

// ============================================================================
// Config
// ============================================================================

QVector<GitConfigEntry> GitRepo::configList(const QString& scope) {
    if (!m_valid) return {};

    QStringList args = {"config", "--list"};
    if (!scope.isEmpty()) args << QString("--%1").arg(scope);

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        emit errorOccurred("config", error);
        return {};
    }

    return parseConfigList(output);
}

bool GitRepo::configSet(const QString& key, const QString& value, const QString& scope) {
    if (!m_valid) return false;

    QStringList args = {"config", QString("--%1").arg(scope), key, value};
    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("config", error);
    }
    return success;
}

bool GitRepo::configUnset(const QString& key, const QString& scope) {
    if (!m_valid) return false;

    QStringList args = {"config", QString("--%1").arg(scope), "--unset", key};
    QString output, error;
    bool success = executeGit(args, &output, &error);
    if (!success) {
        emit errorOccurred("config", error);
    }
    return success;
}

QString GitRepo::configGet(const QString& key, const QString& scope) {
    if (!m_valid) return {};

    QStringList args = {"config"};
    if (!scope.isEmpty()) args << QString("--%1").arg(scope);
    args << "--get" << key;

    QString output, error;
    if (!executeGit(args, &output, &error)) {
        return {};
    }
    return output.trimmed();
}

QVector<GitConfigEntry> GitRepo::parseConfigList(const QString& output) {
    QVector<GitConfigEntry> result;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        int eqPos = line.indexOf('=');
        if (eqPos > 0) {
            GitConfigEntry entry;
            entry.key = line.left(eqPos).trimmed();
            entry.value = line.mid(eqPos + 1).trimmed();
            result.append(entry);
        }
    }

    return result;
}

// ============================================================================
// Utility
// ============================================================================

bool GitRepo::init(const QString& initialBranch) {
    QString output, error;
    QStringList args = {"init"};
    if (!initialBranch.isEmpty()) {
        args << "--initial-branch" << initialBranch;
    }

    bool success = executeGitInDir(m_repoPath, args, &output, &error, 30000);
    if (success) {
        m_valid = true;
        m_gitDir = m_repoPath + "/.git";
        m_workTree = m_repoPath;
        emit repositoryChanged();
    } else {
        emit errorOccurred("init", error);
    }
    return success;
}

bool GitRepo::isBare() const {
    if (!m_valid) return false;
    QString output, error;
    if (executeGit({"rev-parse", "--is-bare-repository"}, &output, &error)) {
        return output.trimmed() == "true";
    }
    return false;
}

QString GitRepo::rootDir() const {
    return m_workTree;
}

QStringList GitRepo::submodules() const {
    if (!m_valid) return {};

    QString output, error;
    if (!executeGit({"submodule", "status"}, &output, &error)) {
        return {};
    }

    QStringList result;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            result << parts[1];
        }
    }
    return result;
}

} // namespace papiro