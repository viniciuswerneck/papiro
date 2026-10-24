#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>

namespace papiro {

/*! Dialog for configuring sort lines options. */
class SortLinesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SortLinesDialog(QWidget *parent = nullptr);

    int keyCount() const;
    bool ascending() const;
    Qt::CaseSensitivity caseSensitivity() const;

private:
    QSpinBox *m_keyCountSpin = nullptr;
    QComboBox *m_ascendingCombo = nullptr;
    QCheckBox *m_caseSensitiveCheck = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

} // namespace papiro