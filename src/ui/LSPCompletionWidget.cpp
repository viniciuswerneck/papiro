#include "ui/LSPCompletionWidget.h"

#include "editor/Editor.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace papiro {

namespace {

// Map LSP CompletionItemKind to icons
const QString ICON_METHOD = QStringLiteral("🟦");
const QString ICON_FUNCTION = QStringLiteral("🟦");
const QString ICON_CLASS = QStringLiteral("🟨");
const QString ICON_INTERFACE = QStringLiteral("🟨");
const QString ICON_VARIABLE = QStringLiteral("🟩");
const QString ICON_PROPERTY = QStringLiteral("🟩");
const QString ICON_KEYWORD = QStringLiteral("🟪");
const QString ICON_SNIPPET = QStringLiteral("🟧");
const QString ICON_TEXT = QStringLiteral("⬜");
const QString ICON_FIELD = QStringLiteral("🟩");
const QString ICON_CONSTRUCTOR = QStringLiteral("🟦");
const QString ICON_MODULE = QStringLiteral("🟦");
const QString ICON_ENUM = QStringLiteral("🟨");
const QString ICON_UNIT = QStringLiteral("⬜");
const QString ICON_VALUE = QStringLiteral("⬜");
const QString ICON_ENUM_MEMBER = QStringLiteral("🟨");
const QString ICON_CONSTANT = QStringLiteral("🟨");
const QString ICON_STRUCT = QStringLiteral("🟨");
const QString ICON_EVENT = QStringLiteral("🟪");
const QString ICON_OPERATOR = QStringLiteral("🟪");
const QString ICON_TYPE_PARAMETER = QStringLiteral("🟪");
const QString ICON_COLOR = QStringLiteral("🟧");
const QString ICON_FILE = QStringLiteral("🟧");
const QString ICON_REFERENCE = QStringLiteral("🟧");
const QString ICON_FOLDER = QStringLiteral("🟧");

} // namespace

LSPCompletionWidget::LSPCompletionWidget(Editor* editor, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_editor(editor)
{
    setupUi();
}

LSPCompletionWidget::~LSPCompletionWidget() = default;

void LSPCompletionWidget::setupUi()
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setUniformItemSizes(true);
    m_listWidget->setMouseTracking(true);
    m_listWidget->setFocusPolicy(Qt::NoFocus);
    m_listWidget->installEventFilter(this);

    // Style the list widget to match theme
    m_listWidget->setStyleSheet(QStringLiteral(R"(
        QListWidget {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 4px;
            outline: none;
        }
        QListWidget::item {
            padding: 4px 8px;
            border: none;
        }
        QListWidget::item:selected {
            background-color: %4;
            color: %5;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: %6;
            border-radius: 3px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: %7;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )")
        .arg(Theme::colors().completionBackground.name())
        .arg(Theme::colors().completionText.name())
        .arg(Theme::colors().completionBorder.name())
        .arg(Theme::colors().completionSelectionBackground.name())
        .arg(Theme::colors().completionSelectionText.name())
        .arg(Theme::colors().scrollbarHandle.name())
        .arg(Theme::colors().scrollbarHandleHover.name()));

    layout->addWidget(m_listWidget);

    // Connect item activation
    connect(m_listWidget, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        Q_UNUSED(item);
        acceptCompletion();
    });

    connect(m_listWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        Q_UNUSED(item);
        acceptCompletion();
    });
}

void LSPCompletionWidget::showCompletions(const QVector<LSP::CompletionItem>& items, const QPoint& position)
{
    m_allItems = items;
    m_filterPrefix.clear();
    m_filterActive = false;
    m_anchorPosition = position;

    populateList(m_allItems);
    updateGeometry(position);

    // Show and ensure first item is selected
    show();
    raise();
    activateWindow();

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
        m_listWidget->scrollToItem(m_listWidget->item(0));
    }
}

void LSPCompletionWidget::hideCompletions()
{
    if (isVisible()) {
        hide();
        m_allItems.clear();
        m_filterPrefix.clear();
        m_filterActive = false;
    }
}

bool LSPCompletionWidget::isVisible() const
{
    return QWidget::isVisible();
}

void LSPCompletionWidget::populateList(const QVector<LSP::CompletionItem>& items)
{
    m_listWidget->clear();

    for (const auto& item : items) {
        auto* listItem = new QListWidgetItem();
        listItem->setData(Qt::UserRole, QVariant::fromValue(item));

        // Build display text: icon + label + detail
        QString displayText = kindIcon(item.kind.value_or(LSP::CompletionItemKind::Text)) + QStringLiteral("  ") + item.label;

        if (item.detail.has_value() && !item.detail.value().isEmpty()) {
            displayText += QStringLiteral("  ") + item.detail.value();
        }

        listItem->setText(displayText);

        // Set tooltip with documentation
        if (item.documentation.has_value() && !item.documentation.value().isEmpty()) {
            listItem->setToolTip(item.documentation.value());
        }

        // Apply deprecation styling
        if (item.deprecated.has_value() && item.deprecated.value()) {
            QFont font = listItem->font();
            font.setStrikeOut(true);
            listItem->setFont(font);
            listItem->setForeground(QBrush(Theme::colors().completionDeprecatedText));
        }

        m_listWidget->addItem(listItem);
    }

    // Apply initial filter if any
    if (m_filterActive) {
        applyFilter();
    }
}

void LSPCompletionWidget::applyFilter()
{
    if (!m_filterActive || m_filterPrefix.isEmpty()) {
        // Show all items
        for (int i = 0; i < m_listWidget->count(); ++i) {
            m_listWidget->item(i)->setHidden(false);
        }
        return;
    }

    const QString prefix = m_filterPrefix.toLower();
    int visibleCount = 0;
    QListWidgetItem* firstVisible = nullptr;

    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* item = m_listWidget->item(i);
        const auto completionItem = item->data(Qt::UserRole).value<LSP::CompletionItem>();

        // Use filterText if available, otherwise label
        QString matchText = completionItem.filterText.value_or(completionItem.label).toLower();

        if (matchText.startsWith(prefix)) {
            item->setHidden(false);
            if (!firstVisible) {
                firstVisible = item;
            }
            ++visibleCount;

            // Highlight the matching prefix in the label
            highlightPrefix(item, m_filterPrefix);
        } else {
            item->setHidden(true);
        }
    }

    // Select first visible item
    if (firstVisible) {
        m_listWidget->setCurrentItem(firstVisible);
        m_listWidget->scrollToItem(firstVisible);
    }
}

void LSPCompletionWidget::highlightPrefix(QListWidgetItem* item, const QString& prefix) const
{
    if (prefix.isEmpty())
        return;

    const auto completionItem = item->data(Qt::UserRole).value<LSP::CompletionItem>();
    QString label = completionItem.label;

    // Find the prefix position in the label (case-insensitive)
    int index = label.toLower().indexOf(prefix.toLower());
    if (index < 0)
        return;

    // Create rich text with highlighted prefix
    QString before = label.left(index);
    QString matched = label.mid(index, prefix.length());
    QString after = label.mid(index + prefix.length());

    QString detail = completionItem.detail.value_or(QString());
    if (!detail.isEmpty())
        detail = QStringLiteral("  ") + detail;

    QString kindIconStr = kindIcon(completionItem.kind.value_or(LSP::CompletionItemKind::Text));

    QString html = QStringLiteral("<span style='color:%1;'>%2</span> %3<span style='background-color:%4; color:%5;'>%6</span>%7%8")
        .arg(Theme::colors().completionIcon.name())
        .arg(kindIconStr)
        .arg(before)
        .arg(Theme::colors().completionMatchHighlight.name())
        .arg(Theme::colors().completionMatchText.name())
        .arg(matched)
        .arg(after)
        .arg(detail);

    item->setText(html);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void LSPCompletionWidget::updateGeometry(const QPoint& position)
{
    if (!m_editor)
        return;

    // Calculate widget size based on content
    int itemHeight = m_listWidget->sizeHintForRow(0);
    if (itemHeight <= 0)
        itemHeight = 24;

    int maxVisibleItems = 10;
    int contentHeight = qMin(m_listWidget->count(), maxVisibleItems) * itemHeight;
    contentHeight += m_listWidget->horizontalScrollBar()->isVisible() ? 
                     m_listWidget->horizontalScrollBar()->height() : 0;
    contentHeight += 2; // margins

    // Calculate width based on longest item
    int maxWidth = 0;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        int w = m_listWidget->sizeHintForColumn(0);
        if (w > maxWidth)
            maxWidth = w;
    }
    maxWidth = qBound(200, maxWidth + 20, 600); // min 200, max 600

    // Position below cursor
    QPoint globalPos = m_editor->viewport()->mapToGlobal(position);
    
    // Adjust if would go off screen
    QRect screenGeom = QApplication::primaryScreen()->availableGeometry();
    if (globalPos.y() + contentHeight > screenGeom.bottom()) {
        // Show above cursor instead
        QRect cursorRect = m_editor->cursorRect();
        QPoint abovePos = m_editor->viewport()->mapToGlobal(QPoint(cursorRect.left(), cursorRect.top() - contentHeight));
        globalPos = abovePos;
    }
    if (globalPos.x() + maxWidth > screenGeom.right()) {
        globalPos.setX(screenGeom.right() - maxWidth);
    }
    if (globalPos.x() < screenGeom.left()) {
        globalPos.setX(screenGeom.left());
    }

    setFixedSize(maxWidth, contentHeight);
    move(globalPos);
}

QString LSPCompletionWidget::kindIcon(LSP::CompletionItemKind kind) const
{
    switch (kind) {
        case LSP::CompletionItemKind::Method:
        case LSP::CompletionItemKind::Function:
        case LSP::CompletionItemKind::Constructor:
            return ICON_METHOD;
        case LSP::CompletionItemKind::Class:
        case LSP::CompletionItemKind::Interface:
        case LSP::CompletionItemKind::Struct:
        case LSP::CompletionItemKind::Enum:
        case LSP::CompletionItemKind::Module:
            return ICON_CLASS;
        case LSP::CompletionItemKind::Variable:
        case LSP::CompletionItemKind::Property:
        case LSP::CompletionItemKind::Field:
        case LSP::CompletionItemKind::Constant:
            return ICON_VARIABLE;
        case LSP::CompletionItemKind::Keyword:
        case LSP::CompletionItemKind::Operator:
        case LSP::CompletionItemKind::Event:
        case LSP::CompletionItemKind::TypeParameter:
            return ICON_KEYWORD;
        case LSP::CompletionItemKind::Snippet:
            return ICON_SNIPPET;
        case LSP::CompletionItemKind::Text:
        case LSP::CompletionItemKind::Unit:
        case LSP::CompletionItemKind::Value:
        case LSP::CompletionItemKind::EnumMember:
            return ICON_TEXT;
        case LSP::CompletionItemKind::Color:
        case LSP::CompletionItemKind::File:
        case LSP::CompletionItemKind::Reference:
        case LSP::CompletionItemKind::Folder:
            return ICON_FILE;
        default:
            return ICON_TEXT;
    }
}

QString LSPCompletionWidget::kindName(LSP::CompletionItemKind kind) const
{
    switch (kind) {
        case LSP::CompletionItemKind::Text: return QStringLiteral("Text");
        case LSP::CompletionItemKind::Method: return QStringLiteral("Method");
        case LSP::CompletionItemKind::Function: return QStringLiteral("Function");
        case LSP::CompletionItemKind::Constructor: return QStringLiteral("Constructor");
        case LSP::CompletionItemKind::Field: return QStringLiteral("Field");
        case LSP::CompletionItemKind::Variable: return QStringLiteral("Variable");
        case LSP::CompletionItemKind::Class: return QStringLiteral("Class");
        case LSP::CompletionItemKind::Interface: return QStringLiteral("Interface");
        case LSP::CompletionItemKind::Module: return QStringLiteral("Module");
        case LSP::CompletionItemKind::Property: return QStringLiteral("Property");
        case LSP::CompletionItemKind::Unit: return QStringLiteral("Unit");
        case LSP::CompletionItemKind::Value: return QStringLiteral("Value");
        case LSP::CompletionItemKind::Enum: return QStringLiteral("Enum");
        case LSP::CompletionItemKind::Keyword: return QStringLiteral("Keyword");
        case LSP::CompletionItemKind::Snippet: return QStringLiteral("Snippet");
        case LSP::CompletionItemKind::Color: return QStringLiteral("Color");
        case LSP::CompletionItemKind::File: return QStringLiteral("File");
        case LSP::CompletionItemKind::Reference: return QStringLiteral("Reference");
        case LSP::CompletionItemKind::Folder: return QStringLiteral("Folder");
        case LSP::CompletionItemKind::EnumMember: return QStringLiteral("EnumMember");
        case LSP::CompletionItemKind::Constant: return QStringLiteral("Constant");
        case LSP::CompletionItemKind::Struct: return QStringLiteral("Struct");
        case LSP::CompletionItemKind::Event: return QStringLiteral("Event");
        case LSP::CompletionItemKind::Operator: return QStringLiteral("Operator");
        case LSP::CompletionItemKind::TypeParameter: return QStringLiteral("TypeParameter");
        default: return QStringLiteral("Unknown");
    }
}

LSP::CompletionItem LSPCompletionWidget::currentItem() const
{
    auto* item = m_listWidget->currentItem();
    if (!item)
        return LSP::CompletionItem{};
    return item->data(Qt::UserRole).value<LSP::CompletionItem>();
}

// Navigation
void LSPCompletionWidget::selectNext()
{
    int count = m_listWidget->count();
    if (count == 0)
        return;

    int currentRow = m_listWidget->currentRow();
    int nextRow = currentRow + 1;

    // Find next visible item
    while (nextRow < count && m_listWidget->item(nextRow)->isHidden()) {
        ++nextRow;
    }

    if (nextRow >= count) {
        // Wrap to first visible
        nextRow = 0;
        while (nextRow < count && m_listWidget->item(nextRow)->isHidden()) {
            ++nextRow;
        }
    }

    if (nextRow < count) {
        m_listWidget->setCurrentRow(nextRow);
        m_listWidget->scrollToItem(m_listWidget->item(nextRow));
    }
}

void LSPCompletionWidget::selectPrevious()
{
    int count = m_listWidget->count();
    if (count == 0)
        return;

    int currentRow = m_listWidget->currentRow();
    int prevRow = currentRow - 1;

    // Find previous visible item
    while (prevRow >= 0 && m_listWidget->item(prevRow)->isHidden()) {
        --prevRow;
    }

    if (prevRow < 0) {
        // Wrap to last visible
        prevRow = count - 1;
        while (prevRow >= 0 && m_listWidget->item(prevRow)->isHidden()) {
            --prevRow;
        }
    }

    if (prevRow >= 0) {
        m_listWidget->setCurrentRow(prevRow);
        m_listWidget->scrollToItem(m_listWidget->item(prevRow));
    }
}

void LSPCompletionWidget::selectPageUp()
{
    int count = m_listWidget->count();
    if (count == 0)
        return;

    int currentRow = m_listWidget->currentRow();
    int pageSize = m_listWidget->height() / m_listWidget->sizeHintForRow(0);
    if (pageSize <= 0)
        pageSize = 5;

    int targetRow = qMax(0, currentRow - pageSize);

    // Find previous visible item from target
    while (targetRow >= 0 && m_listWidget->item(targetRow)->isHidden()) {
        --targetRow;
    }

    if (targetRow < 0)
        targetRow = 0;

    // Find first visible from target
    while (targetRow < count && m_listWidget->item(targetRow)->isHidden()) {
        ++targetRow;
    }

    if (targetRow < count) {
        m_listWidget->setCurrentRow(targetRow);
        m_listWidget->scrollToItem(m_listWidget->item(targetRow));
    }
}

void LSPCompletionWidget::selectPageDown()
{
    int count = m_listWidget->count();
    if (count == 0)
        return;

    int currentRow = m_listWidget->currentRow();
    int pageSize = m_listWidget->height() / m_listWidget->sizeHintForRow(0);
    if (pageSize <= 0)
        pageSize = 5;

    int targetRow = qMin(count - 1, currentRow + pageSize);

    // Find next visible item from target
    while (targetRow < count && m_listWidget->item(targetRow)->isHidden()) {
        ++targetRow;
    }

    if (targetRow >= count) {
        targetRow = count - 1;
        while (targetRow >= 0 && m_listWidget->item(targetRow)->isHidden()) {
            --targetRow;
        }
    }

    if (targetRow >= 0) {
        m_listWidget->setCurrentRow(targetRow);
        m_listWidget->scrollToItem(m_listWidget->item(targetRow));
    }
}

void LSPCompletionWidget::acceptCompletion()
{
    auto item = currentItem();
    if (!item.label.isEmpty()) {
        hideCompletions();
        emit completionAccepted(item);
    }
}

void LSPCompletionWidget::cancelCompletion()
{
    hideCompletions();
    emit completionCancelled();
}

// Filter
void LSPCompletionWidget::setFilter(const QString& prefix)
{
    m_filterPrefix = prefix;
    m_filterActive = !prefix.isEmpty();
    applyFilter();
}

// Event handling
bool LSPCompletionWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_listWidget) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            switch (keyEvent->key()) {
                case Qt::Key_Down:
                    if (keyEvent->modifiers() == Qt::NoModifier) {
                        selectNext();
                        return true;
                    }
                    break;
                case Qt::Key_Up:
                    if (keyEvent->modifiers() == Qt::NoModifier) {
                        selectPrevious();
                        return true;
                    }
                    break;
                case Qt::Key_PageUp:
                    if (keyEvent->modifiers() == Qt::NoModifier) {
                        selectPageUp();
                        return true;
                    }
                    break;
                case Qt::Key_PageDown:
                    if (keyEvent->modifiers() == Qt::NoModifier) {
                        selectPageDown();
                        return true;
                    }
                    break;
                case Qt::Key_Return:
                case Qt::Key_Enter:
                case Qt::Key_Tab:
                    if (keyEvent->modifiers() == Qt::NoModifier) {
                        acceptCompletion();
                        return true;
                    }
                    break;
                case Qt::Key_Escape:
                    if (keyEvent->modifiers() == Qt::NoModifier) {
                        cancelCompletion();
                        return true;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LSPCompletionWidget::focusOutEvent(QFocusEvent* event)
{
    // Don't hide immediately - give time for click on item
    QTimer::singleShot(100, this, [this]() {
        if (isVisible() && !m_listWidget->hasFocus() && !hasFocus()) {
            // Check if mouse is over the widget
            QPoint globalPos = QCursor::pos();
            if (!rect().contains(mapFromGlobal(globalPos)) &&
                !m_listWidget->rect().contains(m_listWidget->mapFromGlobal(globalPos))) {
                cancelCompletion();
            }
        }
    });
    QWidget::focusOutEvent(event);
}

} // namespace papiro