#include "GitBlameView.h"

#include "editor/Editor.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QToolTip>
#include <QFontMetrics>
#include <QTextBlock>
#include <QPlainTextEdit>

namespace papiro {

GitBlameView::GitBlameView(Editor* editor, QWidget* parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);

    // Tooltip timer for hover details
    m_tooltipTimer.setSingleShot(true);
    m_tooltipTimer.setInterval(500);
    connect(&m_tooltipTimer, &QTimer::timeout, this, [this]() {
        if (m_hoveredLine >= 0 && m_hoveredLine < m_lineInfos.size() && m_hoveredPart) {
            const GitBlameLine& blame = m_lineInfos[m_hoveredLine].blame;
            QToolTip::showText(m_lastMousePos, formatTooltip(blame), this);
        }
    });

    // Connect to editor's vertical scroll bar for sync
    if (m_editor) {
        QScrollBar* editorScroll = m_editor->verticalScrollBar();
        connect(editorScroll, &QScrollBar::valueChanged, this, [this](int value) {
            update(); // Repaint to sync with editor scroll
        });
    }
}

GitBlameView::~GitBlameView() = default;

void GitBlameView::setBlame(const QVector<GitBlameLine>& blameLines)
{
    m_blameLines = blameLines;
    calculateLineRects();
    update();
    updateGeometry();
}

void GitBlameView::clearBlame()
{
    m_blameLines.clear();
    m_lineInfos.clear();
    m_hoveredLine = -1;
    m_hoveredPart = nullptr;
    update();
}

void GitBlameView::setBlameVisible(bool visible)
{
    if (m_blameVisible == visible)
        return;
    m_blameVisible = visible;
    QWidget::setVisible(visible);
    if (visible) {
        calculateLineRects();
    }
    update();
}

void GitBlameView::updateGeometry()
{
    if (!m_editor || !m_blameVisible)
        return;

    calculateLineRects();
}

void GitBlameView::setScrollValue(int value)
{
    // Not used - we sync via repaint on editor scroll
    Q_UNUSED(value);
}

int GitBlameView::scrollValue() const
{
    // Not used
    return 0;
}

void GitBlameView::calculateLineRects()
{
    m_lineInfos.clear();
    if (!m_editor || m_blameLines.isEmpty())
        return;

    const QFontMetrics fm(font());
    const int contentY = m_editor->contentOffset().y();

    for (int i = 0; i < m_blameLines.size(); ++i) {
        const GitBlameLine& blame = m_blameLines[i];
        BlameLineInfo info;
        info.blame = blame;

        // Calculate line position in editor coordinates
        QTextBlock block = m_editor->document()->findBlockByNumber(blame.lineNumber - 1);
        if (!block.isValid())
            continue;

        int top = qRound(m_editor->blockBoundingGeometry(block).translated(m_editor->contentOffset()).top());
        int bottom = top + qRound(m_editor->blockBoundingRect(block).height());

        // Convert to blame view coordinates
        top -= contentY;
        bottom -= contentY;

        info.rect = QRect(0, top, width(), bottom - top);

        // Layout: [hash] [author] [time]
        int x = 4;
        const int hashWidth = fm.horizontalAdvance("0000000 ") + 4;
        const int authorWidth = fm.horizontalAdvance("Author Name ") + 4;
        // Time takes remaining space

        info.hashRect = QRect(x, top, hashWidth, bottom - top);
        x += hashWidth;
        info.authorRect = QRect(x, top, authorWidth, bottom - top);
        x += authorWidth;
        info.timeRect = QRect(x, top, width() - x - 4, bottom - top);

        m_lineInfos.append(info);
    }
}

QString GitBlameView::formatRelativeTime(const QDateTime& dateTime) const
{
    if (!dateTime.isValid())
        return QString();

    qint64 secs = dateTime.secsTo(QDateTime::currentDateTimeUtc());
    if (secs < 0) secs = 0;

    if (secs < 60)
        return tr("just now");
    if (secs < 3600)
        return tr("%1m ago").arg(secs / 60);
    if (secs < 86400)
        return tr("%1h ago").arg(secs / 3600);
    if (secs < 604800)
        return tr("%1d ago").arg(secs / 86400);
    if (secs < 2592000)
        return tr("%1w ago").arg(secs / 604800);
    if (secs < 31536000)
        return tr("%1mo ago").arg(secs / 2592000);
    return tr("%1y ago").arg(secs / 31536000);
}

QString GitBlameView::formatTooltip(const GitBlameLine& blame) const
{
    QString tooltip;
    tooltip += QString("<b>Commit:</b> %1<br>").arg(blame.commitHash);
    tooltip += QString("<b>Subject:</b> %1<br>").arg(blame.summary.toHtmlEscaped());
    tooltip += QString("<b>Author:</b> %1 <%2><br>").arg(blame.authorName.toHtmlEscaped()).arg(blame.authorEmail.toHtmlEscaped());
    tooltip += QString("<b>Date:</b> %1<br>").arg(blame.authorDate.toString("yyyy-MM-dd hh:mm:ss"));
    if (blame.isBoundary)
        tooltip += "<b>Boundary commit</b><br>";
    return tooltip;
}

int GitBlameView::lineAtPosition(const QPoint& pos) const
{
    for (int i = 0; i < m_lineInfos.size(); ++i) {
        if (m_lineInfos[i].rect.contains(pos))
            return i;
    }
    return -1;
}

GitBlameView::BlameLineInfo* GitBlameView::hitTest(const QPoint& pos)
{
    int lineIdx = lineAtPosition(pos);
    if (lineIdx < 0)
        return nullptr;

    BlameLineInfo& info = m_lineInfos[lineIdx];
    if (info.hashRect.contains(pos)) {
        m_hoveredPart = &info;
        return &info;
    }
    if (info.authorRect.contains(pos)) {
        m_hoveredPart = &info;
        return &info;
    }
    if (info.timeRect.contains(pos)) {
        m_hoveredPart = &info;
        return &info;
    }
    m_hoveredPart = nullptr;
    return &info;
}

void GitBlameView::paintEvent(QPaintEvent* event)
{
    if (!m_blameVisible || m_lineInfos.isEmpty()) {
        QWidget::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.fillRect(event->rect(), Theme::colors().gutterBackground);

    const QFontMetrics fm(font());
    const QColor hashColor = Theme::colors().gutterText;      // Gray
    const QColor authorColor = QColor("#4da6ff");             // Blue
    const QColor timeColor = QColor("#888888");               // Light gray

    // Get editor scroll position to sync painting
    int editorScrollY = m_editor ? m_editor->verticalScrollBar()->value() : 0;

    for (const BlameLineInfo& info : m_lineInfos) {
        if (!event->rect().intersects(info.rect))
            continue;

        const GitBlameLine& blame = info.blame;

        // Draw commit hash (short)
        if (!blame.commitShortHash.isEmpty()) {
            painter.setPen(info.hashRect.contains(m_lastMousePos) && m_hoveredPart ? authorColor : hashColor);
            painter.drawText(info.hashRect, Qt::AlignLeft | Qt::AlignVCenter,
                             blame.commitShortHash + " ");
        }

        // Draw author name
        if (!blame.authorName.isEmpty()) {
            painter.setPen(info.authorRect.contains(m_lastMousePos) && m_hoveredPart ? QColor("#6dbfff") : authorColor);
            painter.drawText(info.authorRect, Qt::AlignLeft | Qt::AlignVCenter,
                             blame.authorName + " ");
        }

        // Draw relative time
        QString timeStr = formatRelativeTime(blame.authorDate);
        if (!timeStr.isEmpty()) {
            painter.setPen(info.timeRect.contains(m_lastMousePos) && m_hoveredPart ? QColor("#aaaaaa") : timeColor);
            painter.drawText(info.timeRect, Qt::AlignLeft | Qt::AlignVCenter,
                             timeStr);
        }

        // Highlight hovered line
        if (&info == m_hoveredPart) {
            QColor highlight = Theme::colors().currentLine;
            highlight.setAlpha(60);
            painter.fillRect(info.rect, highlight);
        }
    }
}

void GitBlameView::mousePressEvent(QMouseEvent* event)
{
    if (!m_blameVisible || event->button() != Qt::LeftButton)
        return;

    BlameLineInfo* info = hitTest(event->pos());
    if (!info)
        return;

    const GitBlameLine& blame = info->blame;

    // Check which part was clicked
    if (info->hashRect.contains(event->pos()) && !blame.commitHash.isEmpty()) {
        emit commitClicked(blame.commitHash);
    } else if (info->authorRect.contains(event->pos()) && !blame.authorEmail.isEmpty()) {
        emit authorClicked(blame.authorEmail);
    }
}

void GitBlameView::mouseMoveEvent(QMouseEvent* event)
{
    m_lastMousePos = event->globalPosition().toPoint();
    int newHoveredLine = lineAtPosition(event->pos());

    if (newHoveredLine != m_hoveredLine) {
        m_hoveredLine = newHoveredLine;
        m_tooltipTimer.start();
        update();
    } else {
        // Check if hovering over different part
        BlameLineInfo* newPart = hitTest(event->pos());
        if (newPart != m_hoveredPart) {
            m_hoveredPart = newPart;
            m_tooltipTimer.start();
            update();
        }
    }
}

void GitBlameView::leaveEvent(QEvent* event)
{
    m_hoveredLine = -1;
    m_hoveredPart = nullptr;
    m_tooltipTimer.stop();
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(event);
}

void GitBlameView::resizeEvent(QResizeEvent* event)
{
    calculateLineRects();
    QWidget::resizeEvent(event);
}

} // namespace papiro