#include "ui/FluentTitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>
#include <QWindow>
#include <QMenuBar>
#include <QWidgetAction>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace papiro {

FluentTitleBar::FluentTitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("FluentTitleBar"));
    setFixedHeight(40);
    setAttribute(Qt::WA_StyledBackground, true);

    setupUI();
    setupWindowControls();

#ifdef Q_OS_WIN
    enableMicaBackdrop();
#endif
}

void FluentTitleBar::setupUI()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 8, 0);
    layout->setSpacing(8);

    // Window icon
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(16, 16);
    m_iconLabel->setScaledContents(true);
    layout->addWidget(m_iconLabel);

    // Window title
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("TitleLabel"));
    m_titleLabel->setFont(QFont("Segoe UI Variable", 12, QFont::Medium));
    m_titleLabel->setStyleSheet("color: #1B1B1B;");
    layout->addWidget(m_titleLabel);

    // Menu bar container (stretch)
    m_menuBarContainer = new QWidget(this);
    m_menuBarContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_menuBarContainer, 1);

    // Window control buttons
    auto *controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);

    // Minimize
    m_minimizeButton = new QToolButton(this);
    m_minimizeButton->setObjectName(QStringLiteral("MinimizeButton"));
    m_minimizeButton->setFixedSize(46, 40);
    m_minimizeButton->setToolTip(tr("Minimize"));
    m_minimizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
    m_minimizeButton->setIconSize(QSize(10, 10));
    connect(m_minimizeButton, &QToolButton::clicked, this, &FluentTitleBar::minimizeRequested);
    controlsLayout->addWidget(m_minimizeButton);

    // Maximize/Restore
    m_maximizeButton = new QToolButton(this);
    m_maximizeButton->setObjectName(QStringLiteral("MaximizeButton"));
    m_maximizeButton->setFixedSize(46, 40);
    m_maximizeButton->setToolTip(tr("Maximize"));
    m_maximizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_maximizeButton->setIconSize(QSize(10, 10));
    connect(m_maximizeButton, &QToolButton::clicked, this, &FluentTitleBar::maximizeRequested);
    controlsLayout->addWidget(m_maximizeButton);

    // Close
    m_closeButton = new QToolButton(this);
    m_closeButton->setObjectName(QStringLiteral("CloseButton"));
    m_closeButton->setFixedSize(46, 40);
    m_closeButton->setToolTip(tr("Close"));
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    m_closeButton->setIconSize(QSize(10, 10));
    connect(m_closeButton, &QToolButton::clicked, this, &FluentTitleBar::closeRequested);
    controlsLayout->addWidget(m_closeButton);

    layout->addLayout(controlsLayout);
}

void FluentTitleBar::setupWindowControls()
{
    // Style the buttons with Fluent design
    QString buttonStyle = R"(
        QToolButton#MinimizeButton,
        QToolButton#MaximizeButton {
            background-color: transparent;
            border: none;
            border-radius: 0;
            padding: 0;
        }
        QToolButton#MinimizeButton:hover,
        QToolButton#MaximizeButton:hover {
            background-color: #E8E8E8;
        }
        QToolButton#MinimizeButton:pressed,
        QToolButton#MaximizeButton:pressed {
            background-color: #DCDCDC;
        }
        QToolButton#CloseButton {
            background-color: transparent;
            border: none;
            border-radius: 0;
            padding: 0;
        }
        QToolButton#CloseButton:hover {
            background-color: #E81123;
            color: white;
        }
        QToolButton#CloseButton:pressed {
            background-color: #C4001A;
            color: white;
        }
    )";

    m_minimizeButton->setStyleSheet(buttonStyle);
    m_maximizeButton->setStyleSheet(buttonStyle);
    m_closeButton->setStyleSheet(buttonStyle);
}

void FluentTitleBar::setWindowTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void FluentTitleBar::setWindowIcon(const QIcon &icon)
{
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(16, 16));
        m_iconLabel->setVisible(true);
    } else {
        m_iconLabel->setVisible(false);
    }
}

void FluentTitleBar::setMenuBar(QMenuBar *menuBar)
{
    if (!menuBar) return;

    // Clear existing layout
    QLayout *oldLayout = m_menuBarContainer->layout();
    if (oldLayout) {
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }

    auto *layout = new QHBoxLayout(m_menuBarContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(menuBar);
    menuBar->setParent(m_menuBarContainer);
    menuBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void FluentTitleBar::updateMaximizeButton()
{
    QWidget *window = this->window();
    if (!window) return;

    m_isMaximized = window->isMaximized();
    if (m_isMaximized) {
        m_maximizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
        m_maximizeButton->setToolTip(tr("Restore"));
    } else {
        m_maximizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        m_maximizeButton->setToolTip(tr("Maximize"));
    }
}

void FluentTitleBar::hideWindowControls()
{
    if (m_minimizeButton) m_minimizeButton->hide();
    if (m_maximizeButton) m_maximizeButton->hide();
    if (m_closeButton) m_closeButton->hide();
}

void FluentTitleBar::enableMicaBackdrop()
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(window()->winId());
    if (!hwnd) return;

    // Check if Mica is available (Windows 11 build 22000+)
    OSVERSIONINFOEXW osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = 10;
    osvi.dwBuildNumber = 22000;

    DWORDLONG conditionMask = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    conditionMask = VerSetConditionMask(conditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    if (VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_BUILDNUMBER, conditionMask)) {
        // Enable Mica backdrop
        DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_MICA;
        HRESULT hr = DwmSetWindowAttribute(
            hwnd,
            DWMWA_SYSTEMBACKDROP_TYPE,
            &backdropType,
            sizeof(backdropType)
        );

        if (SUCCEEDED(hr)) {
            m_micaEnabled = true;
            // Make titlebar transparent for Mica
            setAttribute(Qt::WA_TranslucentBackground);
            setStyleSheet("background: transparent;");
        }
    }

    // Also extend frame into client area for custom titlebar
    if (m_micaEnabled) {
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
#endif
}

bool FluentTitleBar::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    MSG *msg = static_cast<MSG *>(message);

    switch (msg->message) {
        case WM_NCCALCSIZE: {
            // Remove standard frame
            if (msg->wParam) {
                NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                params->rgrc[0].top += 0; // Keep our titlebar height
                *result = WVR_REDRAW;
                return true;
            }
            break;
        }
        case WM_NCHITTEST: {
            // Handle hit testing for custom titlebar
            QPoint globalPos(msg->lParam & 0xFFFF, (msg->lParam >> 16) & 0xFFFF);
            QPoint localPos = mapFromGlobal(globalPos);

            // Check if click is on window controls
            if (m_closeButton->geometry().contains(localPos) ||
                m_maximizeButton->geometry().contains(localPos) ||
                m_minimizeButton->geometry().contains(localPos)) {
                *result = HTCLIENT; // Let buttons handle their own clicks
                return true;
            }

            // Title bar area for dragging
            if (localPos.y() >= 0 && localPos.y() <= height() &&
                localPos.x() >= 0 && localPos.x() <= width()) {
                *result = HTCAPTION;
                return true;
            }
            break;
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void FluentTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *window = this->window();
        if (window && !window->isMaximized()) {
            m_dragStartPos = event->globalPosition().toPoint() - window->frameGeometry().topLeft();
            m_isDragging = true;
        }
    }
    QWidget::mousePressEvent(event);
}

void FluentTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        QWidget *window = this->window();
        if (window && !window->isMaximized()) {
            window->move(event->globalPosition().toPoint() - m_dragStartPos);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void FluentTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void FluentTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *window = this->window();
        if (window) {
            if (window->isMaximized()) {
                window->showNormal();
            } else {
                window->showMaximized();
            }
            updateMaximizeButton();
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace papiro