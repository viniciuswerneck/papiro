#include "app/Document.h"

#include "core/GitRepo.h"
#include "core/SyntaxDefinition.h"
#include "editor/Editor.h"

#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPalette>
#include <QSaveFile>
#include <QVBoxLayout>

namespace papiro {

namespace {

/*! Files above this size open in read-only "large file" mode. */
constexpr qint64 kLargeFileThreshold = 16LL * 1024 * 1024;
/*! Maximum amount of a large file decoded into the editor. */
constexpr qint64 kLargeFilePreview = 8LL * 1024 * 1024;

qint64 largeFileThreshold()
{
    bool ok = false;
    const int megabytes =
        qEnvironmentVariableIntValue("PAPIRO_LARGE_FILE_THRESHOLD_MB", &ok);
    if (ok && megabytes > 0)
        return qint64(megabytes) * 1024 * 1024;
    return kLargeFileThreshold;
}

QString formatSize(qint64 bytes)
{
    if (bytes >= 1024LL * 1024 * 1024)
        return QStringLiteral("%1 GB").arg(double(bytes) / (1024.0 * 1024 * 1024), 0, 'f', 1);
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024), 0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 bytes").arg(bytes);
}

} // namespace

Document::Document(QWidget *parent)
    : QWidget(parent)
    , m_editor(new Editor(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_largeFileBanner = new QLabel(this);
    m_largeFileBanner->setObjectName(QStringLiteral("LargeFileBanner"));
    m_largeFileBanner->setWordWrap(true);
    m_largeFileBanner->setVisible(false);
    m_largeFileBanner->setAutoFillBackground(true);
    QPalette bannerPalette = m_largeFileBanner->palette();
    bannerPalette.setColor(QPalette::Window, QColor(255, 243, 205));
    bannerPalette.setColor(QPalette::WindowText, QColor(102, 77, 3));
    m_largeFileBanner->setPalette(bannerPalette);
    m_largeFileBanner->setContentsMargins(8, 6, 8, 6);
    layout->addWidget(m_largeFileBanner);
    layout->addWidget(m_editor);

    connect(m_editor->document(), &QTextDocument::modificationChanged, this,
            &Document::modificationChanged);
}

bool Document::loadFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }

    const qint64 fileSize = file.size();
    const bool large = fileSize > largeFileThreshold();

    if (large) {
        // Map the file instead of reading it into memory. Only a bounded
        // preview is decoded into the editor; the rest stays on disk.
        uchar *mapped = file.map(0, fileSize);
        if (!mapped) {
            if (error)
                *error = QStringLiteral("%1: %2").arg(path, file.errorString());
            return false;
        }

        const qint64 previewSize = qMin(fileSize, kLargeFilePreview);
        const QByteArray preview =
            QByteArray::fromRawData(reinterpret_cast<const char *>(mapped), int(previewSize));
        const DecodedFile decoded = decodeFile(preview);
        file.unmap(mapped);

        m_editor->setReadOnly(true);
        m_editor->clearSyntax();
        m_editor->setPlainText(decoded.text);
        m_encoding = decoded.encoding;
        m_eol = decoded.eol;
        m_path = QFileInfo(path).absoluteFilePath();

        m_largeFile = true;
        m_largeFileSize = fileSize;
        m_truncatedBytes = fileSize - previewSize;
        m_largeFileBanner->setText(tr(
            "Large file (%1): showing the first %2 in read-only mode. "
            "Editing and syntax highlighting are disabled; saving is blocked.")
            .arg(formatSize(fileSize), formatSize(previewSize)));
        m_largeFileBanner->setVisible(true);

        m_editor->document()->setModified(false);
        emit pathChanged(m_path);
        return true;
    }

    const QByteArray data = file.readAll();
    const DecodedFile decoded = decodeFile(data);

    m_largeFile = false;
    m_largeFileSize = 0;
    m_truncatedBytes = 0;
    m_largeFileBanner->setVisible(false);
    m_editor->setReadOnly(false);

    m_editor->setPlainText(decoded.text);
    m_encoding = decoded.encoding;
    m_eol = decoded.eol;
    m_path = QFileInfo(path).absoluteFilePath();

    m_editor->document()->setModified(false);
    applySyntaxFor(path);
    emit pathChanged(m_path);
    return true;
}

bool Document::saveFile(const QString &path, QString *error)
{
    if (m_largeFile && m_truncatedBytes > 0) {
        if (error)
            *error = tr("This is a large file opened in read-only preview mode. "
                        "Saving it would truncate the file, so the operation was blocked.");
        return false;
    }

    const QByteArray data = encodeFile(m_editor->toPlainText(), m_encoding, m_eol);

    QSaveFile file(path);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }

    const QString absolute = QFileInfo(path).absoluteFilePath();
    const bool pathChanged = absolute != m_path;
    m_path = absolute;
    m_editor->document()->setModified(false);
    if (pathChanged)
        emit this->pathChanged(m_path);
    return true;
}

QString Document::displayName() const
{
    if (m_path.isEmpty())
        return tr("Untitled");
    return QFileInfo(m_path).fileName();
}

QString Document::displayPath() const
{
    return m_path;
}

bool Document::isModified() const
{
    return m_editor->document()->isModified();
}

void Document::setModified(bool modified)
{
    m_editor->document()->setModified(modified);
}

void Document::setEncoding(TextEncoding encoding)
{
    if (m_encoding == encoding)
        return;
    m_encoding = encoding;
    m_editor->document()->setModified(true);
}

void Document::setLineEnding(LineEnding eol)
{
    if (m_eol == eol)
        return;
    m_eol = eol;
    m_editor->document()->setModified(true);
}

void Document::applySyntaxFor(const QString &fileName)
{
    if (m_largeFile) {
        m_editor->clearSyntax();
        return;
    }

    const SyntaxDefinition *definition = SyntaxRepository::instance().forFile(fileName);
    if (definition)
        m_editor->applySyntax(*definition);
    else
        m_editor->clearSyntax();
}

void Document::requestGitBlame(GitRepo* gitRepo)
{
    if (!gitRepo || !gitRepo->isValid() || m_path.isEmpty())
        return;

    QString relativePath = gitRepo->relativeToRepo(m_path);
    
    // Use async blame for better performance on large files
    gitRepo->blameAsync(relativePath, 1, -1, [this](const QVector<GitBlameLine>& blameLines) {
        emit gitBlameReady(blameLines);
    });
}

} // namespace papiro
