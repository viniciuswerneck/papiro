#pragma once

#include "core/Encoding.h"
#include "core/GitTypes.h"

#include <QWidget>

class QLabel;

namespace papiro {

class Editor;
class GitRepo;

/*! One open document: an Editor plus its on-disk identity and text metadata. */
class Document : public QWidget {
    Q_OBJECT

public:
    explicit Document(QWidget *parent = nullptr);

    Editor *editor() const { return m_editor; }

    bool loadFile(const QString &path, QString *error = nullptr);
    bool loadFile(const QString &path, qint64 largeFileThresholdBytes, QString *error = nullptr);
    bool saveFile(const QString &path, QString *error = nullptr);

    /*! Set the large file threshold in bytes. */
    void setLargeFileThreshold(qint64 bytes) { m_largeFileThreshold = bytes; }

    QString filePath() const { return m_path; }
    QString displayName() const;
    QString displayPath() const;

    bool isModified() const;
    void setModified(bool modified);

    /*! True when the file was opened in bounded, read-only preview mode. */
    bool isLargeFile() const { return m_largeFile; }
    qint64 largeFileSize() const { return m_largeFileSize; }
    /*! Bytes not loaded into the editor (0 when the whole file fit). */
    qint64 truncatedBytes() const { return m_truncatedBytes; }

    /*! Load additional content from large file (for virtual scrolling). */
    void loadMoreLargeFileContent(qint64 maxBytes = 1024 * 1024);
    /*! Jump to a byte offset in the large file (reads surrounding window). */
    bool seekLargeFile(qint64 offset, QString *error = nullptr);
    /*! Load a specific chunk from the large file. */
    qint64 loadLargeFileChunk(qint64 offset, qint64 maxBytes, QString *error = nullptr);

    TextEncoding encoding() const { return m_encoding; }
    void setEncoding(TextEncoding encoding);

    LineEnding lineEnding() const { return m_eol; }
    void setLineEnding(LineEnding eol);

    void applySyntaxFor(const QString &fileName);
    void requestGitBlame(GitRepo* gitRepo);

signals:
    void modificationChanged(bool modified);
    void pathChanged(const QString &path);
    void gitBlameReady(const QVector<GitBlameLine>& blameLines);

private:
    Editor *m_editor;
    QLabel *m_largeFileBanner = nullptr;
    QString m_path;
    TextEncoding m_encoding = TextEncoding::Utf8;
    LineEnding m_eol = LineEnding::Lf;
    bool m_largeFile = false;
    qint64 m_largeFileSize = 0;
    qint64 m_truncatedBytes = 0;
    qint64 m_largeFileThreshold;
};

} // namespace papiro
