#pragma once

#include <QByteArray>
#include <QString>

namespace papiro {

/*! Text encodings supported for reading and writing documents. */
enum class TextEncoding {
    Utf8,
    Utf8Bom,
    Utf16LE,
    Utf16BE,
    Windows1252,
};

/*! Physical line terminator used when a document is written. */
enum class LineEnding {
    Lf,
    CrLf,
    Cr,
};

struct DecodedFile {
    QString text;
    TextEncoding encoding = TextEncoding::Utf8;
    LineEnding eol = LineEnding::Lf;
};

/*! Human readable labels (never translated, used in menus and status bar). */
QString encodingName(TextEncoding encoding);
QString lineEndingName(LineEnding eol);

/*! Best-effort encoding detection from BOM and UTF-8 validity. */
TextEncoding detectEncoding(const QByteArray &data);

/*! Dominant line ending of already decoded text (defaults to LF). */
LineEnding detectLineEnding(const QString &text);

/*! Decode bytes into text, normalizing line endings to '\n'. */
DecodedFile decodeFile(const QByteArray &data);

/*! Encode text using the requested encoding and line ending. */
QByteArray encodeFile(const QString &text, TextEncoding encoding, LineEnding eol);

/*! Replace CRLF and CR with LF. */
QString normalizeLineEndings(const QString &text);

/*! Replace LF with the requested line ending. */
QString applyLineEnding(const QString &text, LineEnding eol);

} // namespace papiro
