#include "core/SpellChecker.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

#ifdef PAPIRO_HAVE_HUNSPELL
#include <hunspell/hunspell.hxx>
#endif

namespace papiro {

SpellChecker::SpellChecker(QObject *parent)
    : QObject(parent)
{
}

SpellChecker::~SpellChecker()
{
#ifdef PAPIRO_HAVE_HUNSPELL
    delete static_cast<Hunspell *>(m_handle);
#endif
    m_handle = nullptr;
}

QStringList SpellChecker::dictionaryDirectories()
{
    QStringList dirs;
    // User-local dictionaries extracted alongside the app data.
    dirs << QDir::homePath() + QStringLiteral("/.local/share/hunspell");
    dirs << QDir::homePath() + QStringLiteral("/.local/share/myspell");
    // Standard system locations.
    dirs << QStringLiteral("/usr/share/hunspell");
    dirs << QStringLiteral("/usr/share/myspell");
    dirs << QStringLiteral("/usr/share/myspell/dicts");
    // macOS / Windows common locations.
    dirs << QStringLiteral("/Library/Spelling");
    dirs << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/dicts");
    // Deduplicate while preserving order.
    QStringList unique;
    for (const QString &d : dirs) {
        if (!unique.contains(d))
            unique << d;
    }
    return unique;
}

QString SpellChecker::defaultDictionaryFor(const QString &language)
{
    if (language.isEmpty())
        return QStringLiteral("en_US");
    if (language.startsWith(QLatin1String("pt")))
        return QStringLiteral("pt_BR");
    if (language.startsWith(QLatin1String("es")))
        return QStringLiteral("es_ES");
    if (language.startsWith(QLatin1String("en")))
        return QStringLiteral("en_US");
    return language;
}

bool SpellChecker::isLoaded() const
{
#ifdef PAPIRO_HAVE_HUNSPELL
    return m_handle != nullptr;
#else
    return false;
#endif
}

bool SpellChecker::load(const QString &language)
{
#ifdef PAPIRO_HAVE_HUNSPELL
    delete static_cast<Hunspell *>(m_handle);
    m_handle = nullptr;
    m_cache.clear();
    m_language.clear();

    if (language.isEmpty())
        return false;

    QStringList candidates;
    candidates << language;
    // Base-language fallback: "pt_BR" -> "pt".
    if (language.contains(QLatin1Char('_')))
        candidates << language.left(language.indexOf(QLatin1Char('_')));
    // Region default: "pt" -> "pt_BR".
    if (!language.contains(QLatin1Char('_')))
        candidates << defaultDictionaryFor(language);

    for (const QString &candidate : candidates) {
        for (const QString &dir : dictionaryDirectories()) {
            const QString aff = dir + QLatin1Char('/') + candidate + QStringLiteral(".aff");
            const QString dic = dir + QLatin1Char('/') + candidate + QStringLiteral(".dic");
            if (!QFileInfo::exists(aff) || !QFileInfo::exists(dic))
                continue;
            auto *hunspell = new Hunspell(aff.toLocal8Bit().constData(),
                                          dic.toLocal8Bit().constData());
            if (hunspell->get_dict_encoding().empty()) {
                delete hunspell;
                continue;
            }
            m_handle = hunspell;
            m_language = candidate;
            return true;
        }
    }
    return false;
#else
    Q_UNUSED(language);
    return false;
#endif
}

bool SpellChecker::isCorrect(const QString &word) const
{
    if (word.size() < 2)
        return true;
    // Skip tokens that are obviously code / numbers / paths.
    for (const QChar ch : word) {
        if (ch.isDigit() || ch == QLatin1Char('_') || ch == QLatin1Char('/')
            || ch == QLatin1Char('\\') || ch == QLatin1Char('@'))
            return true;
    }

#ifdef PAPIRO_HAVE_HUNSPELL
    if (!m_handle)
        return true;

    const auto cached = m_cache.constFind(word);
    if (cached != m_cache.constEnd())
        return cached.value();

    auto *hunspell = static_cast<Hunspell *>(m_handle);
    const bool ok = hunspell->spell(word.toUtf8().toStdString());
    m_cache.insert(word, ok);
    return ok;
#else
    return true;
#endif
}

QStringList SpellChecker::suggestions(const QString &word, int limit) const
{
    QStringList result;
#ifdef PAPIRO_HAVE_HUNSPELL
    if (!m_handle)
        return result;
    auto *hunspell = static_cast<Hunspell *>(m_handle);
    std::vector<std::string> suggestions = hunspell->suggest(word.toUtf8().toStdString());
    for (const std::string &s : suggestions) {
        result << QString::fromUtf8(s.c_str());
        if (result.size() >= limit)
            break;
    }
#else
    Q_UNUSED(word);
    Q_UNUSED(limit);
#endif
    return result;
}

void SpellChecker::addToDictionary(const QString &word)
{
    if (word.size() < 2)
        return;
#ifdef PAPIRO_HAVE_HUNSPELL
    if (m_handle) {
        auto *hunspell = static_cast<Hunspell *>(m_handle);
        hunspell->add(word.toUtf8().toStdString());
    }
#endif
    m_cache.insert(word, true);
}

} // namespace papiro
