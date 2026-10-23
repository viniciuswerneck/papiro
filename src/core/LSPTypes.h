#ifndef LSPTYPES_H
#define LSPTYPES_H

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <optional>
#include <variant>

namespace LSP {

// ============================================================================
// Basic Types
// ============================================================================

struct Position {
    int line = 0;
    int character = 0;

    bool operator==(const Position& other) const {
        return line == other.line && character == other.character;
    }
    bool operator<(const Position& other) const {
        return line < other.line || (line == other.line && character < other.character);
    }
};

struct Range {
    Position start;
    Position end;

    bool operator==(const Range& other) const {
        return start == other.start && end == other.end;
    }
};

struct Location {
    QString uri;
    Range range;

    bool operator==(const Location& other) const {
        return uri == other.uri && range == other.range;
    }
};

struct LocationLink {
    Range originSelectionRange;
    QString targetUri;
    Range targetRange;
    Range targetSelectionRange;
};

// ============================================================================
// Document Identifiers
// ============================================================================

struct TextDocumentIdentifier {
    QString uri;
};

struct VersionedTextDocumentIdentifier : TextDocumentIdentifier {
    int version = 0;
};

struct TextDocumentItem {
    QString uri;
    QString languageId;
    int version = 0;
    QString text;
};

// ============================================================================
// Content Changes
// ============================================================================

struct TextDocumentContentChangeEvent {
    std::optional<Range> range;
    std::optional<Range> rangeLength;
    QString text;
};

// ============================================================================
// Diagnostics
// ============================================================================

enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

struct Diagnostic {
    Range range;
    std::optional<DiagnosticSeverity> severity;
    std::optional<int> code;
    std::optional<QString> codeDescription;
    std::optional<QString> source;
    QString message;
    std::optional<QVector<Diagnostic>> relatedInformation;
};

struct DiagnosticRelatedInformation {
    Location location;
    QString message;
};

// ============================================================================
// Completion
// ============================================================================

enum class CompletionItemKind {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25
};

enum class InsertTextFormat {
    PlainText = 1,
    Snippet = 2
};

struct CompletionItem {
    QString label;
    std::optional<CompletionItemKind> kind;
    std::optional<QString> detail;
    std::optional<QString> documentation;
    std::optional<bool> deprecated;
    std::optional<QString> preselect;
    std::optional<QString> sortText;
    std::optional<QString> filterText;
    std::optional<QString> insertText;
    std::optional<InsertTextFormat> insertTextFormat;
    std::optional<QJsonObject> textEdit;
    std::optional<QJsonObject> additionalTextEdits;
    std::optional<QString> command;
    std::optional<QJsonObject> data;
};

struct CompletionList {
    bool isIncomplete = false;
    QVector<CompletionItem> items;
};

// ============================================================================
// Hover
// ============================================================================

struct MarkupContent {
    QString kind; // "plaintext" | "markdown"
    QString value;
};

struct Hover {
    std::variant<QString, MarkupContent, QVector<MarkupContent>> contents;
    std::optional<Range> range;
};

// ============================================================================
// Symbols
// ============================================================================

enum class SymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26
};

struct DocumentSymbol {
    QString name;
    std::optional<QString> detail;
    SymbolKind kind;
    Range range;
    Range selectionRange;
    std::optional<QVector<DocumentSymbol>> children;
    std::optional<int> deprecated;
};

// ============================================================================
// Code Actions
// ============================================================================

enum class CodeActionKind {
    QuickFix,
    Refactor,
    RefactorExtract,
    RefactorInline,
    RefactorRewrite,
    Source,
    SourceOrganizeImports
};

struct CodeAction {
    QString title;
    std::optional<CodeActionKind> kind;
    std::optional<QJsonObject> diagnostics;
    std::optional<bool> isPreferred;
    std::optional<bool> disabled;
    std::optional<QJsonObject> edit;
    std::optional<QJsonObject> command;
    std::optional<QJsonObject> data;
};

// ============================================================================
// Workspace Edit
// ============================================================================

struct TextEdit {
    Range range;
    QString newText;
};

struct WorkspaceEdit {
    QMap<QString, QVector<TextEdit>> changes;
    std::optional<QVector<QJsonObject>> documentChanges;
};

// ============================================================================
// Workspace
// ============================================================================

struct WorkspaceFolder {
    QString uri;
    QString name;
};

// ============================================================================
// Initialize
// ============================================================================

struct InitializeParams {
    int processId = 0;
    QString rootUri;
    std::optional<QString> rootPath;
    QJsonObject initializationOptions;
    QJsonObject capabilities;
    std::optional<QString> trace;
    std::optional<QString> workspaceFolders;
    std::optional<QJsonObject> clientInfo;
};

struct ClientCapabilities {
    QJsonObject textDocument;
    QJsonObject workspace;
    QJsonObject window;
    QJsonObject experimental;
};

struct ServerCapabilities {
    std::optional<QJsonObject> textDocumentSync;
    std::optional<bool> hoverProvider;
    std::optional<QJsonObject> completionProvider;
    std::optional<QJsonObject> signatureHelpProvider;
    std::optional<QJsonObject> definitionProvider;
    std::optional<QJsonObject> referencesProvider;
    std::optional<QJsonObject> documentHighlightProvider;
    std::optional<QJsonObject> documentSymbolProvider;
    std::optional<QJsonObject> workspaceSymbolProvider;
    std::optional<QJsonObject> codeActionProvider;
    std::optional<QJsonObject> codeLensProvider;
    std::optional<QJsonObject> documentFormattingProvider;
    std::optional<QJsonObject> documentRangeFormattingProvider;
    std::optional<QJsonObject> documentOnTypeFormattingProvider;
    std::optional<QJsonObject> renameProvider;
    std::optional<QJsonObject> documentLinkProvider;
    std::optional<QJsonObject> colorProvider;
    std::optional<QJsonObject> foldingRangeProvider;
    std::optional<QJsonObject> executeCommandProvider;
    std::optional<QJsonObject> selectionRangeProvider;
    std::optional<QJsonObject> linkedEditingRangeProvider;
    std::optional<QJsonObject> callHierarchyProvider;
    std::optional<QJsonObject> semanticTokensProvider;
    std::optional<QJsonObject> monikerProvider;
    std::optional<QJsonObject> typeHierarchyProvider;
    std::optional<QJsonObject> inlineValueProvider;
    std::optional<QJsonObject> inlayHintProvider;
    std::optional<QJsonObject> diagnosticProvider;
    std::optional<QJsonObject> workspace;
    std::optional<QJsonObject> experimental;
};

struct InitializeResult {
    ServerCapabilities capabilities;
    std::optional<QJsonObject> serverInfo;
};

// ============================================================================
// Text Document Sync
// ============================================================================

struct DidOpenTextDocumentParams {
    TextDocumentItem textDocument;
};

struct DidChangeTextDocumentParams {
    VersionedTextDocumentIdentifier textDocument;
    QVector<TextDocumentContentChangeEvent> contentChanges;
};

struct DidCloseTextDocumentParams {
    TextDocumentIdentifier textDocument;
};

struct DidSaveTextDocumentParams {
    TextDocumentIdentifier textDocument;
    std::optional<QString> text;
};

// ============================================================================
// Completion Request
// ============================================================================

struct CompletionContext {
    enum class TriggerKind { Invoked = 1, TriggerCharacter = 2, TriggerForIncompleteCompletions = 3 };
    TriggerKind triggerKind = TriggerKind::Invoked;
    std::optional<QString> triggerCharacter;
};

struct TextDocumentPositionParams {
    TextDocumentIdentifier textDocument;
    Position position;
};

struct CompletionParams : TextDocumentPositionParams {
    std::optional<CompletionContext> context;
};

// ============================================================================
// Hover Request
// ============================================================================

using HoverParams = TextDocumentPositionParams;

// ============================================================================
// Definition Request
// ============================================================================

using DefinitionParams = TextDocumentPositionParams;

// ============================================================================
// References Request
// ============================================================================

struct ReferenceContext {
    bool includeDeclaration = true;
};

struct ReferenceParams : TextDocumentPositionParams {
    ReferenceContext context;
};

// ============================================================================
// Rename Request
// ============================================================================

struct RenameParams {
    TextDocumentIdentifier textDocument;
    Position position;
    QString newName;
};

struct PrepareRenameParams : TextDocumentPositionParams {};

// ============================================================================
// Document Symbol Request
// ============================================================================

struct DocumentSymbolParams {
    TextDocumentIdentifier textDocument;
};

// ============================================================================
// Code Action Request
// ============================================================================

struct CodeActionContext {
    QVector<Diagnostic> diagnostics;
    std::optional<QVector<CodeActionKind>> only;
    std::optional<bool> triggerKind;
};

struct CodeActionParams {
    TextDocumentIdentifier textDocument;
    Range range;
    CodeActionContext context;
};

// ============================================================================
// Registration
// ============================================================================

struct Registration {
    QString id;
    QString method;
    std::optional<QJsonObject> registerOptions;
};

struct Unregistration {
    QString id;
    QString method;
};

// ============================================================================
// Message Parameters
// ============================================================================

struct LogMessageParams {
    enum class MessageType { Error = 1, Warning = 2, Info = 3, Log = 4 };
    MessageType type = MessageType::Log;
    QString message;
};

struct ShowMessageParams {
    enum class MessageType { Error = 1, Warning = 2, Info = 3, Log = 4 };
    MessageType type = MessageType::Log;
    QString message;
};

struct TelemetryEventParams {
    QJsonObject event;
};

// ============================================================================
// JSON Serialization Helpers (in Detail namespace)
// ============================================================================

namespace Detail {
    inline QJsonObject positionToJson(const Position& pos) {
        return QJsonObject{{"line", pos.line}, {"character", pos.character}};
    }

    inline Position positionFromJson(const QJsonObject& obj) {
        Position pos;
        pos.line = obj["line"].toInt();
        pos.character = obj["character"].toInt();
        return pos;
    }

    inline QJsonObject rangeToJson(const Range& range) {
        return QJsonObject{
            {"start", positionToJson(range.start)},
            {"end", positionToJson(range.end)}
        };
    }

    inline Range rangeFromJson(const QJsonObject& obj) {
        Range range;
        range.start = positionFromJson(obj["start"].toObject());
        range.end = positionFromJson(obj["end"].toObject());
        return range;
    }

    inline QJsonObject locationToJson(const Location& loc) {
        return QJsonObject{
            {"uri", loc.uri},
            {"range", rangeToJson(loc.range)}
        };
    }

    inline Location locationFromJson(const QJsonObject& obj) {
        Location loc;
        loc.uri = obj["uri"].toString();
        loc.range = rangeFromJson(obj["range"].toObject());
        return loc;
    }

    inline QJsonObject diagnosticToJson(const Diagnostic& diag) {
        QJsonObject obj;
        obj["range"] = rangeToJson(diag.range);
        if (diag.severity) obj["severity"] = static_cast<int>(*diag.severity);
        if (diag.code) obj["code"] = *diag.code;
        if (diag.source) obj["source"] = *diag.source;
        obj["message"] = diag.message;
        return obj;
    }

    inline Diagnostic diagnosticFromJson(const QJsonObject& obj) {
        Diagnostic diag;
        diag.range = rangeFromJson(obj["range"].toObject());
        if (obj.contains("severity")) diag.severity = static_cast<DiagnosticSeverity>(obj["severity"].toInt());
        if (obj.contains("code")) diag.code = obj["code"].toInt();
        if (obj.contains("source")) diag.source = obj["source"].toString();
        diag.message = obj["message"].toString();
        return diag;
    }

    inline QJsonObject completionItemToJson(const CompletionItem& item) {
        QJsonObject obj;
        obj["label"] = item.label;
        if (item.kind) obj["kind"] = static_cast<int>(*item.kind);
        if (item.detail) obj["detail"] = *item.detail;
        if (item.documentation) obj["documentation"] = *item.documentation;
        if (item.deprecated) obj["deprecated"] = *item.deprecated;
        if (item.insertText) obj["insertText"] = *item.insertText;
        if (item.insertTextFormat) obj["insertTextFormat"] = static_cast<int>(*item.insertTextFormat);
        return obj;
    }

    inline CompletionItem completionItemFromJson(const QJsonObject& obj) {
        CompletionItem item;
        item.label = obj["label"].toString();
        if (obj.contains("kind")) item.kind = static_cast<CompletionItemKind>(obj["kind"].toInt());
        if (obj.contains("detail")) item.detail = obj["detail"].toString();
        if (obj.contains("documentation")) item.documentation = obj["documentation"].toString();
        if (obj.contains("deprecated")) item.deprecated = obj["deprecated"].toBool();
        if (obj.contains("insertText")) item.insertText = obj["insertText"].toString();
        if (obj.contains("insertTextFormat")) item.insertTextFormat = static_cast<InsertTextFormat>(obj["insertTextFormat"].toInt());
        return item;
    }

    inline QJsonObject textEditToJson(const TextEdit& edit) {
        return QJsonObject{
            {"range", rangeToJson(edit.range)},
            {"newText", edit.newText}
        };
    }

    inline TextEdit textEditFromJson(const QJsonObject& obj) {
        TextEdit edit;
        edit.range = rangeFromJson(obj["range"].toObject());
        edit.newText = obj["newText"].toString();
        return edit;
    }
}

} // namespace LSP

// Q_DECLARE_METATYPE for Qt's meta-object system
Q_DECLARE_METATYPE(LSP::Diagnostic)
Q_DECLARE_METATYPE(LSP::Range)
Q_DECLARE_METATYPE(LSP::Position)
Q_DECLARE_METATYPE(LSP::Location)
Q_DECLARE_METATYPE(LSP::Hover)
Q_DECLARE_METATYPE(LSP::CompletionList)
Q_DECLARE_METATYPE(LSP::CompletionItem)
Q_DECLARE_METATYPE(LSP::DocumentSymbol)
Q_DECLARE_METATYPE(LSP::CodeAction)
Q_DECLARE_METATYPE(LSP::WorkspaceEdit)
Q_DECLARE_METATYPE(LSP::Registration)
Q_DECLARE_METATYPE(LSP::Unregistration)

#endif // LSPTYPES_H