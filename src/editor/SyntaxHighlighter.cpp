#include "editor/SyntaxHighlighter.h"

#include "core/SpellChecker.h"

#include <QColor>
#include <QFont>
#include <QRegularExpression>

namespace papiro {

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *document, const SyntaxDefinition &definition)
    : QSyntaxHighlighter(document)
    , m_definition(definition)
{
    for (const QString &keyword : definition.keywords) {
        m_keywords.insert(definition.caseSensitive ? keyword : keyword.toLower());
    }
    m_hasBlockComment = !definition.blockCommentStart.isEmpty()
        && !definition.blockCommentEnd.isEmpty();
    m_hasLineComment = !definition.lineComment.isEmpty();

    m_keywordFormat.setForeground(QColor(0x00, 0x33, 0xB3));
    m_keywordFormat.setFontWeight(QFont::DemiBold);

    m_commentFormat.setForeground(QColor(0x8A, 0x8A, 0x8A));
    m_commentFormat.setFontItalic(true);

    m_stringFormat.setForeground(QColor(0x06, 0x7D, 0x17));
    m_numberFormat.setForeground(QColor(0x17, 0x50, 0xEB));
}

bool SyntaxHighlighter::startsWithAt(const QString &text, int position, const QString &token) const
{
    if (token.isEmpty())
        return false;
    if (position + token.size() > text.size())
        return false;
    return text.mid(position, token.size()) == token;
}

bool SyntaxHighlighter::isIdentifierStart(QChar ch) const
{
    return ch.isLetter() || ch == QLatin1Char('_');
}

bool SyntaxHighlighter::isIdentifierChar(QChar ch) const
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_');
}

void SyntaxHighlighter::setSpellChecker(SpellChecker *checker)
{
    if (m_spellChecker == checker)
        return;
    m_spellChecker = checker;
    rehighlight();
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    applySyntax(text);
    applySpelling(text);
}

void SyntaxHighlighter::applySpelling(const QString &text)
{
    if (!m_spellChecker || !m_spellChecker->isLoaded())
        return;

    static const QRegularExpression wordExpression(
        QStringLiteral("[\\p{L}][\\p{L}'\\x{2019}\\-]*"),
        QRegularExpression::UseUnicodePropertiesOption);

    QTextCharFormat misspelled;
    misspelled.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    misspelled.setUnderlineColor(QColor(0xD3, 0x2F, 0x2F));

    auto it = wordExpression.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString word = match.captured();
        if (word.size() < 2)
            continue;
        // Don't flag language keywords or identifiers with digits.
        const QString key = m_definition.caseSensitive ? word : word.toLower();
        if (m_keywords.contains(key))
            continue;
        if (!m_spellChecker->isCorrect(word))
            setFormat(match.capturedStart(), match.capturedLength(), misspelled);
    }
}

void SyntaxHighlighter::applySyntax(const QString &text)
{
    static const QRegularExpression numberExpression(
        QStringLiteral("(?:0[xX][0-9a-fA-F]+|\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)"));

    const int length = text.size();
    int position = 0;

    if (previousBlockState() == 1 && m_hasBlockComment) {
        const int end = text.indexOf(m_definition.blockCommentEnd);
        if (end < 0) {
            setFormat(0, length, m_commentFormat);
            setCurrentBlockState(1);
            return;
        }
        const int stop = end + m_definition.blockCommentEnd.size();
        setFormat(0, stop, m_commentFormat);
        position = stop;
    }

    if (previousBlockState() == 2 && !m_definition.stringDelimiters.isEmpty()) {
        const QString delimiter = m_definition.stringDelimiters.first();
        const bool allowEscape = !m_definition.escapeChar.isEmpty();
        int cursor = 0;
        while (cursor < length) {
            if (allowEscape && startsWithAt(text, cursor, m_definition.escapeChar)) {
                cursor += m_definition.escapeChar.size() + 1;
                continue;
            }
            if (startsWithAt(text, cursor, delimiter)) {
                cursor += delimiter.size();
                break;
            }
            ++cursor;
        }
        cursor = qMin(cursor, length);
        setFormat(0, cursor, m_stringFormat);
        if (cursor >= length) {
            setCurrentBlockState(2);
            return;
        }
        position = cursor;
    }

    while (position < length) {
        const QChar ch = text.at(position);

        if (m_hasLineComment && startsWithAt(text, position, m_definition.lineComment)) {
            setFormat(position, length - position, m_commentFormat);
            break;
        }

        if (m_hasBlockComment && startsWithAt(text, position, m_definition.blockCommentStart)) {
            const int end = text.indexOf(m_definition.blockCommentEnd,
                                         position + m_definition.blockCommentStart.size());
            if (end < 0) {
                setFormat(position, length - position, m_commentFormat);
                setCurrentBlockState(1);
                break;
            }
            const int stop = end + m_definition.blockCommentEnd.size();
            setFormat(position, stop - position, m_commentFormat);
            position = stop;
            continue;
        }

        bool stringHandled = false;
        for (const QString &delimiter : m_definition.stringDelimiters) {
            if (delimiter.isEmpty() || !startsWithAt(text, position, delimiter))
                continue;

            const bool allowEscape = !m_definition.escapeChar.isEmpty();
            int cursor = position + delimiter.size();
            bool closed = false;
            while (cursor < length) {
                if (allowEscape && startsWithAt(text, cursor, m_definition.escapeChar)) {
                    cursor += m_definition.escapeChar.size() + 1;
                    continue;
                }
                if (startsWithAt(text, cursor, delimiter)) {
                    cursor += delimiter.size();
                    closed = true;
                    break;
                }
                ++cursor;
            }
            cursor = qMin(cursor, length);
            setFormat(position, cursor - position, m_stringFormat);
            if (!closed) {
                setCurrentBlockState(2);
                return;
            }
            position = cursor;
            stringHandled = true;
            break;
        }
        if (stringHandled)
            continue;

        if (m_definition.highlightNumbers && ch.isDigit()) {
            const bool previousIsIdentifier = position > 0 && isIdentifierChar(text.at(position - 1));
            if (!previousIsIdentifier) {
                const QRegularExpressionMatch match = numberExpression.match(
                    text, position, QRegularExpression::NormalMatch,
                    QRegularExpression::AnchorAtOffsetMatchOption);
                if (match.hasMatch() && match.capturedLength() > 0) {
                    setFormat(position, match.capturedLength(), m_numberFormat);
                    position += match.capturedLength();
                    continue;
                }
            }
            ++position;
            continue;
        }

        if (isIdentifierStart(ch)) {
            int end = position + 1;
            while (end < length && isIdentifierChar(text.at(end)))
                ++end;
            const QString word = text.mid(position, end - position);
            const QString key = m_definition.caseSensitive ? word : word.toLower();
            if (m_keywords.contains(key))
                setFormat(position, end - position, m_keywordFormat);
            position = end;
            continue;
        }

        ++position;
    }

    setCurrentBlockState(0);
}

} // namespace papiro
