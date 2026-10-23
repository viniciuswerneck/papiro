#pragma once

#include "ui/Theme.h"

#include <QDialog>
#include <QFont>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace papiro {

/*! Values edited by PreferencesDialog and consumed by MainWindow. */
struct Preferences {
    QFont editorFont;
    Theme::Mode theme = Theme::Mode::Light;
    bool restoreSession = true;
    bool wordWrap = false;
    bool lineNumbers = true;
    bool spellCheck = true;
    bool searchMatchCase = false;
    bool searchWholeWord = false;
    bool searchRegex = false;
    bool searchWrap = true;
};

/*! Single dialog for editor font, theme, session restoration and search defaults. */
class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(const Preferences &current, QWidget *parent = nullptr);

    Preferences preferences() const;

private:
    void chooseFont();
    void updateFontLabel();

    QFont m_font;
    QLabel *m_fontLabel = nullptr;
    QPushButton *m_fontButton = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QCheckBox *m_restoreSession = nullptr;
    QCheckBox *m_wordWrap = nullptr;
    QCheckBox *m_lineNumbers = nullptr;
    QCheckBox *m_spellCheck = nullptr;
    QCheckBox *m_matchCase = nullptr;
    QCheckBox *m_wholeWord = nullptr;
    QCheckBox *m_regex = nullptr;
    QCheckBox *m_wrap = nullptr;
};

} // namespace papiro
