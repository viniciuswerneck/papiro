#include "core/ExternalTool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace papiro {
namespace {

QJsonObject toJson(const ExternalTool &tool)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), tool.name);
    object.insert(QStringLiteral("command"), tool.command);
    object.insert(QStringLiteral("arguments"), tool.arguments);
    object.insert(QStringLiteral("workingDirectory"), tool.workingDirectory);
    object.insert(QStringLiteral("saveBeforeRun"), tool.saveBeforeRun);
    return object;
}

ExternalTool fromJson(const QJsonObject &object)
{
    ExternalTool tool;
    tool.name = object.value(QStringLiteral("name")).toString();
    tool.command = object.value(QStringLiteral("command")).toString();
    tool.arguments = object.value(QStringLiteral("arguments")).toString();
    tool.workingDirectory = object.value(QStringLiteral("workingDirectory")).toString();
    tool.saveBeforeRun = object.value(QStringLiteral("saveBeforeRun")).toBool(true);
    return tool;
}

} // namespace

ToolManager::ToolManager(QObject *parent)
    : QObject(parent)
{
}

QString ToolManager::defaultFile()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config/Papiro");
    return base + QStringLiteral("/tools.json");
}

void ToolManager::setTools(const QList<ExternalTool> &tools)
{
    m_tools = tools;
}

void ToolManager::addTool(const ExternalTool &tool)
{
    m_tools.append(tool);
}

void ToolManager::replaceTool(int index, const ExternalTool &tool)
{
    if (index >= 0 && index < m_tools.size())
        m_tools[index] = tool;
}

void ToolManager::removeTool(int index)
{
    if (index >= 0 && index < m_tools.size())
        m_tools.removeAt(index);
}

bool ToolManager::load()
{
    QFile file(defaultFile());
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1)
        return false;

    QList<ExternalTool> tools;
    const QJsonArray array = root.value(QStringLiteral("tools")).toArray();
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            continue;
        const ExternalTool tool = fromJson(value.toObject());
        if (tool.isValid())
            tools.append(tool);
    }
    m_tools = tools;
    return true;
}

bool ToolManager::save() const
{
    const QString path = defaultFile();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray array;
    for (const ExternalTool &tool : m_tools)
        array.append(toJson(tool));

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("tools"), array);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace papiro
