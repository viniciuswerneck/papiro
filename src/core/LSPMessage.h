#ifndef LSPMESSAGE_H
#define LSPMESSAGE_H

#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <optional>
#include <variant>
#include <functional>
#include "LSPTypes.h"

namespace LSP {

// ============================================================================
// JSON-RPC 2.0 Message Types
// ============================================================================

struct RequestMessage {
    QString jsonrpc = "2.0";
    QJsonValue id;
    QString method;
    QJsonObject params;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["jsonrpc"] = jsonrpc;
        obj["id"] = id;
        obj["method"] = method;
        if (!params.isEmpty()) obj["params"] = params;
        return obj;
    }
};

struct ResponseMessage {
    QString jsonrpc = "2.0";
    QJsonValue id;
    std::optional<QJsonObject> result;
    std::optional<QJsonObject> error;

    static ResponseMessage fromJson(const QJsonObject& obj) {
        ResponseMessage msg;
        msg.jsonrpc = obj["jsonrpc"].toString();
        msg.id = obj["id"];
        if (obj.contains("result")) msg.result = obj["result"].toObject();
        if (obj.contains("error")) msg.error = obj["error"].toObject();
        return msg;
    }

    bool isError() const { return error.has_value(); }
    bool isSuccess() const { return result.has_value() && !error.has_value(); }
};

struct NotificationMessage {
    QString jsonrpc = "2.0";
    QString method;
    QJsonObject params;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["jsonrpc"] = jsonrpc;
        obj["method"] = method;
        if (!params.isEmpty()) obj["params"] = params;
        return obj;
    }

    static NotificationMessage fromJson(const QJsonObject& obj) {
        NotificationMessage msg;
        msg.jsonrpc = obj["jsonrpc"].toString();
        msg.method = obj["method"].toString();
        if (obj.contains("params")) msg.params = obj["params"].toObject();
        return msg;
    }
};

using Message = std::variant<RequestMessage, ResponseMessage, NotificationMessage>;

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,
    RequestCancelled = -32800,
    ContentModified = -32801
};

struct ResponseError {
    int code = 0;
    QString message;
    QJsonValue data;

    static ResponseError fromJson(const QJsonObject& obj) {
        ResponseError err;
        err.code = obj["code"].toInt();
        err.message = obj["message"].toString();
        if (obj.contains("data")) err.data = obj["data"];
        return err;
    }
};

// ============================================================================
// Message Framing (Content-Length)
// ============================================================================

class MessageReader {
public:
    void appendData(const QByteArray& data);
    std::optional<QJsonObject> readMessage();

private:
    QByteArray m_buffer;
    std::optional<int> m_contentLength = std::nullopt;

    bool parseHeaders();
    bool extractContentLength(const QByteArray& line);
};

class MessageWriter {
public:
    static QByteArray writeMessage(const QJsonObject& message);
    static QByteArray writeRequest(int id, const QString& method, const QJsonObject& params);
    static QByteArray writeNotification(const QString& method, const QJsonObject& params);
    static QByteArray writeResponse(int id, const QJsonObject& result);
    static QByteArray writeErrorResponse(int id, ErrorCode code, const QString& message, const QJsonValue& data = QJsonValue());
};

// ============================================================================
// Request/Response Correlation
// ============================================================================

struct PendingRequest {
    QString method;
    std::function<void(const QJsonObject&)> callback;
    std::function<void(const QString&)> errorCallback;
};

// ============================================================================
// Server Capabilities Helper
// ============================================================================

class ServerCapabilitiesHelper {
public:
    explicit ServerCapabilitiesHelper(const LSP::ServerCapabilities& caps) : m_caps(caps) {}

    bool hasHoverProvider() const { return m_caps.hoverProvider.value_or(false); }
    bool hasCompletionProvider() const { return m_caps.completionProvider.has_value(); }
    bool hasDefinitionProvider() const { return m_caps.definitionProvider.has_value(); }
    bool hasReferencesProvider() const { return m_caps.referencesProvider.has_value(); }
    bool hasRenameProvider() const { return m_caps.renameProvider.has_value(); }
    bool hasPrepareRenameProvider() const {
        if (!m_caps.renameProvider) return false;
        auto obj = m_caps.renameProvider.value();
        return obj.contains("prepareProvider") && obj["prepareProvider"].toBool();
    }
    bool hasDocumentSymbolProvider() const { return m_caps.documentSymbolProvider.has_value(); }
    bool hasCodeActionProvider() const { return m_caps.codeActionProvider.has_value(); }

    QJsonObject completionTriggerCharacters() const {
        if (!m_caps.completionProvider) return QJsonObject();
        return m_caps.completionProvider.value();
    }

    int textDocumentSyncKind() const {
        if (!m_caps.textDocumentSync) return 0;
        auto sync = m_caps.textDocumentSync.value();
        if (sync.contains("change")) return sync["change"].toInt();
        return sync["openClose"].toBool() ? 1 : 0;
    }

private:
    const LSP::ServerCapabilities& m_caps;
};

// ============================================================================
// JSON Helpers
// ============================================================================

namespace JsonUtils {
    inline QJsonObject toJsonObject(const LSP::InitializeParams& params) {
        QJsonObject obj;
        obj["processId"] = params.processId;
        if (!params.rootUri.isEmpty()) obj["rootUri"] = params.rootUri;
        if (params.rootPath) obj["rootPath"] = *params.rootPath;
        if (!params.initializationOptions.isEmpty()) obj["initializationOptions"] = params.initializationOptions;
        if (!params.capabilities.isEmpty()) obj["capabilities"] = params.capabilities;
        if (params.trace) obj["trace"] = *params.trace;
        if (params.workspaceFolders) obj["workspaceFolders"] = *params.workspaceFolders;
        if (params.clientInfo) obj["clientInfo"] = *params.clientInfo;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::DidOpenTextDocumentParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        td["languageId"] = params.textDocument.languageId;
        td["version"] = params.textDocument.version;
        td["text"] = params.textDocument.text;
        obj["textDocument"] = td;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::DidChangeTextDocumentParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        td["version"] = params.textDocument.version;
        obj["textDocument"] = td;

        QJsonArray changes;
        for (const auto& change : params.contentChanges) {
            QJsonObject c;
            if (change.range) c["range"] = Detail::rangeToJson(*change.range);
            if (change.rangeLength) c["rangeLength"] = Detail::rangeToJson(*change.rangeLength);
            c["text"] = change.text;
            changes.append(c);
        }
        obj["contentChanges"] = changes;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::DidCloseTextDocumentParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        obj["textDocument"] = td;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::DidSaveTextDocumentParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        obj["textDocument"] = td;
        if (params.text) obj["text"] = *params.text;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::TextDocumentPositionParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        obj["textDocument"] = td;
        obj["position"] = Detail::positionToJson(params.position);
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::CompletionParams& params) {
        QJsonObject obj = toJsonObject(static_cast<const LSP::TextDocumentPositionParams&>(params));
        if (params.context) {
            QJsonObject ctx;
            ctx["triggerKind"] = static_cast<int>(params.context->triggerKind);
            if (params.context->triggerCharacter) ctx["triggerCharacter"] = *params.context->triggerCharacter;
            obj["context"] = ctx;
        }
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::ReferenceParams& params) {
        QJsonObject obj = toJsonObject(static_cast<const LSP::TextDocumentPositionParams&>(params));
        QJsonObject ctx;
        ctx["includeDeclaration"] = params.context.includeDeclaration;
        obj["context"] = ctx;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::RenameParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        obj["textDocument"] = td;
        obj["position"] = Detail::positionToJson(params.position);
        obj["newName"] = params.newName;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::DocumentSymbolParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        obj["textDocument"] = td;
        return obj;
    }

    inline QJsonObject toJsonObject(const LSP::CodeActionParams& params) {
        QJsonObject obj;
        QJsonObject td;
        td["uri"] = params.textDocument.uri;
        obj["textDocument"] = td;
        obj["range"] = Detail::rangeToJson(params.range);

        QJsonObject ctx;
        QJsonArray diags;
        for (const auto& d : params.context.diagnostics) {
            diags.append(Detail::diagnosticToJson(d));
        }
        ctx["diagnostics"] = diags;
        if (params.context.only) {
            QJsonArray only;
            for (const auto& k : *params.context.only) {
                only.append(static_cast<int>(k));
            }
            ctx["only"] = only;
        }
        obj["context"] = ctx;
        return obj;
    }

    inline LSP::InitializeResult initializeResultFromJson(const QJsonObject& obj) {
        LSP::InitializeResult result;
        if (obj.contains("capabilities")) {
            // Keep capabilities as QJsonObject for flexible parsing
        }
        if (obj.contains("serverInfo")) {
            result.serverInfo = obj["serverInfo"].toObject();
        }
        return result;
    }

    inline LSP::CompletionList completionListFromJson(const QJsonObject& obj) {
        LSP::CompletionList list;
        list.isIncomplete = obj["isIncomplete"].toBool();
        QJsonArray items = obj["items"].toArray();
        for (const auto& item : items) {
            list.items.append(Detail::completionItemFromJson(item.toObject()));
        }
        return list;
    }

    inline std::optional<LSP::Hover> hoverFromJson(const QJsonObject& obj) {
        if (obj.isEmpty()) return std::nullopt;
        LSP::Hover hover;
        if (obj.contains("contents")) {
            auto contents = obj["contents"];
            if (contents.isString()) {
                hover.contents = contents.toString();
            } else if (contents.isObject()) {
                LSP::MarkupContent mc;
                mc.kind = contents.toObject()["kind"].toString();
                mc.value = contents.toObject()["value"].toString();
                hover.contents = mc;
            } else if (contents.isArray()) {
                QVector<LSP::MarkupContent> vec;
                for (const auto& c : contents.toArray()) {
                    LSP::MarkupContent mc;
                    mc.kind = c.toObject()["kind"].toString();
                    mc.value = c.toObject()["value"].toString();
                    vec.append(mc);
                }
                hover.contents = vec;
            }
        }
        if (obj.contains("range")) {
            hover.range = Detail::rangeFromJson(obj["range"].toObject());
        }
        return hover;
    }

    inline QVector<LSP::Location> locationsFromJson(const QJsonValue& val) {
        QVector<LSP::Location> locations;
        if (val.isArray()) {
            for (const auto& v : val.toArray()) {
                locations.append(Detail::locationFromJson(v.toObject()));
            }
        } else if (val.isObject()) {
            locations.append(Detail::locationFromJson(val.toObject()));
        }
        return locations;
    }

    inline std::optional<LSP::WorkspaceEdit> workspaceEditFromJson(const QJsonObject& obj) {
        if (obj.isEmpty()) return std::nullopt;
        LSP::WorkspaceEdit edit;
        if (obj.contains("changes")) {
            QJsonObject changes = obj["changes"].toObject();
            for (auto it = changes.begin(); it != changes.end(); ++it) {
                QVector<LSP::TextEdit> edits;
                for (const auto& e : it.value().toArray()) {
                    edits.append(Detail::textEditFromJson(e.toObject()));
                }
                edit.changes[it.key()] = edits;
            }
        }
        return edit;
    }

    inline std::optional<LSP::Range> rangeFromJson(const QJsonObject& obj) {
        if (obj.isEmpty()) return std::nullopt;
        return Detail::rangeFromJson(obj);
    }

    inline QVector<LSP::DocumentSymbol> documentSymbolsFromJson(const QJsonArray& arr) {
        QVector<LSP::DocumentSymbol> symbols;
        for (const auto& v : arr) {
            LSP::DocumentSymbol sym;
            auto obj = v.toObject();
            sym.name = obj["name"].toString();
            if (obj.contains("detail")) sym.detail = obj["detail"].toString();
            if (obj.contains("kind")) sym.kind = static_cast<LSP::SymbolKind>(obj["kind"].toInt());
            if (obj.contains("range")) sym.range = Detail::rangeFromJson(obj["range"].toObject());
            if (obj.contains("selectionRange")) sym.selectionRange = Detail::rangeFromJson(obj["selectionRange"].toObject());
            if (obj.contains("children")) {
                sym.children = documentSymbolsFromJson(obj["children"].toArray());
            }
            symbols.append(sym);
        }
        return symbols;
    }

    inline QVector<LSP::CodeAction> codeActionsFromJson(const QJsonArray& arr) {
        QVector<LSP::CodeAction> actions;
        for (const auto& v : arr) {
            LSP::CodeAction action;
            auto obj = v.toObject();
            action.title = obj["title"].toString();
            if (obj.contains("kind")) action.kind = static_cast<LSP::CodeActionKind>(obj["kind"].toInt());
            if (obj.contains("diagnostics")) action.diagnostics = obj["diagnostics"].toObject();
            if (obj.contains("isPreferred")) action.isPreferred = obj["isPreferred"].toBool();
            if (obj.contains("disabled")) action.disabled = obj["disabled"].toBool();
            if (obj.contains("edit")) action.edit = obj["edit"].toObject();
            if (obj.contains("command")) action.command = obj["command"].toObject();
            if (obj.contains("data")) action.data = obj["data"].toObject();
            actions.append(action);
        }
        return actions;
    }

    inline QVector<LSP::Diagnostic> diagnosticsFromJson(const QJsonArray& arr) {
        QVector<LSP::Diagnostic> diagnostics;
        for (const auto& v : arr) {
            diagnostics.append(Detail::diagnosticFromJson(v.toObject()));
        }
        return diagnostics;
    }

    inline LSP::Registration registrationFromJson(const QJsonObject& obj) {
        LSP::Registration reg;
        reg.id = obj["id"].toString();
        reg.method = obj["method"].toString();
        if (obj.contains("registerOptions")) reg.registerOptions = obj["registerOptions"].toObject();
        return reg;
    }

    inline LSP::Unregistration unregistrationFromJson(const QJsonObject& obj) {
        LSP::Unregistration unreg;
        unreg.id = obj["id"].toString();
        unreg.method = obj["method"].toString();
        return unreg;
    }
}

} // namespace LSP

#endif // LSPMESSAGE_H