#pragma once

#include "core/SyntaxDefinition.h"

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

protected:
    void highlightBlock(const QString &text) override;

private:
    void applySyntax(const QString &text);
    void applySpelling(const QString &text);
    bool startsWithAt(const QString &text, int position, const QString &token) const;
    bool isIdentifierStart(QChar ch) const;
    bool isIdentifierChar(QChar ch) const;

    SyntaxDefinition m_definition;
    QSet<QString> m_keywords;
    bool m_hasBlockComment = false;
    bool m_hasLineComment = false;
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    SpellChecker *m_spellChecker = nullptr;
};

} // namespace papiro
