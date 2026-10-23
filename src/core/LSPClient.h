#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QMap>
#include <QTimer>
#include <functional>
#include <optional>
#include <variant>
#include "LSPTypes.h"
#include "LSPMessage.h"

class LSPClient : public QObject {
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Starting,
        Initializing,
        Ready,
        Error
    };

    explicit LSPClient(const QString& languageId = {}, QObject* parent = nullptr);
    ~LSPClient() override;

    QString languageId() const { return m_languageId; }

    // Lifecycle
    bool start(const QString& command, const QStringList& args, const QString& workingDir = {});
    void stop();  // graceful: shutdown request + wait + exit notification
    void forceStop();  // kill

    bool isRunning() const;
    bool isInitialized() const;
    State state() const;

    // Initialize handshake
    void initialize(const LSP::InitializeParams& params);
    void initialized();

    // Text document sync
    void didOpen(const LSP::TextDocumentItem& item);
    void didChange(const LSP::VersionedTextDocumentIdentifier& doc,
                   const QVector<LSP::TextDocumentContentChangeEvent>& changes);
    void didClose(const LSP::TextDocumentIdentifier& doc);
    void didSave(const LSP::TextDocumentIdentifier& doc, const QString& text = {});

    // Request callbacks
    using CompletionCallback = std::function<void(const QVector<LSP::CompletionItem>&)>;
    using HoverCallback = std::function<void(const std::optional<LSP::Hover>&)>;
    using DefinitionCallback = std::function<void(const QVector<LSP::Location>&)>;
    using ReferencesCallback = std::function<void(const QVector<LSP::Location>&)>;
    using RenameCallback = std::function<void(const std::optional<LSP::WorkspaceEdit>&)>;
    using PrepareRenameCallback = std::function<void(const std::optional<LSP::Range>&)>;
    using DocumentSymbolCallback = std::function<void(const QVector<LSP::DocumentSymbol>&)>;
    using CodeActionCallback = std::function<void(const QVector<LSP::CodeAction>&)>;
    using GenericCallback = std::function<void(const QJsonObject&)>;

    // Requests (async with callbacks)
    void requestCompletion(const LSP::TextDocumentPositionParams& params, CompletionCallback cb);
    void requestHover(const LSP::TextDocumentPositionParams& params, HoverCallback cb);
    void requestDefinition(const LSP::TextDocumentPositionParams& params, DefinitionCallback cb);
    void requestReferences(const LSP::ReferenceParams& params, ReferencesCallback cb);
    void requestRename(const LSP::RenameParams& params, RenameCallback cb);
    void requestPrepareRename(const LSP::TextDocumentPositionParams& params, PrepareRenameCallback cb);
    void requestDocumentSymbol(const LSP::DocumentSymbolParams& params, DocumentSymbolCallback cb);
    void requestCodeAction(const LSP::CodeActionParams& params, CodeActionCallback cb);

    // Generic request for unmapped methods
    void sendRequest(const QString& method, const QJsonObject& params, GenericCallback cb);

    // Server capabilities (raw JSON for flexible access)
    const QJsonObject& serverCapabilities() const;
    QJsonObject serverCapabilities() { return m_serverCapabilities; }

signals:
    void stateChanged(State state);
    void errorOccurred(const QString& error);
    void diagnosticsReceived(const QString& uri, const QVector<LSP::Diagnostic>& diagnostics);
    void logMessage(const QString& type, const QString& message);
    void showMessage(const QString& type, const QString& message);
    void telemetryEvent(const QJsonObject& event);
    void registerCapability(const LSP::Registration& reg);
    void unregisterCapability(const LSP::Unregistration& unreg);

private:
    QString m_languageId;

    // Internal state
    State m_state = State::Stopped;
    bool m_initialized = false;
    QJsonObject m_serverCapabilities;

    // Process management
    QProcess* m_process = nullptr;
    QString m_command;
    QStringList m_args;
    QString m_workingDir;

    // JSON-RPC
    int m_nextRequestId = 1;
    QMap<int, LSP::PendingRequest> m_pendingRequests;
    LSP::MessageReader m_messageReader;

    // Debounced didChange
    struct PendingChange {
        LSP::VersionedTextDocumentIdentifier doc;
        QVector<LSP::TextDocumentContentChangeEvent> changes;
    };
    QMap<QString, PendingChange> m_pendingChanges;
    QTimer* m_debounceTimer = nullptr;
    static constexpr int DEBOUNCE_MS = 300;

    // Initialize handshake
    std::function<void(const QJsonObject&)> m_initializeCallback;
    std::function<void(const QString&)> m_initializeErrorCallback;

    // Private methods
    void setState(State state);
    void sendRaw(const QByteArray& data);
    void sendRequestInternal(const QString& method, const QJsonObject& params,
                             std::function<void(const QJsonObject&)> callback,
                             std::function<void(const QString&)> errorCallback);
    void sendNotification(const QString& method, const QJsonObject& params);

    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void handleResponse(const LSP::ResponseMessage& response);
    void handleNotification(const LSP::NotificationMessage& notification);

    void flushPendingChanges();
    void handleInitializeResponse(const QJsonObject& result);
    void handleInitializeError(const QString& error);

    // Notification handlers
    void handlePublishDiagnostics(const QJsonObject& params);
    void handleLogMessage(const QJsonObject& params);
    void handleShowMessage(const QJsonObject& params);
    void handleTelemetryEvent(const QJsonObject& params);
    void handleRegisterCapability(const QJsonObject& params);
    void handleUnregisterCapability(const QJsonObject& params);
};

#endif // LSPCLIENT_H