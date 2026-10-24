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

    // Error parsing
    QString errorPattern;          // Regex pattern to match file:line:col (e.g., "([^:]+):(\\d+):(\\d+)")
    QString errorFilePattern;      // Capture group index for file path (e.g., "1")
    QString errorLinePattern;      // Capture group index for line number (e.g., "2")
    QString errorColumnPattern;    // Capture group index for column (e.g., "3")

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
