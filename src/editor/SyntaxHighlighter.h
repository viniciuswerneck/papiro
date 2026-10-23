#pragma once

#include "core/SyntaxDefinition.h"
#include "editor/BlockState.h"

#include <QList>
#include <QSet>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace papiro {

class SpellChecker;

/*! Generic highlighter driven entirely by a SyntaxDefinition. */
class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    SyntaxHighlighter(QTextDocument *document, const SyntaxDefinition &definition);

    /*! Enables live spell checking. Pass nullptr to disable it. */
    void setSpellChecker(SpellChecker *checker);

    /*! Re-reads the current theme colors and repaints the document. */
    void refreshTheme();

protected:
    void highlightBlock(const QString &text) override;

private:
    struct StringToken {
        QString text;
        bool multiline = false;
    };

    void applySyntax(const QString &text);
    void applySpelling(const QString &text);
    void updateFormats();
    bool startsWithAt(const QString &text, int position, const QString &token) const;
    bool isIdentifierStart(QChar ch) const;
    bool isIdentifierChar(QChar ch) const;

    /*! Position just past the closing delimiter, or -1 when it is not closed. */
    int findStringEnd(const QString &text, int from, const QString &delimiter) const;

    /*! Syntax state helpers that preserve the folding bits in userState. */
    int previousSyntaxState() const;
    void setSyntaxState(int state);

    static constexpr int BlockCommentState = 1;
    static constexpr int MultilineStringStateBase = 100;

    SyntaxDefinition m_definition;
    QSet<QString> m_keywords;
    QList<StringToken> m_stringTokens;
    bool m_hasBlockComment = false;
    bool m_hasLineComment = false;
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    SpellChecker *m_spellChecker = nullptr;
};

} // namespace papiro
