#include "core/Workspace.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace papiro {
namespace {

constexpr int kVersion = 1;

} // namespace

QString Workspace::fileFilter()
{
    return QStringLiteral("Papiro Workspace (*.ppw);;All Files (*)");
}

QString Workspace::defaultSuffix()
{
    return QStringLiteral("ppw");
}

Workspace Workspace::load(const QString &path, QString *error)
{
    Workspace workspace;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open %1 for reading: %2").arg(path, file.errorString());
        return workspace;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid JSON in %1: %2").arg(path, parseError.errorString());
        return workspace;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != kVersion) {
        if (error) {
            *error = QStringLiteral("Unsupported workspace version %1 (expected %2)")
                         .arg(root.value(QStringLiteral("version")).toInt())
                         .arg(kVersion);
        }
        return workspace;
    }

    workspace.name = root.value(QStringLiteral("name")).toString();
    workspace.rootPath = root.value(QStringLiteral("root")).toString();
    workspace.activeIndex = root.value(QStringLiteral("active")).toInt(0);

    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    for (const QJsonValue &value : files) {
        const QString filePath = value.toString();
        if (!filePath.isEmpty())
            workspace.files.append(filePath);
    }

    return workspace;
}

bool Workspace::save(const QString &path, QString *error) const
{
    QJsonArray files;
    for (const QString &file : this->files)
        files.append(file);

    QJsonObject root;
    root.insert(QStringLiteral("version"), kVersion);
    root.insert(QStringLiteral("name"), name);
    root.insert(QStringLiteral("root"), rootPath);
    root.insert(QStringLiteral("active"), activeIndex);
    root.insert(QStringLiteral("files"), files);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open %1 for writing: %2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

} // namespace papiro
