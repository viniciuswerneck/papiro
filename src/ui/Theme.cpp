#include "ui/Theme.h"

#include <QApplication>

namespace papiro {
namespace Theme {
namespace {

Mode g_mode = Mode::Light;

} // namespace

Mode mode()
{
    return g_mode;
}

void setMode(Mode mode)
{
    g_mode = mode;
}

Colors colors(Mode mode)
{
    Colors result;
    if (mode == Mode::Dark) {
        result.keyword = QColor(0x56, 0x9C, 0xD6);
        result.comment = QColor(0x6A, 0x99, 0x55);
        result.string = QColor(0xCE, 0x91, 0x78);
        result.number = QColor(0xB5, 0xCE, 0xA8);
        result.currentLine = QColor(0x2A, 0x2D, 0x2E);
        result.bracketMatch = QColor(0x3A, 0x3D, 0x41);
        result.gutterBackground = QColor(0x25, 0x25, 0x26);
        result.gutterText = QColor(0x85, 0x85, 0x85);
    } else {
        result.keyword = QColor(0x00, 0x33, 0xB3);
        result.comment = QColor(0x8A, 0x8A, 0x8A);
        result.string = QColor(0x06, 0x7D, 0x17);
        result.number = QColor(0x17, 0x50, 0xEB);
        result.currentLine = QColor(0xF5, 0xF7, 0xFA);
        result.bracketMatch = QColor(0xDF, 0xEB, 0xFF);
        result.gutterBackground = QColor(0xF2, 0xF2, 0xF2);
        result.gutterText = QColor(0x8A, 0x8A, 0x8A);
    }
    return result;
}

QPalette palette(Mode mode)
{
    QPalette palette;
    if (mode == Mode::Dark) {
        const QColor window(0x2D, 0x2D, 0x30);
        const QColor base(0x1E, 0x1E, 0x1E);
        const QColor text(0xD4, 0xD4, 0xD4);
        const QColor disabled(0x7A, 0x7A, 0x7A);

        palette.setColor(QPalette::Window, window);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::AlternateBase, QColor(0x25, 0x25, 0x26));
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Button, QColor(0x33, 0x33, 0x37));
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, Qt::white);
        palette.setColor(QPalette::Highlight, QColor(0x26, 0x4F, 0x78));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::ToolTipBase, QColor(0x25, 0x25, 0x26));
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::PlaceholderText, QColor(0x9A, 0x9A, 0x9A));
        palette.setColor(QPalette::Link, QColor(0x4F, 0x9C, 0xE8));
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    } else {
        palette.setColor(QPalette::Window, QColor(0xF0, 0xF0, 0xF0));
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor(0xF7, 0xF7, 0xF7));
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Button, QColor(0xF0, 0xF0, 0xF0));
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::BrightText, Qt::white);
        palette.setColor(QPalette::Highlight, QColor(0x30, 0x8C, 0xC6));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::ToolTipBase, QColor(0xFF, 0xFF, 0xDC));
        palette.setColor(QPalette::ToolTipText, Qt::black);
        palette.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
        palette.setColor(QPalette::Link, QColor(0x00, 0x00, 0xFF));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x9A, 0x9A, 0x9A));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x9A, 0x9A, 0x9A));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x9A, 0x9A, 0x9A));
    }
    return palette;
}

void apply(Mode mode)
{
    setMode(mode);
    if (auto *application = qobject_cast<QApplication *>(QCoreApplication::instance()))
        application->setPalette(palette(mode));
}

QString key(Mode mode)
{
    return mode == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("light");
}

Mode fromKey(const QString &key, Mode fallback)
{
    if (key.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0)
        return Mode::Dark;
    if (key.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0)
        return Mode::Light;
    return fallback;
}

} // namespace Theme
} // namespace papiro
