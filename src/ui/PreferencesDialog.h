#pragma once

#include "ui/Theme.h"

#include <QDialog>
#include <QFont>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace papiro {

/*! Values edited by PreferencesDialog and consumed by MainWindow. */
struct Preferences {
    QFont editorFont;
    Theme::Mode theme = Theme::Mode::Light;
    QString language; // locale name (e.g. "pt"), empty follows the system locale
    bool restoreSession = true;
    bool wordWrap = false;
    bool lineNumbers = true;
    bool whitespaceVisible = false;
    bool cursorFreePositioning = false;
    bool spellCheck = true;
    bool searchMatchCase = false;
    bool searchWholeWord = false;
    bool searchRegex = false;
    bool searchWrap = true;
    int largeFileThresholdMB = 16; // MB, files above this open in read-only preview mode
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
    QComboBox *m_languageCombo = nullptr;
    QCheckBox *m_restoreSession = nullptr;
    QCheckBox *m_wordWrap = nullptr;
    QCheckBox *m_lineNumbers = nullptr;
    QCheckBox *m_whitespaceVisible = nullptr;
    QCheckBox *m_cursorFreePositioning = nullptr;
    QCheckBox *m_spellCheck = nullptr;
    QCheckBox *m_matchCase = nullptr;
    QCheckBox *m_wholeWord = nullptr;
    QCheckBox *m_regex = nullptr;
    QCheckBox *m_wrap = nullptr;
    QSpinBox *m_largeFileThreshold = nullptr;
};

} // namespace papiro
