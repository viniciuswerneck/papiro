#include "app/Document.h"

#include "core/SyntaxDefinition.h"
#include "editor/Editor.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QVBoxLayout>

namespace papiro {

Document::Document(QWidget *parent)
    : QWidget(parent)
    , m_editor(new Editor(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
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

    const QByteArray data = file.readAll();
    const DecodedFile decoded = decodeFile(data);

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
    const SyntaxDefinition *definition = SyntaxRepository::instance().forFile(fileName);
    if (definition)
        m_editor->applySyntax(*definition);
    else
        m_editor->clearSyntax();
}

} // namespace papiro
