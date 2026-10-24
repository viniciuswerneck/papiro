#pragma once

#include "LSPTypes.h"
#include "LSPClient.h"

#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <map>
#include <memory>

namespace papiro {

class Document;

/*!
 * \brief Configuration for a single language server.
 */
struct LSPServerConfig {
    QString name;                   // "clangd", "pyright", etc.
    QString languageId;             // "cpp", "python", etc.
    QString command;                // Executable name or path
    QStringList arguments;          // Command line arguments
    QString workingDirectory;       // Working directory (empty = auto)
    QMap<QString, QString> env;     // Environment variables
    bool enabled = true;            // Whether this server is enabled
    QString fileExtensions;         // Comma-separated extensions: ".cpp,.h,.hpp"

    bool matchesFile(const QString &filePath) const;
};

/*!
 * \brief LSPManager manages multiple LSP clients for different languages.
 *
 * It handles:
 * - Loading/saving server configurations via QSettings
 * - Creating and caching LSPClient instances per language
 * - Routing documents to the appropriate client
 * - Aggregating diagnostics from all clients
 * - Workspace folder management
 */
class LSPManager : public QObject {
    Q_OBJECT

public:
    explicit LSPManager(QObject *parent = nullptr);
    ~LSPManager() override;

    // --- Configuration ---

    /*! Load server configurations from QSettings (group "LSP"). */
    void loadSettings();

    /*! Save current server configurations to QSettings. */
    void saveSettings();

    /*! Set or update a server configuration. */
    void setServerConfig(const LSPServerConfig &config);

    /*! Remove a server configuration by language ID. */
    void removeServerConfig(const QString &languageId);

    /*! Get all server configurations. */
    const QMap<QString, LSPServerConfig> &serverConfigs() const { return m_configs; }

    /*! Get configuration for a specific language ID. */
    const LSPServerConfig *configForLanguage(const QString &languageId) const;

    /*! Get configuration matching a file path. */
    const LSPServerConfig *configForFile(const QString &filePath) const;

    // --- Client Management ---

    /*! Get or create a client for the given language ID. Returns nullptr if no config exists or disabled. */
    LSPClient *getOrCreateClient(const QString &languageId);

    /*! Get the client associated with a document. Returns nullptr if no client matches. */
    LSPClient *clientForDocument(Document *document) const;

    /*! Release the client association for a document (call when document closes). */
    void releaseClientForDocument(Document *document);

    // --- Workspace Folders ---

    /*! Set workspace folders for all clients. */
    void setWorkspaceFolders(const QVector<LSP::WorkspaceFolder> &folders);

    /*! Get current workspace folders. */
    QVector<LSP::WorkspaceFolder> workspaceFolders() const { return m_workspaceFolders; }

    // --- Utility ---

    /*! Get language ID for a file path based on extension matching. */
    QString languageIdForFile(const QString &filePath) const;

    /*! Check if a language has an enabled and configured server. */
    bool hasServerForLanguage(const QString &languageId) const;

    /*! Stop all running clients. */
    void stopAllClients();

    /*! Get default server configurations. */
    static QMap<QString, LSPServerConfig> defaultServerConfigs();

signals:
    /*! Emitted when a client starts successfully. */
    void clientStarted(LSPClient *client, const QString &languageId);

    /*! Emitted when a client stops. */
    void clientStopped(LSPClient *client, const QString &languageId);

    /*! Emitted when a client encounters an error. */
    void clientError(LSPClient *client, const QString &error);

    /*! Emitted when diagnostics are received from any client. */
    void diagnosticsReceived(const QString &uri, const QVector<LSP::Diagnostic> &diagnostics);

private:
    void setupDefaultConfigs();
    void applyConfigToClient(LSPClient *client, const LSPServerConfig &config);
    QString fileExtension(const QString &filePath) const;

    QMap<QString, LSPServerConfig> m_configs;
    std::map<QString, std::unique_ptr<LSPClient>> m_clients;
    QMap<Document *, QString> m_documentToLanguage;
    QVector<LSP::WorkspaceFolder> m_workspaceFolders;
    bool m_settingsLoaded = false;
};

} // namespace papiro