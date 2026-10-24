#pragma once

#include "editor/Editor.h"

#include <QSplitter>
#include <QWidget>

namespace papiro {

/*! A widget that shows two editors sharing the same document (split view). */
class SplitView : public QSplitter {
    Q_OBJECT

public:
    enum class Orientation {
        Horizontal,
        Vertical
    };

    explicit SplitView(Editor *primaryEditor, QWidget *parent = nullptr);
    ~SplitView() override;

    Editor *primaryEditor() const { return m_primaryEditor; }
    Editor *secondaryEditor() const { return m_secondaryEditor; }

    void setSplitOrientation(Orientation orientation);
    Orientation orientation() const { return m_orientation; }

    void syncFromPrimary();
    void syncFromSecondary();

signals:
    void secondaryEditorCreated(Editor *editor);

private slots:
    void onPrimaryScroll(int value);
    void onSecondaryScroll(int value);
    void onPrimaryCursorPositionChanged();
    void onSecondaryCursorPositionChanged();
    void onPrimarySelectionChanged();
    void onSecondarySelectionChanged();

private:
    void setupSecondaryEditor();
    void syncScroll(QScrollBar *from, QScrollBar *to);
    void syncCursor(Editor *from, Editor *to);
    void syncSelection(Editor *from, Editor *to);

    Editor *m_primaryEditor = nullptr;
    Editor *m_secondaryEditor = nullptr;
    Orientation m_orientation = Orientation::Horizontal;
    bool m_syncingScroll = false;
    bool m_syncingCursor = false;
    bool m_syncingSelection = false;
};

} // namespace papiro