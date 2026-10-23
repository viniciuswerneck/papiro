#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

namespace papiro {
namespace Theme {

enum class Mode {
    Light,
    Dark,
};

/*! Colors used by the editor surface and the syntax highlighter. */
struct Colors {
    QColor keyword;
    QColor comment;
    QColor string;
    QColor number;
    QColor currentLine;
    QColor bracketMatch;
    QColor gutterBackground;
    QColor gutterText;

    // Completion widget colors
    QColor completionBackground;
    QColor completionText;
    QColor completionBorder;
    QColor completionSelectionBackground;
    QColor completionSelectionText;
    QColor completionIcon;
    QColor completionMatchHighlight;
    QColor completionMatchText;
    QColor completionDeprecatedText;
    QColor scrollbarHandle;
    QColor scrollbarHandleHover;
};

Mode mode();
void setMode(Mode mode);

Colors colors(Mode mode);
inline Colors colors()
{
    return colors(mode());
}

QPalette palette(Mode mode);

/*! Applies the palette to the running QApplication and stores the mode. */
void apply(Mode mode);

QString key(Mode mode);
Mode fromKey(const QString &key, Mode fallback = Mode::Light);

} // namespace Theme
} // namespace papiro
