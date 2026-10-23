#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace papiro {

/*! One configurable external command shown in the Tools menu. */
struct ExternalTool {
    QString name;
    QString command;
    QString arguments;
    QString workingDirectory; // empty: directory of the current document
    bool saveBeforeRun = true;

    bool isValid() const { return !name.trimmed().isEmpty() && !command.trimmed().isEmpty(); }
};

/*! Persists the user's external tool definitions as JSON. */
class ToolManager : public QObject {
    Q_OBJECT

public:
    explicit ToolManager(QObject *parent = nullptr);

    static QString defaultFile();

    const QList<ExternalTool> &tools() const { return m_tools; }
    void setTools(const QList<ExternalTool> &tools);
    void addTool(const ExternalTool &tool);
    void replaceTool(int index, const ExternalTool &tool);
    void removeTool(int index);

    bool load();
    bool save() const;

private:
    QList<ExternalTool> m_tools;
};

} // namespace papiro
