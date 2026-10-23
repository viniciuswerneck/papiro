#ifndef GITTYPES_H
#define GITTYPES_H

#include <QString>
#include <QVector>
#include <QDateTime>
#include <QMap>

namespace papiro {

// ============================================================================
// Git File Status
// ============================================================================

enum class GitFileStatusType {
    Unmodified = 0,
    Added,
    Modified,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Ignored,
    Unmerged,
    TypeChange
};

struct GitFileStatus {
    QString filePath;              // Path relative to repo root
    GitFileStatusType indexStatus = GitFileStatusType::Unmodified;   // Staged status
    GitFileStatusType workTreeStatus = GitFileStatusType::Unmodified; // Working tree status
    QString renamedFrom;           // For renamed files: original path
    QString renamedTo;             // For renamed files: new path

    bool isStaged() const { return indexStatus != GitFileStatusType::Unmodified; }
    bool isUnstaged() const { return workTreeStatus != GitFileStatusType::Unmodified; }
    bool isClean() const { return !isStaged() && !isUnstaged(); }

    static GitFileStatusType parseStatusChar(QChar c);
};

// ============================================================================
// Git Branch
// ============================================================================

enum class GitBranchType {
    Local = 0,
    Remote,
    Tag
};

struct GitBranch {
    QString name;                  // Branch name (e.g., "main", "origin/main")
    QString displayName;           // Display name (e.g., "main", "main [origin/main]")
    GitBranchType type = GitBranchType::Local;
    QString commitHash;            // Current commit hash
    QString commitShortHash;       // Short commit hash
    QString upstreamBranch;        // Upstream branch name (for local branches)
    bool isCurrent = false;        // Is this the current HEAD?
    bool isRemote = false;         // Is this a remote branch?
    QString remoteName;            // Remote name (e.g., "origin")
    int ahead = 0;                 // Commits ahead of upstream
    int behind = 0;                // Commits behind upstream
};

// ============================================================================
// Git Diff
// ============================================================================

struct GitDiffHunk {
    int oldStart = 0;
    int oldLines = 0;
    int newStart = 0;
    int newLines = 0;
    QString header;                // @@ -oldStart,oldLines +newStart,newLines @@
    QVector<QString> lines;        // Diff lines (with +/-/ prefix)
};

struct GitDiffFile {
    QString filePath;              // Path relative to repo root
    QString oldFilePath;           // For renames: old path
    QString newFilePath;           // For renames: new path
    GitFileStatusType status = GitFileStatusType::Modified;
    bool isBinary = false;
    QVector<GitDiffHunk> hunks;
    int additions = 0;
    int deletions = 0;

    int totalLinesChanged() const { return additions + deletions; }
};

// ============================================================================
// Git Commit
// ============================================================================

struct GitCommit {
    QString hash;                  // Full commit hash
    QString shortHash;             // Short commit hash (7-8 chars)
    QString authorName;
    QString authorEmail;
    QDateTime authorDate;
    QString committerName;
    QString committerEmail;
    QDateTime committerDate;
    QString subject;               // First line of commit message
    QString body;                  // Rest of commit message
    QVector<QString> parentHashes; // Parent commit hashes
    QVector<QString> refs;         // Branch/tag refs pointing to this commit

    bool isMerge() const { return parentHashes.size() > 1; }
    bool isRoot() const { return parentHashes.isEmpty(); }
};

// ============================================================================
// Git Blame
// ============================================================================

struct GitBlameLine {
    int lineNumber = 0;            // Line number in file (1-based)
    QString commitHash;            // Commit hash that last modified this line
    QString commitShortHash;
    QString authorName;
    QString authorEmail;
    QDateTime authorDate;
    QString summary;               // Commit subject
    QString lineContent;           // Actual line content
    bool isBoundary = false;       // Line from boundary commit (e.g., initial commit)
};

// ============================================================================
// Git Remote
// ============================================================================

struct GitRemote {
    QString name;                  // Remote name (e.g., "origin")
    QString fetchUrl;              // Fetch URL
    QString pushUrl;               // Push URL (may differ from fetch)
    QStringList fetchRefspecs;     // Configured fetch refspecs
    QStringList pushRefspecs;      // Configured push refspecs
};

// ============================================================================
// Git Stash
// ============================================================================

struct GitStashEntry {
    int index = 0;                 // Stash index (0 = latest)
    QString ref;                   // Stash ref (e.g., "stash@{0}")
    QString message;               // Stash message
    QString commitHash;            // Commit hash of stash
    QString commitShortHash;
    QString branchName;            // Branch where stash was created
    QDateTime date;
    bool hasUntracked = false;     // Whether stash includes untracked files
};

// ============================================================================
// Git Config
// ============================================================================

struct GitConfigEntry {
    QString key;
    QString value;
    QString scope;                 // "system", "global", "local", "worktree"
    QString file;                  // Config file path
};

// ============================================================================
// Helper functions
// ============================================================================

inline QString gitStatusTypeToString(GitFileStatusType type) {
    switch (type) {
        case GitFileStatusType::Unmodified: return "unmodified";
        case GitFileStatusType::Added: return "added";
        case GitFileStatusType::Modified: return "modified";
        case GitFileStatusType::Deleted: return "deleted";
        case GitFileStatusType::Renamed: return "renamed";
        case GitFileStatusType::Copied: return "copied";
        case GitFileStatusType::Untracked: return "untracked";
        case GitFileStatusType::Ignored: return "ignored";
        case GitFileStatusType::Unmerged: return "unmerged";
        case GitFileStatusType::TypeChange: return "typechange";
    }
    return "unknown";
}

inline QString gitBranchTypeToString(GitBranchType type) {
    switch (type) {
        case GitBranchType::Local: return "local";
        case GitBranchType::Remote: return "remote";
        case GitBranchType::Tag: return "tag";
    }
    return "unknown";
}

} // namespace papiro

Q_DECLARE_METATYPE(papiro::GitFileStatus)
Q_DECLARE_METATYPE(papiro::GitBranch)
Q_DECLARE_METATYPE(papiro::GitDiffFile)
Q_DECLARE_METATYPE(papiro::GitCommit)
Q_DECLARE_METATYPE(papiro::GitBlameLine)
Q_DECLARE_METATYPE(papiro::GitRemote)
Q_DECLARE_METATYPE(papiro::GitStashEntry)
Q_DECLARE_METATYPE(papiro::GitConfigEntry)

#endif // GITTYPES_H