#include "ui/SplitView.h"

#include "editor/Editor.h"

#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>

namespace papiro {

SplitView::SplitView(Editor *primaryEditor, QWidget *parent)
    : QSplitter(parent)
    , m_primaryEditor(primaryEditor)
{
    setObjectName(QStringLiteral("SplitView"));
    setChildrenCollapsible(false);
    setHandleWidth(4);

    setupSecondaryEditor();

    // Add both editors to splitter
    addWidget(m_primaryEditor);
    addWidget(m_secondaryEditor);

    // Set initial sizes (50/50)
    setSizes({width() / 2, width() / 2});

    // Connect signals for synchronization
    connect(m_primaryEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &SplitView::onPrimaryScroll);
    connect(m_secondaryEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &SplitView::onSecondaryScroll);

    connect(m_primaryEditor, &Editor::cursorPositionChanged,
            this, &SplitView::onPrimaryCursorPositionChanged);
    connect(m_secondaryEditor, &Editor::cursorPositionChanged,
            this, &SplitView::onSecondaryCursorPositionChanged);

    connect(m_primaryEditor, &QPlainTextEdit::copyAvailable,
            this, [this](bool available) {
                if (!m_syncingSelection && available)
                    syncSelection(m_primaryEditor, m_secondaryEditor);
            });
    connect(m_secondaryEditor, &QPlainTextEdit::copyAvailable,
            this, [this](bool available) {
                if (!m_syncingSelection && available)
                    syncSelection(m_secondaryEditor, m_primaryEditor);
            });

    emit secondaryEditorCreated(m_secondaryEditor);
}

SplitView::~SplitView()
{
    // The editors are parented to the splitter, so they'll be deleted automatically
}

void SplitView::setupSecondaryEditor()
{
    // Create secondary editor with the same document
    m_secondaryEditor = new Editor(this);
    m_secondaryEditor->setDocument(m_primaryEditor->document());

    // Copy settings from primary editor
    m_secondaryEditor->setBaseFont(m_primaryEditor->font());
    m_secondaryEditor->setLineNumbersVisible(m_primaryEditor->lineNumbersVisible());
    m_secondaryEditor->setWhitespaceVisible(m_primaryEditor->whitespaceVisible());
    m_secondaryEditor->setCursorFreePositioning(m_primaryEditor->cursorFreePositioning());
    m_secondaryEditor->setLineWrapMode(m_primaryEditor->lineWrapMode());

    // Copy syntax highlighter
    // Note: SyntaxHighlighter is attached to the document, so it's shared automatically

    // Copy macro manager and spell checker
    // These are set via MainWindow when creating the split view
}

void SplitView::setSplitOrientation(Orientation orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    QSplitter::setOrientation(orientation == Orientation::Horizontal ? Qt::Horizontal : Qt::Vertical);
}

void SplitView::syncFromPrimary()
{
    // Called when primary editor changes - sync to secondary
    if (!m_syncingCursor) {
        m_syncingCursor = true;
        syncCursor(m_primaryEditor, m_secondaryEditor);
        m_syncingCursor = false;
    }
    if (!m_syncingSelection) {
        m_syncingSelection = true;
        syncSelection(m_primaryEditor, m_secondaryEditor);
        m_syncingSelection = false;
    }
}

void SplitView::syncFromSecondary()
{
    // Called when secondary editor changes - sync to primary
    if (!m_syncingCursor) {
        m_syncingCursor = true;
        syncCursor(m_secondaryEditor, m_primaryEditor);
        m_syncingCursor = false;
    }
    if (!m_syncingSelection) {
        m_syncingSelection = true;
        syncSelection(m_secondaryEditor, m_primaryEditor);
        m_syncingSelection = false;
    }
}

void SplitView::onPrimaryScroll(int value)
{
    if (m_syncingScroll)
        return;
    m_syncingScroll = true;
    syncScroll(m_primaryEditor->verticalScrollBar(), m_secondaryEditor->verticalScrollBar());
    m_syncingScroll = false;
}

void SplitView::onSecondaryScroll(int value)
{
    if (m_syncingScroll)
        return;
    m_syncingScroll = true;
    syncScroll(m_secondaryEditor->verticalScrollBar(), m_primaryEditor->verticalScrollBar());
    m_syncingScroll = false;
}

void SplitView::onPrimaryCursorPositionChanged()
{
    syncFromPrimary();
}

void SplitView::onSecondaryCursorPositionChanged()
{
    syncFromSecondary();
}

void SplitView::onPrimarySelectionChanged()
{
    // Handled via copyAvailable signal
}

void SplitView::onSecondarySelectionChanged()
{
    // Handled via copyAvailable signal
}

void SplitView::syncScroll(QScrollBar *from, QScrollBar *to)
{
    to->setValue(from->value());
}

void SplitView::syncCursor(Editor *from, Editor *to)
{
    QTextDocument *document = to->document();
    if (!document || from->document() != document)
        return;

    const QTextCursor source = from->textCursor();
    const int maxPosition = qMax(0, document->characterCount() - 1);
    const int position = qBound(0, source.position(), maxPosition);
    const int anchor = qBound(0, source.anchor(), maxPosition);

    QTextCursor cursor(document);
    cursor.setPosition(anchor);
    cursor.setPosition(position, QTextCursor::KeepAnchor);
    to->setTextCursor(cursor);
}

void SplitView::syncSelection(Editor *from, Editor *to)
{
    QTextDocument *document = to->document();
    if (!document || from->document() != document)
        return;

    const QTextCursor source = from->textCursor();
    if (!source.hasSelection())
        return;

    const int maxPosition = qMax(0, document->characterCount() - 1);
    const int position = qBound(0, source.position(), maxPosition);
    const int anchor = qBound(0, source.anchor(), maxPosition);

    QTextCursor cursor(document);
    cursor.setPosition(anchor);
    cursor.setPosition(position, QTextCursor::KeepAnchor);
    to->setTextCursor(cursor);
}

} // namespace papiro