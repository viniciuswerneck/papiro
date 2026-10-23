#pragma once

#include "core/GitTypes.h"

#include <QWidget>
#include <QVector>
#include <QScrollBar>
#include <QTimer>

namespace papiro {

class Editor;

/*! Inline git blame overlay widget showing commit info per line. */
class GitBlameView : public QWidget {
    Q_OBJECT

public:
    explicit GitBlameView(Editor* editor, QWidget* parent = nullptr);
    ~GitBlameView() override;

    void setBlame(const QVector<GitBlameLine>& blameLines);
    void clearBlame();
    void setVisible(bool visible);
    bool isBlameVisible() const { return m_visible; }

    // Positioning: column to the left of the editor (gutter)
    void updateGeometry();

    // Scroll sync
    void setScrollValue(int value);
    int scrollValue() const;

signals:
    void commitClicked(const QString& commitHash);
    void authorClicked(const QString& authorEmail);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct BlameLineInfo {
        GitBlameLine blame;
        QRect rect;           // Full line rect in blame view coordinates
        QRect hashRect;       // Commit hash clickable area
        QRect authorRect;     // Author clickable area
        QRect timeRect;       // Time clickable area
    };

    void calculateLineRects();
    QString formatRelativeTime(const QDateTime& dateTime) const;
    QString formatTooltip(const GitBlameLine& blame) const;
    int lineAtPosition(const QPoint& pos) const;
    BlameLineInfo* hitTest(const QPoint& pos);

    Editor* m_editor = nullptr;
    QVector<GitBlameLine> m_blameLines;
    QVector<BlameLineInfo> m_lineInfos;
    bool m_visible = false;
    int m_hoveredLine = -1;
    BlameLineInfo* m_hoveredPart = nullptr; // nullptr = none, or points to hash/author/time rect
    QTimer m_tooltipTimer;
    QPoint m_lastMousePos;
};

} // namespace papiro