#include "LSPMessage.h"
#include <QJsonDocument>

namespace LSP {

// ============================================================================
// MessageReader Implementation
// ============================================================================

void MessageReader::appendData(const QByteArray& data) {
    m_buffer.append(data);
}

std::optional<QJsonObject> MessageReader::readMessage() {
    // Need at least headers
    if (m_buffer.size() < 4) return std::nullopt;

    // Parse headers if not already done
    if (!m_contentLength.has_value()) {
        if (!parseHeaders()) return std::nullopt;
    }

    // Check if we have the full content
    int totalSize = 4 + m_contentLength.value(); // \r\n\r\n = 4 bytes
    if (m_buffer.size() < totalSize) return std::nullopt;

    // Extract content
    QByteArray content = m_buffer.mid(4, m_contentLength.value());
    m_buffer = m_buffer.mid(totalSize);
    m_contentLength = std::nullopt;

    // Parse JSON
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }

    return doc.object();
}

bool MessageReader::parseHeaders() {
    int headerEnd = m_buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) return false;

    QByteArray headers = m_buffer.left(headerEnd);
    QList<QByteArray> lines = headers.split('\n');

    for (const QByteArray& line : lines) {
        QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith("Content-Length:") || trimmed.startsWith("content-length:")) {
            if (!extractContentLength(trimmed)) return false;
        }
    }

    // Move buffer past headers
    m_buffer = m_buffer.mid(headerEnd + 4);
    return m_contentLength.has_value();
}

bool MessageReader::extractContentLength(const QByteArray& line) {
    int colonPos = line.indexOf(':');
    if (colonPos == -1) return false;

    QByteArray value = line.mid(colonPos + 1).trimmed();
    bool ok = false;
    int length = value.toInt(&ok);
    if (!ok || length < 0) return false;

    m_contentLength = length;
    return true;
}

// ============================================================================
// MessageWriter Implementation
// ============================================================================

QByteArray MessageWriter::writeMessage(const QJsonObject& message) {
    QJsonDocument doc(message);
    QByteArray content = doc.toJson(QJsonDocument::Compact);
    QByteArray header = "Content-Length: " + QByteArray::number(content.size()) + "\r\n\r\n";
    return header + content;
}

QByteArray MessageWriter::writeRequest(int id, const QString& method, const QJsonObject& params) {
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["method"] = method;
    if (!params.isEmpty()) msg["params"] = params;
    return writeMessage(msg);
}

QByteArray MessageWriter::writeNotification(const QString& method, const QJsonObject& params) {
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["method"] = method;
    if (!params.isEmpty()) msg["params"] = params;
    return writeMessage(msg);
}

QByteArray MessageWriter::writeResponse(int id, const QJsonObject& result) {
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["result"] = result;
    return writeMessage(msg);
}

QByteArray MessageWriter::writeErrorResponse(int id, ErrorCode code, const QString& message, const QJsonValue& data) {
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    QJsonObject error;
    error["code"] = static_cast<int>(code);
    error["message"] = message;
    if (!data.isUndefined() && !data.isNull()) error["data"] = data;
    msg["error"] = error;
    return writeMessage(msg);
}

} // namespace LSP