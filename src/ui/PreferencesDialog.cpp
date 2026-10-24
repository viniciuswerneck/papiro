#include "ui/PreferencesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace papiro {

PreferencesDialog::PreferencesDialog(const Preferences &current, QWidget *parent)
    : QDialog(parent)
    , m_font(current.editorFont)
{
    setObjectName(QStringLiteral("PreferencesDialog"));
    setWindowTitle(tr("Preferences"));
    setModal(true);
    resize(520, 380);

    // --- Editor -----------------------------------------------------------
    auto *editorPage = new QWidget(this);

    m_fontLabel = new QLabel(this);
    m_fontLabel->setObjectName(QStringLiteral("PreferencesFontLabel"));
    m_fontButton = new QPushButton(tr("C&hoose..."), this);
    m_fontButton->setObjectName(QStringLiteral("PreferencesFontButton"));

    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(m_fontLabel, 1);
    fontRow->addWidget(m_fontButton);

    m_wordWrap = new QCheckBox(tr("&Word wrap"), this);
    m_wordWrap->setObjectName(QStringLiteral("PreferencesWordWrap"));
    m_wordWrap->setChecked(current.wordWrap);
    m_lineNumbers = new QCheckBox(tr("Show &line numbers"), this);
    m_lineNumbers->setObjectName(QStringLiteral("PreferencesLineNumbers"));
    m_lineNumbers->setChecked(current.lineNumbers);
    m_whitespaceVisible = new QCheckBox(tr("Show &whitespace"), this);
    m_whitespaceVisible->setObjectName(QStringLiteral("PreferencesWhitespaceVisible"));
    m_whitespaceVisible->setChecked(current.whitespaceVisible);
    m_cursorFreePositioning = new QCheckBox(tr("Cursor free &positioning (virtual space)"), this);
    m_cursorFreePositioning->setObjectName(QStringLiteral("PreferencesCursorFreePositioning"));
    m_cursorFreePositioning->setChecked(current.cursorFreePositioning);
    m_cursorFreePositioning->setToolTip(tr("Allow cursor to move beyond end of line"));
    m_spellCheck = new QCheckBox(tr("Enable &spell checking"), this);
    m_spellCheck->setObjectName(QStringLiteral("PreferencesSpellCheck"));
    m_spellCheck->setChecked(current.spellCheck);
    m_largeFileThreshold = new QSpinBox(this);
    m_largeFileThreshold->setObjectName(QStringLiteral("PreferencesLargeFileThreshold"));
    m_largeFileThreshold->setRange(1, 1024);
    m_largeFileThreshold->setValue(current.largeFileThresholdMB);
    m_largeFileThreshold->setSuffix(tr(" MB"));
    m_largeFileThreshold->setToolTip(tr("Files larger than this open in read-only preview mode"));

    auto *editorForm = new QFormLayout;
    editorForm->addRow(tr("Editor font:"), fontRow);
    editorForm->addRow(QString(), m_wordWrap);
    editorForm->addRow(QString(), m_lineNumbers);
    editorForm->addRow(QString(), m_whitespaceVisible);
    editorForm->addRow(QString(), m_cursorFreePositioning);
    editorForm->addRow(QString(), m_spellCheck);
    editorForm->addRow(tr("Large file threshold:"), m_largeFileThreshold);

    auto *editorLayout = new QVBoxLayout(editorPage);
    editorLayout->addLayout(editorForm);
    editorLayout->addStretch();

    // --- Appearance -------------------------------------------------------
    auto *appearancePage = new QWidget(this);
    m_themeCombo = new QComboBox(this);
    m_themeCombo->setObjectName(QStringLiteral("PreferencesThemeCombo"));
    m_themeCombo->addItem(tr("Light"), Theme::key(Theme::Mode::Light));
    m_themeCombo->addItem(tr("Dark"), Theme::key(Theme::Mode::Dark));
    const int themeIndex = m_themeCombo->findData(Theme::key(current.theme));
    m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);

    auto *appearanceForm = new QFormLayout;
    appearanceForm->addRow(tr("Theme:"), m_themeCombo);

    auto *appearanceLayout = new QVBoxLayout(appearancePage);
    appearanceLayout->addLayout(appearanceForm);
    appearanceLayout->addStretch();

    // --- Session ----------------------------------------------------------
    auto *sessionPage = new QWidget(this);
    m_restoreSession = new QCheckBox(tr("Restore the previous session on startup"), this);
    m_restoreSession->setObjectName(QStringLiteral("PreferencesRestoreSession"));
    m_restoreSession->setChecked(current.restoreSession);

    auto *sessionLayout = new QVBoxLayout(sessionPage);
    sessionLayout->addWidget(m_restoreSession);
    sessionLayout->addStretch();

    // --- Search -----------------------------------------------------------
    auto *searchPage = new QWidget(this);
    m_matchCase = new QCheckBox(tr("Match &case"), this);
    m_matchCase->setObjectName(QStringLiteral("PreferencesMatchCase"));
    m_matchCase->setChecked(current.searchMatchCase);
    m_wholeWord = new QCheckBox(tr("&Whole word"), this);
    m_wholeWord->setObjectName(QStringLiteral("PreferencesWholeWord"));
    m_wholeWord->setChecked(current.searchWholeWord);
    m_regex = new QCheckBox(tr("Regular &expression"), this);
    m_regex->setObjectName(QStringLiteral("PreferencesRegex"));
    m_regex->setChecked(current.searchRegex);
    m_wrap = new QCheckBox(tr("&Wrap around"), this);
    m_wrap->setObjectName(QStringLiteral("PreferencesWrap"));
    m_wrap->setChecked(current.searchWrap);

    auto *searchGroup = new QGroupBox(tr("Search defaults"), this);
    auto *searchOptions = new QVBoxLayout(searchGroup);
    searchOptions->addWidget(m_matchCase);
    searchOptions->addWidget(m_wholeWord);
    searchOptions->addWidget(m_regex);
    searchOptions->addWidget(m_wrap);

    auto *searchLayout = new QVBoxLayout(searchPage);
    searchLayout->addWidget(new QLabel(
        tr("These options are used when the Find/Replace dialog opens."), this));
    searchLayout->addWidget(searchGroup);
    searchLayout->addStretch();

    // --- Tabs + buttons ---------------------------------------------------
    auto *tabs = new QTabWidget(this);
    tabs->addTab(editorPage, tr("Editor"));
    tabs->addTab(appearancePage, tr("Appearance"));
    tabs->addTab(sessionPage, tr("Session"));
    tabs->addTab(searchPage, tr("Search"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);

    connect(m_fontButton, &QPushButton::clicked, this, &PreferencesDialog::chooseFont);
    updateFontLabel();
}

Preferences PreferencesDialog::preferences() const
{
    Preferences result;
    result.editorFont = m_font;
    result.theme = Theme::fromKey(m_themeCombo->currentData().toString());
    result.restoreSession = m_restoreSession->isChecked();
    result.wordWrap = m_wordWrap->isChecked();
    result.lineNumbers = m_lineNumbers->isChecked();
    result.whitespaceVisible = m_whitespaceVisible->isChecked();
    result.cursorFreePositioning = m_cursorFreePositioning->isChecked();
    result.spellCheck = m_spellCheck->isChecked();
    result.largeFileThresholdMB = m_largeFileThreshold->value();
    result.searchMatchCase = m_matchCase->isChecked();
    result.searchWholeWord = m_wholeWord->isChecked();
    result.searchRegex = m_regex->isChecked();
    result.searchWrap = m_wrap->isChecked();
    return result;
}

void PreferencesDialog::chooseFont()
{
    bool ok = false;
    const QFont font = QFontDialog::getFont(&ok, m_font, this, tr("Editor Font"));
    if (!ok)
        return;
    m_font = font;
    updateFontLabel();
}

void PreferencesDialog::updateFontLabel()
{
    m_fontLabel->setText(tr("%1, %2 pt").arg(m_font.family()).arg(m_font.pointSize()));
}

} // namespace papiro
