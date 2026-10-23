#include "editor/SyntaxHighlighter.h"

#include "core/SpellChecker.h"
#include "ui/Theme.h"

#include <QColor>
#include <QFont>
#include <QRegularExpression>

#include <algorithm>

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

    const auto addToken = [this](const QString &text, bool multiline) {
        if (text.isEmpty())
            return;
        for (StringToken &token : m_stringTokens) {
            if (token.text == text) {
                token.multiline = token.multiline || multiline;
                return;
            }
        }
        m_stringTokens.append({text, multiline});
    };

    for (const QString &delimiter : definition.stringDelimiters)
        addToken(delimiter, false);
    for (const QString &delimiter : definition.multilineStringDelimiters)
        addToken(delimiter, true);

    // Match the longest delimiter first so """ wins over " and ```` over `.
    std::stable_sort(m_stringTokens.begin(), m_stringTokens.end(),
                     [](const StringToken &a, const StringToken &b) {
                         return a.text.size() > b.text.size();
                     });

    updateFormats();
}

void SyntaxHighlighter::updateFormats()
{
    const Theme::Colors colors = Theme::colors();
    m_keywordFormat.setForeground(colors.keyword);
    m_keywordFormat.setFontWeight(QFont::DemiBold);

    m_commentFormat.setForeground(colors.comment);
    m_commentFormat.setFontItalic(true);

    m_stringFormat.setForeground(colors.string);
    m_numberFormat.setForeground(colors.number);

    // LSP Diagnostic formats - use theme-aware colors
    const QPalette palette = Theme::palette(Theme::mode());
    
    // Error - red squiggly underline
    m_diagErrorFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    m_diagErrorFormat.setUnderlineColor(QColor(0xD3, 0x2F, 0x2F)); // Red
    
    // Warning - orange squiggly underline
    m_diagWarningFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    m_diagWarningFormat.setUnderlineColor(QColor(0xE6, 0x8A, 0x00)); // Orange
    
    // Info - blue squiggly underline
    m_diagInfoFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    m_diagInfoFormat.setUnderlineColor(QColor(0x2F, 0x7F, 0xD3)); // Blue
    
    // Hint - gray squiggly underline
    m_diagHintFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    m_diagHintFormat.setUnderlineColor(QColor(0x8A, 0x8A, 0x8A)); // Gray
}

void SyntaxHighlighter::refreshTheme()
{
    updateFormats();
    rehighlight();
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

int SyntaxHighlighter::findStringEnd(const QString &text, int from, const QString &delimiter) const
{
    const bool allowEscape = !m_definition.escapeChar.isEmpty();
    int cursor = qMax(0, from);
    while (cursor < text.size()) {
        if (allowEscape && startsWithAt(text, cursor, m_definition.escapeChar)) {
            cursor += m_definition.escapeChar.size() + 1;
            continue;
        }
        if (startsWithAt(text, cursor, delimiter))
            return cursor + delimiter.size();
        ++cursor;
    }
    return -1;
}

int SyntaxHighlighter::previousSyntaxState() const
{
    return previousBlockState() & BlockState::SyntaxMask;
}

void SyntaxHighlighter::setSyntaxState(int state)
{
    // Keep the fold bit (and any other non-syntax bits) untouched.
    const int preserved = currentBlock().userState() & ~BlockState::SyntaxMask;
    setCurrentBlockState(preserved | (state & BlockState::SyntaxMask));
}

void SyntaxHighlighter::setSpellChecker(SpellChecker *checker)
{
    if (m_spellChecker == checker)
        return;
    m_spellChecker = checker;
    rehighlight();
}

void SyntaxHighlighter::setLSPDiagnostics(const QVector<LSP::Diagnostic>& diagnostics)
{
    m_lspDiagnostics = diagnostics;
    rehighlight();
}

void SyntaxHighlighter::clearLSPDiagnostics()
{
    m_lspDiagnostics.clear();
    rehighlight();
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    applySyntax(text);
    applySpelling(text);
    applyLSPDiagnostics(text);
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

void SyntaxHighlighter::applyLSPDiagnostics(const QString &text)
{
    if (m_lspDiagnostics.isEmpty())
        return;

    const int blockNumber = currentBlock().blockNumber();
    
    for (const LSP::Diagnostic& diag : m_lspDiagnostics) {
        // Check if this diagnostic applies to the current block
        if (diag.range.start.line > blockNumber || diag.range.end.line < blockNumber)
            continue;
        
        int startColumn = 0;
        int endColumn = text.length();
        
        if (diag.range.start.line == blockNumber) {
            startColumn = diag.range.start.character;
        }
        
        if (diag.range.end.line == blockNumber) {
            endColumn = diag.range.end.character;
        } else if (diag.range.end.line > blockNumber) {
            endColumn = text.length();
        }
        
        // Clamp to valid range
        startColumn = qBound(0, startColumn, text.length());
        endColumn = qBound(startColumn, endColumn, text.length());
        
        if (endColumn <= startColumn)
            continue;
        
        const int length = endColumn - startColumn;
        const QTextCharFormat format = diagnosticFormat(diag.severity.value_or(LSP::DiagnosticSeverity::Error));
        setFormat(startColumn, length, format);
    }
}

QTextCharFormat SyntaxHighlighter::diagnosticFormat(LSP::DiagnosticSeverity severity) const
{
    switch (severity) {
    case LSP::DiagnosticSeverity::Error:
        return m_diagErrorFormat;
    case LSP::DiagnosticSeverity::Warning:
        return m_diagWarningFormat;
    case LSP::DiagnosticSeverity::Information:
        return m_diagInfoFormat;
    case LSP::DiagnosticSeverity::Hint:
        return m_diagHintFormat;
    default:
        return m_diagErrorFormat;
    }
}

void SyntaxHighlighter::applySyntax(const QString &text)
{
    static const QRegularExpression numberExpression(
        QStringLiteral("(?:0[xX][0-9a-fA-F]+|\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)"));

    const int length = text.size();
    int position = 0;
    const int previous = previousSyntaxState();

    if (previous == BlockCommentState) {
        if (m_hasBlockComment) {
            const int end = text.indexOf(m_definition.blockCommentEnd);
            if (end < 0) {
                setFormat(0, length, m_commentFormat);
                setSyntaxState(BlockCommentState);
                return;
            }
            const int stop = end + m_definition.blockCommentEnd.size();
            setFormat(0, stop, m_commentFormat);
            position = stop;
        }
    } else if (previous >= MultilineStringStateBase) {
        const int index = previous - MultilineStringStateBase;
        if (index >= 0 && index < m_stringTokens.size() && m_stringTokens.at(index).multiline) {
            const QString &delimiter = m_stringTokens.at(index).text;
            const int stop = findStringEnd(text, 0, delimiter);
            if (stop < 0) {
                setFormat(0, length, m_stringFormat);
                setSyntaxState(previous);
                return;
            }
            setFormat(0, stop, m_stringFormat);
            position = stop;
        }
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
                setSyntaxState(BlockCommentState);
                return;
            }
            const int stop = end + m_definition.blockCommentEnd.size();
            setFormat(position, stop - position, m_commentFormat);
            position = stop;
            continue;
        }

        bool stringHandled = false;
        for (int i = 0; i < m_stringTokens.size(); ++i) {
            const StringToken &token = m_stringTokens.at(i);
            if (!startsWithAt(text, position, token.text))
                continue;

            const int end = findStringEnd(text, position + token.text.size(), token.text);
            if (end < 0) {
                setFormat(position, length - position, m_stringFormat);
                setSyntaxState(token.multiline ? MultilineStringStateBase + i : 0);
                return;
            }

            setFormat(position, end - position, m_stringFormat);
            position = end;
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

    setSyntaxState(0);
}

} // namespace papiro