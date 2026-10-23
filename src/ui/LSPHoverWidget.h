#pragma once

#include "core/LSPTypes.h"

#include <QWidget>
#include <QPoint>
#include <QFont>

class QLabel;
class QTextDocument;
class QScrollArea;
class QTimer;

namespace papiro {

class Editor;

class LSPHoverWidget : public QWidget {
    Q_OBJECT

public:
    explicit LSPHoverWidget(Editor* editor, QWidget* parent = nullptr);
    ~LSPHoverWidget() override;

    void showHover(const LSP::Hover& hover, const QPoint& globalPos);
    void hideHover();
    bool isVisible() const;

    void setFont(const QFont& font);

signals:
    void linkActivated(const QString& link);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void setupUi();
    void updateContent(const LSP::Hover& hover);
    void updateGeometry(const QPoint& globalPos);
    QString hoverToHtml(const LSP::Hover& hover);
    QString markupContentToHtml(const LSP::MarkupContent& content);
    void startHideTimer();
    void stopHideTimer();

    Editor* m_editor = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_contentLabel = nullptr;
    QTextDocument* m_textDocument = nullptr;
    QTimer* m_hideTimer = nullptr;
    QTimer* m_hoverCheckTimer = nullptr;
    QFont m_font;
    bool m_mouseInside = false;
    static constexpr int MAX_WIDTH = 600;
    static constexpr int MAX_HEIGHT = 400;
    static constexpr int HIDE_DELAY_MS = 2000;
    static constexpr int HOVER_CHECK_INTERVAL_MS = 100;
};

} // namespace papiro