#include "ui/SortLinesDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>

namespace papiro {

SortLinesDialog::SortLinesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sort Lines"));
    setModal(true);
    resize(350, 200);

    m_keyCountSpin = new QSpinBox(this);
    m_keyCountSpin->setRange(1, 3);
    m_keyCountSpin->setValue(1);
    m_keyCountSpin->setToolTip(tr("Number of sort keys (columns)"));

    m_ascendingCombo = new QComboBox(this);
    m_ascendingCombo->addItem(tr("Ascending (A-Z, 0-9)"), true);
    m_ascendingCombo->addItem(tr("Descending (Z-A, 9-0)"), false);
    m_ascendingCombo->setCurrentIndex(0);

    m_caseSensitiveCheck = new QCheckBox(tr("Case sensitive"), this);
    m_caseSensitiveCheck->setChecked(true);

    auto *form = new QFormLayout;
    form->addRow(tr("Sort &keys (columns):"), m_keyCountSpin);
    form->addRow(tr("&Order:"), m_ascendingCombo);
    form->addRow(QString(), m_caseSensitiveCheck);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_buttonBox);
}

int SortLinesDialog::keyCount() const
{
    return m_keyCountSpin->value();
}

bool SortLinesDialog::ascending() const
{
    return m_ascendingCombo->currentIndex() == 0;
}

Qt::CaseSensitivity SortLinesDialog::caseSensitivity() const
{
    return m_caseSensitiveCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
}

} // namespace papiro