#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace papiro {

/*!
 * A syntax description loaded from a simple INI-like `.syn` file.
 *
 * The format is intentionally close in spirit to classic desktop editors:
 * sections in brackets, `Key=Value` lines, `;` or `#` to start a comment line.
 * Recognized keys: Name, Description, Extensions, CaseSensitive, Keywords,
 * LineComment, BlockCommentStart, BlockCommentEnd, StringDelimiters,
 * MultilineStringDelimiters, EscapeChar and Numbers. See resources/syntax for
 * examples.
 */
struct SyntaxDefinition {
    QString name = QStringLiteral("Plain Text");
    QString description;
    QStringList extensions;
    bool caseSensitive = false;
    QStringList keywords;
    QString lineComment;
    QString blockCommentStart;
    QString blockCommentEnd;
    QStringList stringDelimiters;
    /*! Delimiters whose strings may continue on following lines (e.g. Python
     *  triple quotes or JavaScript template literals). */
    QStringList multilineStringDelimiters;
    QString escapeChar = QStringLiteral("\\");
    bool highlightNumbers = true;

    bool matchesFile(const QString &fileName) const;

    static SyntaxDefinition fromText(const QString &text, QString *error = nullptr);
    static SyntaxDefinition fromFile(const QString &path, QString *error = nullptr);
};

/*! Owns every known syntax definition: user files take precedence over builtins. */
class SyntaxRepository {
public:
    static SyntaxRepository &instance();

    /*! Load builtin resource definitions (:/syntax) plus *.syn from userDir. */
    void load(const QString &userDir = QString());

    /*! Definition matching the file name, or nullptr for plain text. */
    const SyntaxDefinition *forFile(const QString &fileName) const;

    const QList<SyntaxDefinition> &definitions() const { return m_definitions; }

    /*! Path of the directory scanned for user definitions. */
    static QString userDirectory();

private:
    QList<SyntaxDefinition> m_definitions;
};

} // namespace papiro
