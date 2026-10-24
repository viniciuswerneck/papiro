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
    bool saveFile(const QString &path, QString *error = nullptr);

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
};

} // namespace papiro
