#pragma once

#include "core/Encoding.h"
#include "core/GitTypes.h"

#include <QWidget>

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
    QString m_path;
    TextEncoding m_encoding = TextEncoding::Utf8;
    LineEnding m_eol = LineEnding::Lf;
};

} // namespace papiro
