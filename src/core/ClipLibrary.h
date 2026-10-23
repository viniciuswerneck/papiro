#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

namespace papiro {

/*! A single clip/snippet entry. */
struct Clip {
    QString id;                 // UUID
    QString title;              // user-visible title
    QString content;            // the snippet text
    QString category;           // folder/category name
    QStringList tags;           // optional tags
    QDateTime created;          // creation timestamp
    QDateTime modified;         // last modification timestamp
    QDateTime lastUsed;         // last insertion timestamp
    int useCount = 0;           // how many times inserted

    bool isValid() const { return !id.isEmpty(); }

    QJsonObject toJson() const;
    static Clip fromJson(const QJsonObject &obj);
};

/*! Clip library: persistent collection of clips with categories and search. */
class ClipLibrary : public QObject {
    Q_OBJECT

public:
    explicit ClipLibrary(QObject *parent = nullptr);

    /*! Load library from default directory. */
    bool load();

    /*! Save library to default directory. */
    bool save();

    /*! Default library file path. */
    static QString defaultPath();

    /*! All clips. */
    const QVector<Clip> &clips() const { return m_clips; }

    /*! Find clip by ID. */
    const Clip *findClip(const QString &id) const;
    Clip *findClip(const QString &id);

    /*! Add a new clip (generates UUID). Returns the new clip's ID. */
    QString addClip(const QString &title, const QString &content,
                    const QString &category = QString(),
                    const QStringList &tags = QStringList());

    /*! Update an existing clip. */
    bool updateClip(const QString &id, const QString &title,
                    const QString &content, const QString &category,
                    const QStringList &tags);

    /*! Remove a clip by ID. */
    bool removeClip(const QString &id);

    /*! Record a clip usage (updates lastUsed, increments useCount). */
    void recordUsage(const QString &id);

    /*! All unique categories. */
    QStringList categories() const;

    /*! Clips in a specific category (empty = all). */
    QVector<Clip *> clipsInCategory(const QString &category);

    /*! Search clips by text (title, content, tags, category). */
    QVector<Clip *> search(const QString &query) const;

    /*! Move clip to another category. */
    bool moveClip(const QString &id, const QString &newCategory);

    /*! Rename a category (updates all clips in it). */
    void renameCategory(const QString &oldName, const QString &newName);

    /*! Delete a category (moves clips to "Uncategorized"). */
    void deleteCategory(const QString &name);

signals:
    void libraryChanged();
    void clipAdded(const Clip &clip);
    void clipUpdated(const Clip &clip);
    void clipRemoved(const QString &id);

private:
    QVector<Clip> m_clips;
    QString m_filePath;
};

} // namespace papiro