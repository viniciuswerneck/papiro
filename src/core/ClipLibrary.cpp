#include "core/ClipLibrary.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUuid>

namespace papiro {

// ---------------------------------------------------------------------------
// Clip
// ---------------------------------------------------------------------------

QJsonObject Clip::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["title"] = title;
    obj["content"] = content;
    obj["category"] = category;
    obj["tags"] = QJsonArray::fromStringList(tags);
    obj["created"] = created.toString(Qt::ISODate);
    obj["modified"] = modified.toString(Qt::ISODate);
    obj["lastUsed"] = lastUsed.toString(Qt::ISODate);
    obj["useCount"] = useCount;
    return obj;
}

Clip Clip::fromJson(const QJsonObject &obj) {
    Clip clip;
    clip.id = obj["id"].toString();
    clip.title = obj["title"].toString();
    clip.content = obj["content"].toString();
    clip.category = obj["category"].toString();
    QJsonArray tagsArray = obj["tags"].toArray();
    for (const QJsonValue &v : tagsArray) {
        clip.tags.append(v.toString());
    }
    clip.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    clip.modified = QDateTime::fromString(obj["modified"].toString(), Qt::ISODate);
    clip.lastUsed = QDateTime::fromString(obj["lastUsed"].toString(), Qt::ISODate);
    clip.useCount = obj["useCount"].toInt(0);
    return clip;
}

// ---------------------------------------------------------------------------
// ClipLibrary
// ---------------------------------------------------------------------------

static QString generateUuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

ClipLibrary::ClipLibrary(QObject *parent)
    : QObject(parent)
    , m_filePath(defaultPath()) {
}

QString ClipLibrary::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/cliplib.json");
}

bool ClipLibrary::load() {
    QFile file(m_filePath);
    if (!file.exists())
        return true; // empty library is fine

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return false;
    }

    m_clips.clear();
    QJsonArray array = doc.array();
    for (const QJsonValue &val : array) {
        if (val.isObject()) {
            Clip clip = Clip::fromJson(val.toObject());
            if (clip.isValid()) {
                m_clips.append(clip);
            }
        }
    }
    return true;
}

bool ClipLibrary::save() {
    QDir dir = QFileInfo(m_filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonArray array;
    for (const Clip &clip : m_clips) {
        array.append(clip.toJson());
    }
    QJsonDocument doc(array);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

const Clip *ClipLibrary::findClip(const QString &id) const {
    for (const Clip &clip : m_clips) {
        if (clip.id == id)
            return &clip;
    }
    return nullptr;
}

Clip *ClipLibrary::findClip(const QString &id) {
    for (Clip &clip : m_clips) {
        if (clip.id == id)
            return &clip;
    }
    return nullptr;
}

QString ClipLibrary::addClip(const QString &title, const QString &content,
                             const QString &category, const QStringList &tags) {
    Clip clip;
    clip.id = generateUuid();
    clip.title = title.isEmpty() ? tr("Untitled") : title;
    clip.content = content;
    clip.category = category.isEmpty() ? QStringLiteral("Uncategorized") : category;
    clip.tags = tags;
    clip.created = QDateTime::currentDateTime();
    clip.modified = clip.created;
    clip.lastUsed = QDateTime();
    clip.useCount = 0;

    m_clips.prepend(clip); // newest first
    emit clipAdded(clip);
    emit libraryChanged();
    return clip.id;
}

bool ClipLibrary::updateClip(const QString &id, const QString &title,
                             const QString &content, const QString &category,
                             const QStringList &tags) {
    Clip *clip = findClip(id);
    if (!clip)
        return false;

    clip->title = title.isEmpty() ? tr("Untitled") : title;
    clip->content = content;
    clip->category = category.isEmpty() ? QStringLiteral("Uncategorized") : category;
    clip->tags = tags;
    clip->modified = QDateTime::currentDateTime();

    emit clipUpdated(*clip);
    emit libraryChanged();
    return true;
}

bool ClipLibrary::removeClip(const QString &id) {
    for (int i = 0; i < m_clips.size(); ++i) {
        if (m_clips[i].id == id) {
            m_clips.removeAt(i);
            emit clipRemoved(id);
            emit libraryChanged();
            return true;
        }
    }
    return false;
}

void ClipLibrary::recordUsage(const QString &id) {
    Clip *clip = findClip(id);
    if (clip) {
        clip->lastUsed = QDateTime::currentDateTime();
        ++clip->useCount;
    }
}

QStringList ClipLibrary::categories() const {
    QStringList cats;
    for (const Clip &clip : m_clips) {
        if (!clip.category.isEmpty() && !cats.contains(clip.category)) {
            cats.append(clip.category);
        }
    }
    cats.sort(Qt::CaseInsensitive);
    if (!cats.contains(QStringLiteral("Uncategorized"))) {
        cats.prepend(QStringLiteral("Uncategorized"));
    }
    return cats;
}

QVector<Clip *> ClipLibrary::clipsInCategory(const QString &category) {
    QVector<Clip *> result;
    for (Clip &clip : m_clips) {
        if (category.isEmpty() || clip.category == category) {
            result.append(&clip);
        }
    }
    return result;
}

QVector<Clip *> ClipLibrary::search(const QString &query) const {
    if (query.trimmed().isEmpty()) {
        QVector<Clip *> all;
        for (const Clip &clip : m_clips) {
            all.append(const_cast<Clip *>(&clip));
        }
        return all;
    }

    QString q = query.toLower();
    QVector<Clip *> result;
    for (const Clip &clip : m_clips) {
        if (clip.title.toLower().contains(q)
            || clip.content.toLower().contains(q)
            || clip.category.toLower().contains(q)) {
            result.append(const_cast<Clip *>(&clip));
            continue;
        }
        for (const QString &tag : clip.tags) {
            if (tag.toLower().contains(q)) {
                result.append(const_cast<Clip *>(&clip));
                break;
            }
        }
    }
    return result;
}

bool ClipLibrary::moveClip(const QString &id, const QString &newCategory) {
    Clip *clip = findClip(id);
    if (!clip)
        return false;
    clip->category = newCategory.isEmpty() ? QStringLiteral("Uncategorized") : newCategory;
    clip->modified = QDateTime::currentDateTime();
    emit clipUpdated(*clip);
    emit libraryChanged();
    return true;
}

void ClipLibrary::renameCategory(const QString &oldName, const QString &newName) {
    if (oldName == newName || oldName.isEmpty())
        return;
    bool changed = false;
    for (Clip &clip : m_clips) {
        if (clip.category == oldName) {
            clip.category = newName.isEmpty() ? QStringLiteral("Uncategorized") : newName;
            clip.modified = QDateTime::currentDateTime();
            changed = true;
        }
    }
    if (changed)
        emit libraryChanged();
}

void ClipLibrary::deleteCategory(const QString &name) {
    if (name.isEmpty())
        return;
    bool changed = false;
    for (Clip &clip : m_clips) {
        if (clip.category == name) {
            clip.category = QStringLiteral("Uncategorized");
            clip.modified = QDateTime::currentDateTime();
            changed = true;
        }
    }
    if (changed)
        emit libraryChanged();
}

} // namespace papiro