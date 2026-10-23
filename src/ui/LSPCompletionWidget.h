#pragma once

#include "core/LSPTypes.h"

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QString>

class QListWidget;
class QListWidgetItem;

namespace papiro {

class Editor;

class LSPCompletionWidget : public QWidget {
    Q_OBJECT

public:
    explicit LSPCompletionWidget(Editor* editor, QWidget* parent = nullptr);
    ~LSPCompletionWidget() override;

    void showCompletions(const QVector<LSP::CompletionItem>& items, const QPoint& position);
    void hideCompletions();
    bool isVisible() const;

    // Navigation
    void selectNext();
    void selectPrevious();
    void selectPageUp();
    void selectPageDown();
    void acceptCompletion();
    void cancelCompletion();

    // Filter
    void setFilter(const QString& prefix);

signals:
    void completionAccepted(const LSP::CompletionItem& item);
    void completionCancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void setupUi();
    void populateList(const QVector<LSP::CompletionItem>& items);
    void applyFilter();
    void updateGeometry(const QPoint& position);
    QString kindIcon(LSP::CompletionItemKind kind) const;
    QString kindName(LSP::CompletionItemKind kind) const;
    void highlightPrefix(QListWidgetItem* item, const QString& prefix) const;
    LSP::CompletionItem currentItem() const;

    Editor* m_editor = nullptr;
    QListWidget* m_listWidget = nullptr;
    QVector<LSP::CompletionItem> m_allItems;
    QString m_filterPrefix;
    bool m_filterActive = false;
    QPoint m_anchorPosition;
};

} // namespace papiro