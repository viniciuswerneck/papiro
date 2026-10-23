#pragma once

#include <QString>
#include <QStringList>

namespace papiro {

/*! A saved editing session: open files, active tab and the explorer root. */
struct Workspace {
    QString name;
    QString rootPath;
    QStringList files;
    int activeIndex = 0;

    bool isValid() const { return !files.isEmpty() || !rootPath.isEmpty(); }

    static Workspace load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;

    static QString fileFilter();
    static QString defaultSuffix();
};

} // namespace papiro
