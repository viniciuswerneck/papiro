#include "ui/LSPHoverWidget.h"

#include "editor/Editor.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFocusEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace papiro {

namespace {

QString escapeHtml(const QString& text) {
    QString result = text;
    result.replace("&", "&");
    result.replace("<", "<");
    result.replace(">", ">");
    return result;
}

} // namespace

LSPHoverWidget::LSPHoverWidget(Editor* editor, QWidget* parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_editor(editor)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);

    setupUi();
}

LSPHoverWidget::~LSPHoverWidget() = default;

void LSPHoverWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFocusPolicy(Qt::NoFocus);
    m_scrollArea->setMouseTracking(true);
    m_scrollArea->viewport()->setMouseTracking(true);
    m_scrollArea->installEventFilter(this);
    m_scrollArea->viewport()->installEventFilter(this);

    m_contentLabel = new QLabel(m_scrollArea);
    m_contentLabel->setWordWrap(true);
    m_contentLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_contentLabel->setOpenExternalLinks(false);
    m_contentLabel->setMouseTracking(true);
    m_contentLabel->installEventFilter(this);

    m_scrollArea->setWidget(m_contentLabel);
    layout->addWidget(m_scrollArea);

    m_textDocument = new QTextDocument(this);
    m_textDocument->setDefaultFont(m_font);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(HIDE_DELAY_MS);
    connect(m_hideTimer, &QTimer::timeout, this, &LSPHoverWidget::hideHover);

    m_hoverCheckTimer = new QTimer(this);
    m_hoverCheckTimer->setInterval(HOVER_CHECK_INTERVAL_MS);
    connect(m_hoverCheckTimer, &QTimer::timeout, this, [this]() {
        QPoint globalPos = QCursor::pos();
        QPoint localPos = mapFromGlobal(globalPos);
        bool inside = rect().contains(localPos) && isVisible();

        if (!inside && m_mouseInside) {
            m_mouseInside = false;
            startHideTimer();
        } else if (inside && !m_mouseInside) {
            m_mouseInside = true;
            stopHideTimer();
        }
    });

    // Apply theme styling
    const auto& colors = Theme::colors();
    setStyleSheet(QStringLiteral(R"(
        LSPHoverWidget {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 4px;
        }
        QScrollArea {
            background-color: %1;
            border: none;
        }
        QLabel {
            background-color: %1;
            color: %3;
            padding: 8px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: %4;
            border-radius: 3px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: %5;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )")
        .arg(colors.completionBackground.name())
        .arg(colors.completionBorder.name())
        .arg(colors.completionText.name())
        .arg(colors.scrollbarHandle.name())
        .arg(colors.scrollbarHandleHover.name()));

    connect(m_contentLabel, &QLabel::linkActivated, this, [this](const QString& link) {
        emit linkActivated(link);
    });
}

void LSPHoverWidget::showHover(const LSP::Hover& hover, const QPoint& globalPos)
{
    updateContent(hover);
    updateGeometry(globalPos);

    m_mouseInside = true;
    stopHideTimer();

    show();
    raise();
    m_hoverCheckTimer->start();
}

void LSPHoverWidget::hideHover()
{
    if (isVisible()) {
        m_hoverCheckTimer->stop();
        stopHideTimer();
        hide();
        m_mouseInside = false;
    }
}

bool LSPHoverWidget::isVisible() const
{
    return QWidget::isVisible();
}

void LSPHoverWidget::setFont(const QFont& font)
{
    m_font = font;
    if (m_contentLabel) {
        m_contentLabel->setFont(font);
    }
    if (m_textDocument) {
        m_textDocument->setDefaultFont(font);
    }
}

void LSPHoverWidget::updateContent(const LSP::Hover& hover)
{
    QString html = hoverToHtml(hover);
    m_contentLabel->setText(html);
    m_contentLabel->adjustSize();
}

void LSPHoverWidget::updateGeometry(const QPoint& globalPos)
{
    if (!m_editor)
        return;

    // Calculate size based on content
    m_contentLabel->adjustSize();
    QSize contentSize = m_contentLabel->sizeHint();
    
    int width = qBound(200, contentSize.width() + 20, MAX_WIDTH);
    int height = qBound(50, contentSize.height() + 20, MAX_HEIGHT);

    // Position near cursor, avoiding screen edges
    QPoint pos = globalPos;
    QRect screenGeom = QApplication::primaryScreen()->availableGeometry();

    // Try below cursor first
    QRect cursorRect = m_editor->cursorRect();
    QPoint belowPos = m_editor->viewport()->mapToGlobal(QPoint(cursorRect.left(), cursorRect.bottom() + 4));
    QPoint abovePos = m_editor->viewport()->mapToGlobal(QPoint(cursorRect.left(), cursorRect.top() - height - 4));

    // Check if below fits
    bool fitsBelow = (belowPos.y() + height <= screenGeom.bottom());
    bool fitsAbove = (abovePos.y() >= screenGeom.top());

    if (fitsBelow) {
        pos = belowPos;
    } else if (fitsAbove) {
        pos = abovePos;
    } else {
        // Neither fits perfectly, use below and let it clip
        pos = belowPos;
    }

    // Horizontal adjustment
    if (pos.x() + width > screenGeom.right()) {
        pos.setX(screenGeom.right() - width);
    }
    if (pos.x() < screenGeom.left()) {
        pos.setX(screenGeom.left());
    }

    setFixedSize(width, height);
    move(pos);
}

QString LSPHoverWidget::hoverToHtml(const LSP::Hover& hover)
{
    QString html;
    QString baseStyle = QStringLiteral(R"(
        <style>
            body { font-family: '%1'; font-size: %2pt; color: %3; margin: 0; padding: 0; }
            h1, h2, h3, h4, h5, h6 { margin: 0.5em 0 0.3em; font-weight: bold; color: %4; }
            h1 { font-size: 1.4em; border-bottom: 1px solid %5; padding-bottom: 0.2em; }
            h2 { font-size: 1.2em; border-bottom: 1px solid %5; padding-bottom: 0.2em; }
            h3 { font-size: 1.1em; }
            code { background-color: %6; padding: 0.1em 0.3em; border-radius: 3px; font-family: monospace; font-size: 0.9em; }
            pre { background-color: %6; padding: 0.5em; border-radius: 4px; overflow: auto; margin: 0.5em 0; }
            pre code { background-color: transparent; padding: 0; font-size: 0.85em; }
            a { color: %7; text-decoration: none; }
            a:hover { text-decoration: underline; }
            ul, ol { margin: 0.5em 0; padding-left: 1.5em; }
            li { margin: 0.2em 0; }
            blockquote { border-left: 3px solid %5; padding-left: 0.8em; margin: 0.5em 0; color: %8; font-style: italic; }
            table { border-collapse: collapse; margin: 0.5em 0; }
            th, td { border: 1px solid %5; padding: 0.3em 0.6em; }
            th { background-color: %6; font-weight: bold; }
            hr { border: none; border-top: 1px solid %5; margin: 0.8em 0; }
            p { margin: 0.4em 0; }
        </style>
    )")
        .arg(m_font.family())
        .arg(m_font.pointSize() > 0 ? m_font.pointSize() : 10)
        .arg(Theme::colors().completionText.name())
        .arg(Theme::colors().completionText.name())
        .arg(Theme::colors().completionBorder.name())
        .arg(Theme::colors().completionBackground.lighter(120).name())
        .arg(Theme::colors().keyword.name())
        .arg(Theme::colors().comment.name());

    html = "<!DOCTYPE html><html><head>" + baseStyle + "</head><body>";

    auto markupContentToHtmlStatic = [this](const LSP::MarkupContent& content) -> QString {
        if (content.kind.compare("markdown", Qt::CaseInsensitive) == 0) {
            m_textDocument->setMarkdown(content.value);
            return m_textDocument->toHtml();
        } else {
            QString escaped = escapeHtml(content.value);
            escaped.replace("\n", "<br/>");
            return QString("<p>%1</p>").arg(escaped);
        }
    };

    // Use std::visit to handle the variant
    struct Visitor {
        std::function<QString(const LSP::MarkupContent&)> markupToHtml;

        Visitor(std::function<QString(const LSP::MarkupContent&)> f) : markupToHtml(std::move(f)) {}

        QString operator()(const QString& text) const {
            QString escaped = escapeHtml(text);
            escaped.replace("\n", "<br/>");
            return QString("<p>%1</p>").arg(escaped);
        }
        QString operator()(const LSP::MarkupContent& content) const {
            return markupToHtml(content);
        }
        QString operator()(const QVector<LSP::MarkupContent>& contents) const {
            QString result;
            for (const auto& mc : contents) {
                result += markupToHtml(mc);
            }
            return result;
        }
    };

    html += std::visit(Visitor{markupContentToHtmlStatic}, hover.contents);

    // Add range info if available
    if (hover.range.has_value()) {
        const auto& range = hover.range.value();
        html += QString("<hr/><p style='font-size: 0.8em; color: %1;'>Range: %2:%3 - %4:%5</p>")
            .arg(Theme::colors().comment.name())
            .arg(range.start.line + 1)
            .arg(range.start.character + 1)
            .arg(range.end.line + 1)
            .arg(range.end.character + 1);
    }

    html += "</body></html>";
    return html;
}

bool LSPHoverWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_contentLabel || watched == m_scrollArea || watched == m_scrollArea->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            m_mouseInside = true;
            stopHideTimer();
        } else if (event->type() == QEvent::Leave) {
            m_mouseInside = false;
            startHideTimer();
        } else if (event->type() == QEvent::MouseButtonPress) {
            // Click outside content area (on scrollbar, etc.) - hide
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (!m_contentLabel->geometry().contains(mouseEvent->pos())) {
                hideHover();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LSPHoverWidget::leaveEvent(QEvent* event)
{
    m_mouseInside = false;
    startHideTimer();
    QWidget::leaveEvent(event);
}

void LSPHoverWidget::focusOutEvent(QFocusEvent* event)
{
    // Don't hide immediately - give time for click on link
    QTimer::singleShot(100, this, [this]() {
        if (isVisible() && !hasFocus() && !m_contentLabel->hasFocus() && !m_scrollArea->hasFocus()) {
            QPoint globalPos = QCursor::pos();
            if (!rect().contains(mapFromGlobal(globalPos)) &&
                !m_contentLabel->rect().contains(m_contentLabel->mapFromGlobal(globalPos)) &&
                !m_scrollArea->rect().contains(m_scrollArea->mapFromGlobal(globalPos))) {
                hideHover();
            }
        }
    });
    QWidget::focusOutEvent(event);
}

void LSPHoverWidget::startHideTimer()
{
    if (!m_hideTimer->isActive()) {
        m_hideTimer->start();
    }
}

void LSPHoverWidget::stopHideTimer()
{
    if (m_hideTimer->isActive()) {
        m_hideTimer->stop();
    }
}

} // namespace papiro