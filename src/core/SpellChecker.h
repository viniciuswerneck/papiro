#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

namespace papiro {

/*! Hunspell-backed spell checker.
 *
 *  Loads a dictionary pair (<lang>.aff + <lang>.dic) from a set of search
 *  directories and answers correctness / suggestion queries. Results are
 *  cached so repeated lookups during highlighting stay cheap.
 *
 *  When built without hunspell support, every word is considered correct so
 *  the rest of the editor keeps working unchanged.
 */
class SpellChecker : public QObject {
    Q_OBJECT

public:
    explicit SpellChecker(QObject *parent = nullptr);
    ~SpellChecker() override;

    /*! Try to load the dictionary for \a language ("pt_BR", "en_US", ...).
     *  Returns true on success. A base language fallback is attempted
     *  ("pt_BR" -> "pt"). */
    bool load(const QString &language);

    /*! Language code of the currently loaded dictionary ("" if none). */
    QString language() const { return m_language; }

    /*! True when a dictionary is loaded. */
    bool isLoaded() const;

    /*! Whether \a word is spelled correctly. Words with digits, underscores
     *  or fewer than 2 letters are treated as correct. */
    bool isCorrect(const QString &word) const;

    /*! Suggestions for a misspelled word (max \a limit). */
    QStringList suggestions(const QString &word, int limit = 5) const;

    /*! Add \a word to the runtime dictionary (session only). */
    void addToDictionary(const QString &word);

    /*! Directories searched for dictionaries. */
    static QStringList dictionaryDirectories();

    /*! Best dictionary suffix for a UI language code ("pt" -> "pt_BR"). */
    static QString defaultDictionaryFor(const QString &language);

private:
    void *m_handle = nullptr; // Hunspell*
    QString m_language;
    mutable QHash<QString, bool> m_cache;
};

} // namespace papiro