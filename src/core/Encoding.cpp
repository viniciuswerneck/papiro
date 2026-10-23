#include "core/Encoding.h"

#include <QStringConverter>

namespace papiro {
namespace {

// Windows-1252 code points for bytes 0x80..0x9F (the only range that differs
// from ISO-8859-1). Undefined positions map to the matching C1 control code.
constexpr ushort kWindows1252High[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

QString decodeWindows1252(const QByteArray &data)
{
    QString out;
    out.reserve(data.size());
    for (char c : data) {
        const auto byte = static_cast<uchar>(c);
        if (byte >= 0x80 && byte <= 0x9F)
            out.append(QChar(kWindows1252High[byte - 0x80]));
        else
            out.append(QChar(byte));
    }
    return out;
}

QByteArray encodeWindows1252(const QString &text)
{
    QByteArray out;
    out.reserve(text.size());
    for (QChar ch : text) {
        const ushort u = ch.unicode();
        if (u <= 0x7F || (u >= 0xA0 && u <= 0xFF)) {
            out.append(static_cast<char>(u));
            continue;
        }
        char mapped = '?';
        for (int i = 0; i < 32; ++i) {
            if (kWindows1252High[i] == u) {
                mapped = static_cast<char>(0x80 + i);
                break;
            }
        }
        out.append(mapped);
    }
    return out;
}

QString decodeUtf16(const QByteArray &data, bool littleEndian)
{
    const int units = data.size() / 2;
    QString out;
    out.resize(units);
    QChar *dst = out.data();
    const auto *src = reinterpret_cast<const uchar *>(data.constData());
    for (int i = 0; i < units; ++i) {
        const ushort lo = littleEndian ? src[i * 2] : src[i * 2 + 1];
        const ushort hi = littleEndian ? src[i * 2 + 1] : src[i * 2];
        dst[i] = QChar(static_cast<ushort>((hi << 8) | lo));
    }
    return out;
}

QByteArray encodeUtf16(const QString &text, bool littleEndian)
{
    QByteArray out;
    out.reserve(text.size() * 2 + 2);
    out.append(static_cast<char>(littleEndian ? 0xFF : 0xFE));
    out.append(static_cast<char>(littleEndian ? 0xFE : 0xFF));
    for (QChar ch : text) {
        const ushort u = ch.unicode();
        const char first = static_cast<char>(littleEndian ? (u & 0xFF) : (u >> 8));
        const char second = static_cast<char>(littleEndian ? (u >> 8) : (u & 0xFF));
        out.append(first);
        out.append(second);
    }
    return out;
}

bool looksLikeValidUtf8(const QByteArray &data)
{
    if (data.isEmpty())
        return true;
    QStringDecoder decoder(QStringConverter::Utf8);
    decoder.decode(QByteArrayView(data));
    return !decoder.hasError();
}

} // namespace

QString encodingName(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Utf8:
        return QStringLiteral("UTF-8");
    case TextEncoding::Utf8Bom:
        return QStringLiteral("UTF-8 BOM");
    case TextEncoding::Utf16LE:
        return QStringLiteral("UTF-16 LE");
    case TextEncoding::Utf16BE:
        return QStringLiteral("UTF-16 BE");
    case TextEncoding::Windows1252:
        return QStringLiteral("Windows-1252");
    }
    return QStringLiteral("Unknown");
}

QString lineEndingName(LineEnding eol)
{
    switch (eol) {
    case LineEnding::Lf:
        return QStringLiteral("LF");
    case LineEnding::CrLf:
        return QStringLiteral("CRLF");
    case LineEnding::Cr:
        return QStringLiteral("CR");
    }
    return QStringLiteral("LF");
}

TextEncoding detectEncoding(const QByteArray &data)
{
    const auto startsWith = [&data](std::initializer_list<int> bytes) {
        if (data.size() < static_cast<int>(bytes.size()))
            return false;
        int i = 0;
        for (int b : bytes) {
            if (static_cast<uchar>(data.at(i)) != b)
                return false;
            ++i;
        }
        return true;
    };

    if (startsWith({0xEF, 0xBB, 0xBF}))
        return TextEncoding::Utf8Bom;
    if (startsWith({0xFF, 0xFE}))
        return TextEncoding::Utf16LE;
    if (startsWith({0xFE, 0xFF}))
        return TextEncoding::Utf16BE;

    if (looksLikeValidUtf8(data))
        return TextEncoding::Utf8;
    return TextEncoding::Windows1252;
}

LineEnding detectLineEnding(const QString &text)
{
    int crlf = 0;
    int lf = 0;
    int cr = 0;
    const int n = text.size();
    for (int i = 0; i < n; ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\r')) {
            if (i + 1 < n && text.at(i + 1) == QLatin1Char('\n')) {
                ++crlf;
                ++i;
            } else {
                ++cr;
            }
        } else if (ch == QLatin1Char('\n')) {
            ++lf;
        }
    }
    if (crlf >= lf && crlf >= cr)
        return crlf == 0 ? LineEnding::Lf : LineEnding::CrLf;
    if (cr > lf)
        return LineEnding::Cr;
    return LineEnding::Lf;
}

QString normalizeLineEndings(const QString &text)
{
    QString out;
    out.reserve(text.size());
    const int n = text.size();
    for (int i = 0; i < n; ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\r')) {
            out.append(QLatin1Char('\n'));
            if (i + 1 < n && text.at(i + 1) == QLatin1Char('\n'))
                ++i;
        } else {
            out.append(ch);
        }
    }
    return out;
}

QString applyLineEnding(const QString &text, LineEnding eol)
{
    if (eol == LineEnding::Lf)
        return text;
    const QString replacement = eol == LineEnding::CrLf ? QStringLiteral("\r\n")
                                                        : QStringLiteral("\r");
    return QString(text).replace(QLatin1Char('\n'), replacement);
}

DecodedFile decodeFile(const QByteArray &data)
{
    DecodedFile result;
    result.encoding = detectEncoding(data);

    QByteArray payload = data;
    switch (result.encoding) {
    case TextEncoding::Utf8Bom:
        payload.remove(0, 3);
        break;
    case TextEncoding::Utf16LE:
    case TextEncoding::Utf16BE:
        payload.remove(0, 2);
        break;
    default:
        break;
    }

    QString text;
    switch (result.encoding) {
    case TextEncoding::Utf16LE:
        text = decodeUtf16(payload, true);
        break;
    case TextEncoding::Utf16BE:
        text = decodeUtf16(payload, false);
        break;
    case TextEncoding::Windows1252:
        text = decodeWindows1252(payload);
        break;
    case TextEncoding::Utf8:
    case TextEncoding::Utf8Bom:
    default:
        text = QString::fromUtf8(payload);
        break;
    }

    result.eol = detectLineEnding(text);
    if (!text.isEmpty() && text.front() == QChar(0xFEFF))
        text.remove(0, 1);
    result.text = normalizeLineEndings(text);
    return result;
}

QByteArray encodeFile(const QString &text, TextEncoding encoding, LineEnding eol)
{
    QString normalized = text;
    if (!normalized.isEmpty() && normalized.front() == QChar(0xFEFF))
        normalized.remove(0, 1);
    normalized = applyLineEnding(normalized, eol);

    switch (encoding) {
    case TextEncoding::Utf8Bom: {
        QByteArray out("\xEF\xBB\xBF", 3);
        out.append(normalized.toUtf8());
        return out;
    }
    case TextEncoding::Utf16LE:
        return encodeUtf16(normalized, true);
    case TextEncoding::Utf16BE:
        return encodeUtf16(normalized, false);
    case TextEncoding::Windows1252:
        return encodeWindows1252(normalized);
    case TextEncoding::Utf8:
    default:
        return normalized.toUtf8();
    }
}

} // namespace papiro
