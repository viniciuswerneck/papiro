#pragma once

#include <QWidget>
#include <QIcon>
#include <QMenuBar>

class QLabel;
class QToolButton;
class QHBoxLayout;
class QMenu;

namespace papiro {

/*! Custom Fluent-style title bar with window controls.
    Provides Mica backdrop on Windows 11, fallback on other platforms. */
class FluentTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit FluentTitleBar(QWidget *parent = nullptr);
    ~FluentTitleBar() override = default;

    void setWindowTitle(const QString &title);
    void setWindowIcon(const QIcon &icon);
    void setMenuBar(QMenuBar *menuBar);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

public slots:
    void updateMaximizeButton();
    void hideWindowControls();

private:
    void setupUI();
    void setupWindowControls();
    void enableMicaBackdrop();

    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QWidget *m_menuBarContainer = nullptr;
    QToolButton *m_minimizeButton = nullptr;
    QToolButton *m_maximizeButton = nullptr;
    QToolButton *m_closeButton = nullptr;

    QPoint m_dragStartPos;
    bool m_isDragging = false;
    bool m_isMaximized = false;

#ifdef Q_OS_WIN
    bool m_micaEnabled = false;
#endif
};

} // namespace papiro