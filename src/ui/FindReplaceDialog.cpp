#include "ui/FindReplaceDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace papiro {

FindReplaceDialog::FindReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Find and Replace"));
    setModal(false);

    m_find = new QLineEdit(this);
    m_find->setObjectName(QStringLiteral("FindLineEdit"));
    m_replace = new QLineEdit(this);
    m_replace->setObjectName(QStringLiteral("ReplaceLineEdit"));
    m_folder = new QLineEdit(this);
    m_folder->setObjectName(QStringLiteral("FolderLineEdit"));
    m_folder->setPlaceholderText(tr("Folder to search"));

    m_matchCase = new QCheckBox(tr("Match &case"), this);
    m_matchCase->setObjectName(QStringLiteral("FindMatchCaseCheck"));
    m_wholeWord = new QCheckBox(tr("&Whole word"), this);
    m_wholeWord->setObjectName(QStringLiteral("FindWholeWordCheck"));
    m_regex = new QCheckBox(tr("Regular &expression"), this);
    m_regex->setObjectName(QStringLiteral("FindRegexCheck"));
    m_multiline = new QCheckBox(tr("&Multiline"), this);
    m_multiline->setObjectName(QStringLiteral("FindMultilineCheck"));
    m_multiline->setToolTip(tr("Allow ^ and $ to match start/end of each line"));
    m_wrap = new QCheckBox(tr("&Wrap around"), this);
    m_wrap->setObjectName(QStringLiteral("FindWrapCheck"));
    m_wrap->setChecked(true);

    auto *browse = new QPushButton(tr("..."), this);
    browse->setToolTip(tr("Choose folder"));

    auto *fields = new QGridLayout;
    fields->addWidget(new QLabel(tr("&Find:"), this), 0, 0);
    fields->addWidget(m_find, 0, 1, 1, 3);
    fields->addWidget(new QLabel(tr("Re&place:"), this), 1, 0);
    fields->addWidget(m_replace, 1, 1, 1, 3);
    fields->addWidget(new QLabel(tr("&Folder:"), this), 2, 0);
    fields->addWidget(m_folder, 2, 1, 1, 2);
    fields->addWidget(browse, 2, 3);

    auto *options = new QGroupBox(tr("Options"), this);
    auto *optionsLayout = new QGridLayout(options);
    optionsLayout->addWidget(m_matchCase, 0, 0);
    optionsLayout->addWidget(m_wholeWord, 0, 1);
    optionsLayout->addWidget(m_regex, 1, 0);
    optionsLayout->addWidget(m_multiline, 1, 1);
    optionsLayout->addWidget(m_wrap, 2, 0);

    auto *findNext = new QPushButton(tr("Find &Next"), this);
    findNext->setDefault(true);
    auto *findPrevious = new QPushButton(tr("Find &Previous"), this);
    auto *findAll = new QPushButton(tr("Find &All"), this);
    findAll->setObjectName(QStringLiteral("FindAllButton"));
    auto *findInFolder = new QPushButton(tr("Find in F&older"), this);
    findInFolder->setObjectName(QStringLiteral("FindInFolderButton"));
    auto *replace = new QPushButton(tr("&Replace"), this);
    auto *replaceAll = new QPushButton(tr("Replace &All"), this);
    auto *close = new QPushButton(tr("Close"), this);
    close->setAutoDefault(false);

    auto *buttons = new QGridLayout;
    buttons->addWidget(findNext, 0, 0);
    buttons->addWidget(findPrevious, 1, 0);
    buttons->addWidget(findAll, 2, 0);
    buttons->addWidget(findInFolder, 2, 1);
    buttons->addWidget(replace, 0, 1);
    buttons->addWidget(replaceAll, 1, 1);
    buttons->addWidget(close, 3, 0, 1, 2);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(fields);
    layout->addWidget(options);
    layout->addLayout(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(findNext, &QPushButton::clicked, this, [this] { emitFind(false); });
    connect(findPrevious, &QPushButton::clicked, this, [this] { emitFind(true); });
    connect(findAll, &QPushButton::clicked, this,
            [this] { emit findAllRequested(request()); });
    connect(findInFolder, &QPushButton::clicked, this,
            &FindReplaceDialog::emitFindInFolder);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(
            this, tr("Choose Folder"), m_folder->text());
        if (!folder.isEmpty())
            m_folder->setText(QDir::toNativeSeparators(folder));
    });
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

void FindReplaceDialog::setSearchFolder(const QString &path)
{
    if (path.isEmpty())
        return;
    if (m_folder->text() != path)
        m_folder->setText(QDir::toNativeSeparators(path));
}

QString FindReplaceDialog::searchFolder() const
{
    return QDir::fromNativeSeparators(m_folder->text().trimmed());
}

void FindReplaceDialog::setSearchOptions(bool matchCase, bool wholeWord, bool regex, bool multiline, bool wrap)
{
    m_matchCase->setChecked(matchCase);
    m_wholeWord->setChecked(wholeWord);
    m_regex->setChecked(regex);
    m_multiline->setChecked(multiline);
    m_wrap->setChecked(wrap);
}

FindRequest FindReplaceDialog::request() const
{
    FindRequest result;
    result.pattern = m_find->text();
    result.matchCase = m_matchCase->isChecked();
    result.wholeWord = m_wholeWord->isChecked();
    result.regex = m_regex->isChecked();
    result.multiline = m_multiline->isChecked();
    result.wrap = m_wrap->isChecked();
    return result;
}

void FindReplaceDialog::emitFind(bool backward)
{
    emit findNextRequested(request(), backward);
}

void FindReplaceDialog::emitFindInFolder()
{
    const QString folder = searchFolder();
    const QFileInfo info(folder);
    if (folder.isEmpty() || !info.isDir()) {
        QMessageBox::warning(this, tr("Find in Folder"),
                             tr("Choose a valid folder to search."));
        return;
    }
    emit findAllInFolderRequested(request(), info.absoluteFilePath());
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
