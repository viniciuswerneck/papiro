#include "LSPClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>

LSPClient::LSPClient(const QString& languageId, QObject* parent) : QObject(parent), m_languageId(languageId) {
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DEBOUNCE_MS);
    connect(m_debounceTimer, &QTimer::timeout, this, &LSPClient::flushPendingChanges);
}

LSPClient::~LSPClient() {
    forceStop();
}

bool LSPClient::start(const QString& command, const QStringList& args, const QString& workingDir) {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "LSPClient: Process already running";
        return false;
    }

    m_command = command;
    m_args = args;
    m_workingDir = workingDir;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    if (!workingDir.isEmpty()) {
        m_process->setWorkingDirectory(workingDir);
    }

    connect(m_process, &QProcess::readyRead, this, &LSPClient::onReadyRead);
    connect(m_process, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
            this, &LSPClient::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LSPClient::onProcessFinished);

    setState(State::Starting);
    m_process->start(command, args);

    if (!m_process->waitForStarted(5000)) {
        QString error = QString("Failed to start LSP server: %1").arg(m_process->errorString());
        setState(State::Error);
        emit errorOccurred(error);
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }

    return true;
}

void LSPClient::stop() {
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        setState(State::Stopped);
        return;
    }

    if (m_initialized) {
        // Send shutdown request
        sendRequestInternal("shutdown", QJsonObject(),
            [this](const QJsonObject&) {
                // Shutdown acknowledged, send exit notification
                sendNotification("exit", QJsonObject());

                // Wait for process to finish gracefully
                if (m_process && m_process->state() != QProcess::NotRunning) {
                    if (!m_process->waitForFinished(3000)) {
                        m_process->kill();
                        m_process->waitForFinished(1000);
                    }
                }
                setState(State::Stopped);
            },
            [this](const QString& error) {
                qWarning() << "LSPClient: Shutdown request failed:" << error;
                sendNotification("exit", QJsonObject());
                if (m_process && m_process->state() != QProcess::NotRunning) {
                    m_process->waitForFinished(1000);
                }
                setState(State::Stopped);
            });
    } else {
        // Not initialized, just terminate
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        setState(State::Stopped);
    }
}

void LSPClient::forceStop() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
    m_pendingRequests.clear();
    m_pendingChanges.clear();
    m_initialized = false;
    setState(State::Stopped);
}

bool LSPClient::isRunning() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

bool LSPClient::isInitialized() const {
    return m_initialized;
}

LSPClient::State LSPClient::state() const {
    return m_state;
}

void LSPClient::initialize(const LSP::InitializeParams& params) {
    if (m_state != State::Starting) {
        qWarning() << "LSPClient: initialize() called in invalid state:" << static_cast<int>(m_state);
        return;
    }

    setState(State::Initializing);

    // Build client capabilities
    QJsonObject textDocSync;
    textDocSync["dynamicRegistration"] = true;
    textDocSync["willSave"] = true;
    textDocSync["willSaveWaitUntil"] = true;
    textDocSync["didSave"] = true;

    QJsonObject completion;
    completion["dynamicRegistration"] = true;
    QJsonObject completionItem;
    completionItem["snippetSupport"] = true;
    QJsonObject resolveSupport;
    QJsonArray props = {"documentation", "detail"};
    resolveSupport["properties"] = props;
    completionItem["resolveSupport"] = resolveSupport;
    completion["completionItem"] = completionItem;

    QJsonObject hover;
    hover["dynamicRegistration"] = true;
    hover["contentFormat"] = QJsonArray{"markdown", "plaintext"};

    QJsonObject definition;
    definition["dynamicRegistration"] = true;
    definition["linkSupport"] = true;

    QJsonObject references;
    references["dynamicRegistration"] = true;

    QJsonObject docSymbols;
    docSymbols["dynamicRegistration"] = true;
    docSymbols["hierarchicalDocumentSymbolSupport"] = true;
    QJsonArray symbolKinds;
    for (int i = 1; i <= 26; ++i) symbolKinds.append(i);
    QJsonObject symbolKind;
    symbolKind["valueSet"] = symbolKinds;
    docSymbols["symbolKind"] = symbolKind;

    QJsonObject codeAction;
    codeAction["dynamicRegistration"] = true;
    QJsonObject codeActionLiteralSupport;
    QJsonObject codeActionKind;
    QJsonArray kindValues = {"", "quickfix", "refactor", "refactor.extract", "refactor.inline",
                             "refactor.rewrite", "source", "source.organizeImports"};
    codeActionKind["valueSet"] = kindValues;
    codeActionLiteralSupport["codeActionKind"] = codeActionKind;
    codeAction["codeActionLiteralSupport"] = codeActionLiteralSupport;

    QJsonObject rename;
    rename["dynamicRegistration"] = true;
    rename["prepareSupport"] = true;

    QJsonObject textDocumentCaps;
    textDocumentCaps["synchronization"] = textDocSync;
    textDocumentCaps["completion"] = completion;
    textDocumentCaps["hover"] = hover;
    textDocumentCaps["definition"] = definition;
    textDocumentCaps["references"] = references;
    textDocumentCaps["documentSymbol"] = docSymbols;
    textDocumentCaps["codeAction"] = codeAction;
    textDocumentCaps["rename"] = rename;

    QJsonObject workspace;
    QJsonObject didChangeConfig;
    didChangeConfig["dynamicRegistration"] = true;
    workspace["didChangeConfiguration"] = didChangeConfig;

    QJsonObject didChangeWatchedFiles;
    didChangeWatchedFiles["dynamicRegistration"] = true;
    workspace["didChangeWatchedFiles"] = didChangeWatchedFiles;

    QJsonObject symbol;
    symbol["dynamicRegistration"] = true;
    workspace["symbol"] = symbol;

    QJsonObject executeCommand;
    executeCommand["dynamicRegistration"] = true;
    workspace["executeCommand"] = executeCommand;

    QJsonObject caps;
    caps["textDocument"] = textDocumentCaps;
    caps["workspace"] = workspace;
    caps["window"] = QJsonObject();
    caps["experimental"] = QJsonObject();

    LSP::InitializeParams initParams = params;
    initParams.capabilities = caps;

    sendRequestInternal("initialize", LSP::JsonUtils::toJsonObject(initParams),
        [this](const QJsonObject& result) {
            handleInitializeResponse(result);
        },
        [this](const QString& error) {
            handleInitializeError(error);
        });
}

void LSPClient::initialized() {
    if (m_state != State::Ready) {
        qWarning() << "LSPClient: initialized() called before ready";
        return;
    }
    sendNotification("initialized", QJsonObject());
}

void LSPClient::didOpen(const LSP::TextDocumentItem& item) {
    if (!m_initialized) return;
    sendNotification("textDocument/didOpen", LSP::JsonUtils::toJsonObject(LSP::DidOpenTextDocumentParams{item}));
}

void LSPClient::didChange(const LSP::VersionedTextDocumentIdentifier& doc,
                          const QVector<LSP::TextDocumentContentChangeEvent>& changes) {
    if (!m_initialized) return;

    QString key = doc.uri;
    auto& pending = m_pendingChanges[key];
    pending.doc = doc;
    pending.changes.append(changes);

    m_debounceTimer->start();
}

void LSPClient::didClose(const LSP::TextDocumentIdentifier& doc) {
    if (!m_initialized) return;
    // Flush any pending changes for this document first
    if (m_pendingChanges.contains(doc.uri)) {
        flushPendingChanges();
    }
    sendNotification("textDocument/didClose", LSP::JsonUtils::toJsonObject(LSP::DidCloseTextDocumentParams{doc}));
}

void LSPClient::didSave(const LSP::TextDocumentIdentifier& doc, const QString& text) {
    if (!m_initialized) return;
    LSP::DidSaveTextDocumentParams params{doc, text.isEmpty() ? std::nullopt : std::optional<QString>(text)};
    sendNotification("textDocument/didSave", LSP::JsonUtils::toJsonObject(params));
}

void LSPClient::requestCompletion(const LSP::TextDocumentPositionParams& params, CompletionCallback cb) {
    sendRequestInternal("textDocument/completion", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            if (result.isEmpty()) {
                cb(QVector<LSP::CompletionItem>{});
                return;
            }
            // Handle CompletionList (has "items" key) or array of items
            if (result.contains("items")) {
                cb(LSP::JsonUtils::completionListFromJson(result).items);
            } else {
                // Assume it's an array-like object or empty
                cb(QVector<LSP::CompletionItem>{});
            }
        },
        [cb](const QString& error) {
            qWarning() << "Completion request failed:" << error;
            cb(QVector<LSP::CompletionItem>{});
        });
}

void LSPClient::requestHover(const LSP::TextDocumentPositionParams& params, HoverCallback cb) {
    sendRequestInternal("textDocument/hover", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            cb(LSP::JsonUtils::hoverFromJson(result));
        },
        [cb](const QString& error) {
            qWarning() << "Hover request failed:" << error;
            cb(std::nullopt);
        });
}

void LSPClient::requestDefinition(const LSP::TextDocumentPositionParams& params, DefinitionCallback cb) {
    sendRequestInternal("textDocument/definition", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            cb(LSP::JsonUtils::locationsFromJson(result));
        },
        [cb](const QString& error) {
            qWarning() << "Definition request failed:" << error;
            cb(QVector<LSP::Location>{});
        });
}

void LSPClient::requestReferences(const LSP::ReferenceParams& params, ReferencesCallback cb) {
    sendRequestInternal("textDocument/references", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            cb(LSP::JsonUtils::locationsFromJson(result));
        },
        [cb](const QString& error) {
            qWarning() << "References request failed:" << error;
            cb(QVector<LSP::Location>{});
        });
}

void LSPClient::requestRename(const LSP::RenameParams& params, RenameCallback cb) {
    sendRequestInternal("textDocument/rename", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            cb(LSP::JsonUtils::workspaceEditFromJson(result));
        },
        [cb](const QString& error) {
            qWarning() << "Rename request failed:" << error;
            cb(std::nullopt);
        });
}

void LSPClient::requestPrepareRename(const LSP::TextDocumentPositionParams& params, PrepareRenameCallback cb) {
    sendRequestInternal("textDocument/prepareRename", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            cb(LSP::JsonUtils::rangeFromJson(result));
        },
        [cb](const QString& error) {
            qWarning() << "PrepareRename request failed:" << error;
            cb(std::nullopt);
        });
}

void LSPClient::requestDocumentSymbol(const LSP::DocumentSymbolParams& params, DocumentSymbolCallback cb) {
    sendRequestInternal("textDocument/documentSymbol", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            // Result is an array of DocumentSymbol or SymbolInformation
            // Since we receive QJsonObject, check if it's actually an array by checking if it's empty
            // but the server should return an array. For now, return empty if not an array.
            cb(QVector<LSP::DocumentSymbol>{});
        },
        [cb](const QString& error) {
            qWarning() << "DocumentSymbol request failed:" << error;
            cb(QVector<LSP::DocumentSymbol>{});
        });
}

void LSPClient::requestCodeAction(const LSP::CodeActionParams& params, CodeActionCallback cb) {
    sendRequestInternal("textDocument/codeAction", LSP::JsonUtils::toJsonObject(params),
        [cb](const QJsonObject& result) {
            cb(QVector<LSP::CodeAction>{});
        },
        [cb](const QString& error) {
            qWarning() << "CodeAction request failed:" << error;
            cb(QVector<LSP::CodeAction>{});
        });
}

void LSPClient::sendRequest(const QString& method, const QJsonObject& params, GenericCallback cb) {
    sendRequestInternal(method, params, cb, [cb](const QString& error) {
        qWarning() << "Generic request failed:" << error;
        cb(QJsonObject{{"error", error}});
    });
}

const QJsonObject& LSPClient::serverCapabilities() const {
    return m_serverCapabilities;
}

void LSPClient::setState(State state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void LSPClient::sendRaw(const QByteArray& data) {
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write(data);
    }
}

void LSPClient::sendRequestInternal(const QString& method, const QJsonObject& params,
                                    std::function<void(const QJsonObject&)> callback,
                                    std::function<void(const QString&)> errorCallback) {
    if (!m_process || m_process->state() != QProcess::Running) {
        errorCallback("Process not running");
        return;
    }

    int id = m_nextRequestId++;
    LSP::PendingRequest pending;
    pending.method = method;
    pending.callback = std::move(callback);
    pending.errorCallback = std::move(errorCallback);
    m_pendingRequests[id] = std::move(pending);

    QByteArray data = LSP::MessageWriter::writeRequest(id, method, params);
    sendRaw(data);
}

void LSPClient::sendNotification(const QString& method, const QJsonObject& params) {
    if (!m_process || m_process->state() != QProcess::Running) {
        return;
    }

    QByteArray data = LSP::MessageWriter::writeNotification(method, params);
    sendRaw(data);
}

void LSPClient::onReadyRead() {
    if (!m_process) return;

    QByteArray data = m_process->readAll();
    m_messageReader.appendData(data);

    while (true) {
        auto msg = m_messageReader.readMessage();
        if (!msg) break;

        if (msg->contains("id")) {
            // Response
            LSP::ResponseMessage response = LSP::ResponseMessage::fromJson(*msg);
            handleResponse(response);
        } else {
            // Notification
            LSP::NotificationMessage notification = LSP::NotificationMessage::fromJson(*msg);
            handleNotification(notification);
        }
    }
}

void LSPClient::onProcessError(QProcess::ProcessError error) {
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start LSP server process";
            break;
        case QProcess::Crashed:
            errorMsg = "LSP server process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "LSP server process timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Write error to LSP server process";
            break;
        case QProcess::ReadError:
            errorMsg = "Read error from LSP server process";
            break;
        case QProcess::UnknownError:
        default:
            errorMsg = QString("Unknown LSP server process error: %1").arg(m_process->errorString());
            break;
    }

    qWarning() << "LSPClient process error:" << errorMsg;
    emit errorOccurred(errorMsg);

    if (m_state == State::Starting || m_state == State::Initializing) {
        setState(State::Error);
    }
}

void LSPClient::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString msg = QString("LSP server process finished (exitCode=%1, status=%2)")
                      .arg(exitCode).arg(exitStatus == QProcess::CrashExit ? "CrashExit" : "NormalExit");
    qWarning() << msg;

    m_initialized = false;
    m_pendingRequests.clear();
    m_pendingChanges.clear();

    if (m_state != State::Stopped) {
        setState(State::Stopped);
        emit errorOccurred(msg);
    }
}

void LSPClient::handleResponse(const LSP::ResponseMessage& response) {
    int id = response.id.toInt();
    auto it = m_pendingRequests.find(id);
    if (it == m_pendingRequests.end()) {
        qWarning() << "LSPClient: Received response for unknown request id:" << id;
        return;
    }

    LSP::PendingRequest pending = std::move(it.value());
    m_pendingRequests.erase(it);

    if (response.isError() && response.error) {
        auto errObj = *response.error;
        int code = errObj["code"].toInt();
        QString message = errObj["message"].toString();
        pending.errorCallback(QString("LSP Error %1: %2").arg(code).arg(message));
    } else if (response.result) {
        pending.callback(*response.result);
    } else {
        pending.errorCallback("Empty response");
    }
}

void LSPClient::handleNotification(const LSP::NotificationMessage& notification) {
    const QString& method = notification.method;
    const QJsonObject& params = notification.params;

    if (method == "textDocument/publishDiagnostics") {
        handlePublishDiagnostics(params);
    } else if (method == "window/logMessage") {
        handleLogMessage(params);
    } else if (method == "window/showMessage") {
        handleShowMessage(params);
    } else if (method == "telemetry/event") {
        handleTelemetryEvent(params);
    } else if (method == "client/registerCapability") {
        handleRegisterCapability(params);
    } else if (method == "client/unregisterCapability") {
        handleUnregisterCapability(params);
    } else {
        qDebug() << "LSPClient: Unhandled notification:" << method;
    }
}

void LSPClient::flushPendingChanges() {
    for (auto it = m_pendingChanges.begin(); it != m_pendingChanges.end(); ++it) {
        const auto& pending = it.value();
        if (!pending.changes.isEmpty()) {
            LSP::DidChangeTextDocumentParams params{pending.doc, pending.changes};
            sendNotification("textDocument/didChange", LSP::JsonUtils::toJsonObject(params));
        }
    }
    m_pendingChanges.clear();
}

void LSPClient::handleInitializeResponse(const QJsonObject& result) {
    if (result.contains("capabilities")) {
        m_serverCapabilities = result["capabilities"].toObject();
    }
    m_initialized = true;
    setState(State::Ready);
    initialized(); // Send initialized notification
}

void LSPClient::handleInitializeError(const QString& error) {
    qWarning() << "LSPClient: Initialize failed:" << error;
    m_initialized = false;
    setState(State::Error);
    emit errorOccurred(QString("Initialize failed: %1").arg(error));
}

void LSPClient::handlePublishDiagnostics(const QJsonObject& params) {
    QString uri = params["uri"].toString();
    QJsonArray diagnosticsArray = params["diagnostics"].toArray();
    QVector<LSP::Diagnostic> diagnostics = LSP::JsonUtils::diagnosticsFromJson(diagnosticsArray);
    emit diagnosticsReceived(uri, diagnostics);
}

void LSPClient::handleLogMessage(const QJsonObject& params) {
    int type = params["type"].toInt();
    QString message = params["message"].toString();
    QString typeStr;
    switch (type) {
        case 1: typeStr = "Error"; break;
        case 2: typeStr = "Warning"; break;
        case 3: typeStr = "Info"; break;
        case 4: typeStr = "Log"; break;
        default: typeStr = "Unknown"; break;
    }
    emit logMessage(typeStr, message);
}

void LSPClient::handleShowMessage(const QJsonObject& params) {
    int type = params["type"].toInt();
    QString message = params["message"].toString();
    QString typeStr;
    switch (type) {
        case 1: typeStr = "Error"; break;
        case 2: typeStr = "Warning"; break;
        case 3: typeStr = "Info"; break;
        case 4: typeStr = "Log"; break;
        default: typeStr = "Unknown"; break;
    }
    emit showMessage(typeStr, message);
}

void LSPClient::handleTelemetryEvent(const QJsonObject& params) {
    emit telemetryEvent(params);
}

void LSPClient::handleRegisterCapability(const QJsonObject& params) {
    QJsonArray registrations = params["registrations"].toArray();
    for (const auto& reg : registrations) {
        emit registerCapability(LSP::JsonUtils::registrationFromJson(reg.toObject()));
    }
    // Register capability is a request from server, we need to respond
    // But we don't have the request id here - this is a notification in some servers
    // or a request in others. For now just acknowledge if it has an id.
}

void LSPClient::handleUnregisterCapability(const QJsonObject& params) {
    QJsonArray unregistrations = params["unregisterations"].toArray();
    for (const auto& unreg : unregistrations) {
        emit unregisterCapability(LSP::JsonUtils::unregistrationFromJson(unreg.toObject()));
    }
}