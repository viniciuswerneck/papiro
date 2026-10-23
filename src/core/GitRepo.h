#ifndef GITREPO_H
#define GITREPO_H

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include "GitTypes.h"

namespace papiro {

class GitRepo : public QObject {
    Q_OBJECT

public:
    explicit GitRepo(const QString& repoPath, QObject* parent = nullptr);
    ~GitRepo() override;

    // Repository info
    bool isValid() const;
    QString repoPath() const;
    QString gitDir() const;
    QString workTree() const;

    // Status
    QVector<GitFileStatus> status(bool includeUntracked = true, bool includeIgnored = false);
    GitFileStatus fileStatus(const QString& filePath);

    // Branch
    QVector<GitBranch> branches();
    GitBranch currentBranch();
    bool checkoutBranch(const QString& branchName, bool create = false);
    bool createBranch(const QString& branchName, const QString& startPoint = {});
    bool deleteBranch(const QString& branchName, bool force = false);

    // Diff
    QVector<GitDiffFile> diff(const QString& filePath = {}, bool staged = false);
    QVector<GitDiffFile> diffCached(const QString& filePath = {});
    QVector<GitDiffFile> diffHEAD(const QString& filePath = {});

    // Log/History
    QVector<GitCommit> log(int limit = 50, const QString& since = {}, const QString& until = {}, const QString& filePath = {});
    QVector<GitCommit> logAll(int limit = 100);

    // Blame
    QVector<GitBlameLine> blame(const QString& filePath, int startLine = 1, int endLine = -1);

    // Stage/Unstage
    bool stage(const QString& filePath);
    bool stageAll();
    bool unstage(const QString& filePath);
    bool unstageAll();

    // Commit
    bool commit(const QString& message, const QString& author = {});
    bool commitAmend(const QString& message);

    // Remote
    QVector<GitRemote> remotes();
    bool fetch(const QString& remote = "origin", const QString& refspec = {});
    bool pull(const QString& remote = "origin", const QString& branch = {});
    bool push(const QString& remote = "origin", const QString& branch = {}, bool force = false);

    // Stash
    QVector<GitStashEntry> stashList();
    bool stashPush(const QString& message = {});
    bool stashPop(int index = 0);
    bool stashDrop(int index = 0);

    // Reset
    bool reset(const QString& target, bool hard = false);

    // Config
    QVector<GitConfigEntry> configList(const QString& scope = {});
    bool configSet(const QString& key, const QString& value, const QString& scope = "local");
    bool configUnset(const QString& key, const QString& scope = "local");
    QString configGet(const QString& key, const QString& scope = {});

    // Utility
    bool init(const QString& initialBranch = "main");
    bool isBare() const;
    QString rootDir() const;
    QStringList submodules() const;

    // Async operations
    using GitCallback = std::function<void(const QString& output, const QString& error, bool success)>;
    void executeAsync(const QStringList& args, GitCallback callback);

signals:
    void repositoryChanged();
    void errorOccurred(const QString& operation, const QString& error);
    void progressUpdated(const QString& operation, int current, int total);

private:
    // Internal helpers
    bool executeGit(const QStringList& args, QString* output = nullptr, QString* error = nullptr, int timeoutMs = 30000) const;
    bool executeGitInDir(const QString& workingDir, const QStringList& args, QString* output, QString* error, int timeoutMs = 30000) const;

    // Parsers
    QVector<GitFileStatus> parseStatus(const QString& output);
    QVector<GitBranch> parseBranches(const QString& output);
    QVector<GitDiffFile> parseDiff(const QString& output);
    QVector<GitCommit> parseLog(const QString& output);
    QVector<GitBlameLine> parseBlame(const QString& output);
    QVector<GitRemote> parseRemotes(const QString& output);
    QVector<GitStashEntry> parseStashList(const QString& output);
    QVector<GitConfigEntry> parseConfigList(const QString& output);

    // Cache
    mutable QMap<QString, QVector<GitFileStatus>> m_statusCache;
    mutable bool m_statusCacheValid = false;
    void invalidateStatusCache() const;

    // Path helpers
    QString normalizePath(const QString& path) const;
    QString relativeToRepo(const QString& path) const;

    QString m_repoPath;
    QString m_gitDir;
    QString m_workTree;
    mutable bool m_valid = false;
};

} // namespace papiro

#endif // GITREPO_H