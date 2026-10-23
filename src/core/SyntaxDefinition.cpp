#include "core/SyntaxDefinition.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace papiro {
namespace {

QStringList splitList(const QString &value, bool splitWhitespace)
{
    QString normalized = value;
    normalized.replace(QLatin1Char(';'), QLatin1Char(','));
    QStringList parts;
    if (splitWhitespace) {
        for (const QString &chunk : normalized.split(QRegularExpression(QStringLiteral("[\\s,]+")),
                                                     Qt::SkipEmptyParts)) {
            parts.append(chunk);
        }
    } else {
        parts = normalized.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString &part : parts)
            part = part.trimmed();
        parts.removeAll(QString());
    }
    return parts;
}

bool toBool(const QString &value)
{
    const QString v = value.trimmed().toLower();
    return v == QLatin1String("1") || v == QLatin1String("true") || v == QLatin1String("yes")
        || v == QLatin1String("on");
}

QString unquote(const QString &value)
{
    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('"') && last == QLatin1Char('"'))
            || (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            return value.mid(1, value.size() - 2);
        }
    }
    return value;
}

} // namespace

bool SyntaxDefinition::matchesFile(const QString &fileName) const
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    for (const QString &extension : extensions) {
        const QString candidate = extension.startsWith(QLatin1Char('.'))
            ? extension.mid(1).toLower()
            : extension.toLower();
        if (!candidate.isEmpty() && candidate == suffix)
            return true;
    }
    return false;
}

SyntaxDefinition SyntaxDefinition::fromText(const QString &text, QString *error)
{
    SyntaxDefinition definition;
    definition.extensions.clear();
    definition.keywords.clear();
    definition.stringDelimiters.clear();

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int index = 0; index < lines.size(); ++index) {
        const QString raw = lines.at(index);
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')))
            continue;

        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            if (error && error->isEmpty())
                *error = QStringLiteral("Line %1 is not a Key=Value pair").arg(index + 1);
            continue;
        }

        const QString key = line.left(equals).trimmed().toLower();
        const QString value = unquote(line.mid(equals + 1).trimmed());

        if (key == QLatin1String("name")) {
            definition.name = value;
        } else if (key == QLatin1String("description")) {
            definition.description = value;
        } else if (key == QLatin1String("extensions")) {
            definition.extensions = splitList(value, false);
        } else if (key == QLatin1String("casesensitive")) {
            definition.caseSensitive = toBool(value);
        } else if (key == QLatin1String("keywords")) {
            definition.keywords = splitList(value, true);
        } else if (key == QLatin1String("linecomment")) {
            definition.lineComment = value;
        } else if (key == QLatin1String("blockcommentstart")) {
            definition.blockCommentStart = value;
        } else if (key == QLatin1String("blockcommentend")) {
            definition.blockCommentEnd = value;
        } else if (key == QLatin1String("stringdelimiters")) {
            definition.stringDelimiters = splitList(value, true);
        } else if (key == QLatin1String("escapechar")) {
            definition.escapeChar = value;
        } else if (key == QLatin1String("numbers")) {
            definition.highlightNumbers = toBool(value);
        }
    }

    if (!definition.blockCommentStart.isEmpty() && definition.blockCommentEnd.isEmpty())
        definition.blockCommentEnd = definition.blockCommentStart;

    return definition;
}

SyntaxDefinition SyntaxDefinition::fromFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return SyntaxDefinition();
    }
    return fromText(QString::fromUtf8(file.readAll()), error);
}

SyntaxRepository &SyntaxRepository::instance()
{
    static SyntaxRepository repository;
    return repository;
}

QString SyntaxRepository::userDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/syntax");
}

void SyntaxRepository::load(const QString &userDir)
{
    m_definitions.clear();

    const QString directory = userDir.isEmpty() ? userDirectory() : userDir;
    QDir userDefinitions(directory);
    const QStringList userFiles = userDefinitions.entryList({QStringLiteral("*.syn")}, QDir::Files,
                                                            QDir::Name);
    for (const QString &name : userFiles) {
        QString error;
        SyntaxDefinition definition = SyntaxDefinition::fromFile(userDefinitions.filePath(name),
                                                                 &error);
        if (!definition.extensions.isEmpty())
            m_definitions.append(definition);
    }

    QDir builtins(QStringLiteral(":/syntax"));
    const QStringList builtinFiles = builtins.entryList({QStringLiteral("*.syn")}, QDir::Files,
                                                        QDir::Name);
    for (const QString &name : builtinFiles) {
        QString error;
        SyntaxDefinition definition = SyntaxDefinition::fromFile(builtins.filePath(name), &error);
        if (!definition.extensions.isEmpty())
            m_definitions.append(definition);
    }
}

const SyntaxDefinition *SyntaxRepository::forFile(const QString &fileName) const
{
    for (const SyntaxDefinition &definition : m_definitions) {
        if (definition.matchesFile(fileName))
            return &definition;
    }
    return nullptr;
}

} // namespace papiro
