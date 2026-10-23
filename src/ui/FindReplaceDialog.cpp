#include "ui/FindReplaceDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace papiro {

FindReplaceDialog::FindReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Find and Replace"));
    setModal(false);

    m_find = new QLineEdit(this);
    m_replace = new QLineEdit(this);

    m_matchCase = new QCheckBox(tr("Match &case"), this);
    m_wholeWord = new QCheckBox(tr("&Whole word"), this);
    m_regex = new QCheckBox(tr("Regular &expression"), this);
    m_wrap = new QCheckBox(tr("&Wrap around"), this);
    m_wrap->setChecked(true);

    auto *fields = new QGridLayout;
    fields->addWidget(new QLabel(tr("&Find:"), this), 0, 0);
    fields->addWidget(m_find, 0, 1, 1, 3);
    fields->addWidget(new QLabel(tr("Re&place:"), this), 1, 0);
    fields->addWidget(m_replace, 1, 1, 1, 3);

    auto *options = new QGroupBox(tr("Options"), this);
    auto *optionsLayout = new QGridLayout(options);
    optionsLayout->addWidget(m_matchCase, 0, 0);
    optionsLayout->addWidget(m_wholeWord, 0, 1);
    optionsLayout->addWidget(m_regex, 1, 0);
    optionsLayout->addWidget(m_wrap, 1, 1);

    auto *findNext = new QPushButton(tr("Find &Next"), this);
    findNext->setDefault(true);
    auto *findPrevious = new QPushButton(tr("Find &Previous"), this);
    auto *replace = new QPushButton(tr("&Replace"), this);
    auto *replaceAll = new QPushButton(tr("Replace &All"), this);
    auto *close = new QPushButton(tr("Close"), this);
    close->setAutoDefault(false);

    auto *buttons = new QGridLayout;
    buttons->addWidget(findNext, 0, 0);
    buttons->addWidget(findPrevious, 1, 0);
    buttons->addWidget(replace, 0, 1);
    buttons->addWidget(replaceAll, 1, 1);
    buttons->addWidget(close, 2, 0, 1, 2);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(fields);
    layout->addWidget(options);
    layout->addLayout(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(findNext, &QPushButton::clicked, this, [this] { emitFind(false); });
    connect(findPrevious, &QPushButton::clicked, this, [this] { emitFind(true); });
    connect(replace, &QPushButton::clicked, this, &FindReplaceDialog::emitReplace);
    connect(replaceAll, &QPushButton::clicked, this, &FindReplaceDialog::emitReplaceAll);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(m_find, &QLineEdit::returnPressed, this, [this] { emitFind(false); });
    connect(m_replace, &QLineEdit::returnPressed, this, &FindReplaceDialog::emitReplace);
}

void FindReplaceDialog::setFindText(const QString &text)
{
    m_find->setText(text);
    if (!text.isEmpty())
        m_find->selectAll();
}

void FindReplaceDialog::focusFindField()
{
    m_find->setFocus();
    m_find->selectAll();
}

FindRequest FindReplaceDialog::request() const
{
    FindRequest result;
    result.pattern = m_find->text();
    result.matchCase = m_matchCase->isChecked();
    result.wholeWord = m_wholeWord->isChecked();
    result.regex = m_regex->isChecked();
    result.wrap = m_wrap->isChecked();
    return result;
}

void FindReplaceDialog::emitFind(bool backward)
{
    emit findNextRequested(request(), backward);
}

void FindReplaceDialog::emitReplace()
{
    emit replaceRequested(request(), m_replace->text());
}

void FindReplaceDialog::emitReplaceAll()
{
    emit replaceAllRequested(request(), m_replace->text());
}

} // namespace papiro
