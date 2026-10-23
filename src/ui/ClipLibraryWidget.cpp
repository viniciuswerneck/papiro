#include "ui/ClipLibraryWidget.h"

#include "core/ClipLibrary.h"
#include "editor/Editor.h"

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QTreeView>

namespace papiro {

// ---------------------------------------------------------------------------
// ClipEditorDialog
// ---------------------------------------------------------------------------

class ClipEditorDialog : public QDialog {
public:
    explicit ClipEditorDialog(QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowTitle(tr("Edit Clip"));
        resize(500, 350);

        auto *layout = new QVBoxLayout(this);

        // Form
        auto *form = new QFormLayout();
        m_titleEdit = new QLineEdit(this);
        m_categoryEdit = new QLineEdit(this);
        m_tagsEdit = new QLineEdit(this);
        m_tagsEdit->setPlaceholderText(tr("tag1, tag2, tag3"));
        m_contentEdit = new QTextEdit(this);
        m_contentEdit->setAcceptRichText(false);
        m_contentEdit->setLineWrapMode(QTextEdit::NoWrap);

        form->addRow(tr("&Title:"), m_titleEdit);
        form->addRow(tr("&Category:"), m_categoryEdit);
        form->addRow(tr("&Tags (comma-separated):"), m_tagsEdit);
        form->addRow(tr("&Content:"), m_contentEdit);
        layout->addLayout(form);

        // Buttons
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    void setClip(const Clip &clip) {
        m_titleEdit->setText(clip.title);
        m_categoryEdit->setText(clip.category);
        m_tagsEdit->setText(clip.tags.join(QStringLiteral(", ")));
        m_contentEdit->setPlainText(clip.content);
    }

    Clip getClip() const {
        Clip clip;
        clip.title = m_titleEdit->text().trimmed();
        clip.category = m_categoryEdit->text().trimmed();
        clip.content = m_contentEdit->toPlainText();
        clip.tags.clear();
        for (const QString &tag : m_tagsEdit->text().split(QLatin1Char(','))) {
            QString t = tag.trimmed();
            if (!t.isEmpty())
                clip.tags.append(t);
        }
        return clip;
    }

private:
    QLineEdit *m_titleEdit;
    QLineEdit *m_categoryEdit;
    QLineEdit *m_tagsEdit;
    QTextEdit *m_contentEdit;
};

// ---------------------------------------------------------------------------
// ClipLibraryWidget
// ---------------------------------------------------------------------------

ClipLibraryWidget::ClipLibraryWidget(ClipLibrary *library, QWidget *parent)
    : QWidget(parent)
    , m_library(library) {
    setupUi();

    connect(m_library, &ClipLibrary::libraryChanged, this, &ClipLibraryWidget::refreshModel);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ClipLibraryWidget::onSearchTextChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClipLibraryWidget::onCategoryChanged);
    connect(m_newBtn, &QPushButton::clicked, this, &ClipLibraryWidget::onNewClip);
    connect(m_editBtn, &QPushButton::clicked, this, &ClipLibraryWidget::onEditClip);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ClipLibraryWidget::onDeleteClip);
    connect(m_insertBtn, &QPushButton::clicked, this, &ClipLibraryWidget::onInsertClip);
    connect(m_treeView, &QTreeView::doubleClicked, this, &ClipLibraryWidget::onTreeDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &ClipLibraryWidget::onCustomContextMenu);

    refreshModel();
}

void ClipLibraryWidget::setEditor(Editor *editor) {
    m_editor = editor;
}

void ClipLibraryWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Toolbar
    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search clips..."));
    m_searchEdit->setClearButtonEnabled(true);
    toolbar->addWidget(m_searchEdit, 1);

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setMinimumWidth(150);
    toolbar->addWidget(m_categoryCombo);

    layout->addLayout(toolbar);

    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setUniformRowHeights(true);

    m_model = new QStandardItemModel(this);
    m_treeView->setModel(m_model);

    layout->addWidget(m_treeView, 1);

    // Button bar
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(4);

    m_newBtn = new QPushButton(tr("New"), this);
    m_editBtn = new QPushButton(tr("Edit"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_insertBtn = new QPushButton(tr("Insert"), this);
    m_insertBtn->setDefault(true);

    btnLayout->addWidget(m_newBtn);
    btnLayout->addWidget(m_editBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_insertBtn);

    layout->addLayout(btnLayout);

    // Context menu
    m_contextMenu = new QMenu(this);
    m_contextMenu->addAction(tr("Insert"), this, &ClipLibraryWidget::onInsertClip);
    m_contextMenu->addAction(tr("Edit"), this, &ClipLibraryWidget::onEditClip);
    m_contextMenu->addAction(tr("Delete"), this, &ClipLibraryWidget::onDeleteClip);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(tr("Copy to Clipboard"), this, [this] {
        if (auto *clip = currentClip()) {
            QApplication::clipboard()->setText(clip->content);
        }
    });
}

void ClipLibraryWidget::refreshModel() {
    // Update category combo
    const QString currentCat = m_categoryCombo->currentText();
    m_categoryCombo->blockSignals(true);
    m_categoryCombo->clear();
    m_categoryCombo->addItem(tr("All Categories"), QString());
    for (const QString &cat : m_library->categories()) {
        m_categoryCombo->addItem(cat, cat);
    }
    int idx = m_categoryCombo->findData(currentCat);
    if (idx >= 0) {
        m_categoryCombo->setCurrentIndex(idx);
    }
    m_categoryCombo->blockSignals(false);

    // Populate model
    populateModel(m_searchEdit->text(), m_categoryCombo->currentData().toString());
}

void ClipLibraryWidget::populateModel(const QString &filter, const QString &category) {
    m_model->clear();

    QVector<Clip *> clips;
    if (filter.isEmpty() && category.isEmpty()) {
        clips = m_library->search(QString());
    } else if (!filter.isEmpty()) {
        clips = m_library->search(filter);
        if (!category.isEmpty()) {
            QVector<Clip *> filtered;
            for (Clip *c : clips) {
                if (c->category == category)
                    filtered.append(c);
            }
            clips = filtered;
        }
    } else {
        clips = m_library->clipsInCategory(category);
    }

    for (Clip *clip : clips) {
        auto *item = new QStandardItem();
        item->setData(clip->id, Qt::UserRole);
        item->setData(clip->title, Qt::DisplayRole);
        item->setToolTip(
            tr("<b>%1</b><br/>Category: %2<br/>Tags: %3<br/>Uses: %4<br/>%5")
                .arg(clip->title)
                .arg(clip->category)
                .arg(clip->tags.join(QStringLiteral(", ")))
                .arg(clip->useCount)
                .arg(clip->content.left(200).toHtmlEscaped())
        );
        item->setEditable(false);
        m_model->appendRow(item);
    }
}

void ClipLibraryWidget::onSearchTextChanged(const QString &text) {
    Q_UNUSED(text);
    populateModel(m_searchEdit->text(), m_categoryCombo->currentData().toString());
}

void ClipLibraryWidget::onCategoryChanged(int index) {
    Q_UNUSED(index);
    populateModel(m_searchEdit->text(), m_categoryCombo->currentData().toString());
}

Clip *ClipLibraryWidget::currentClip() const {
    const QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid())
        return nullptr;
    const QString id = index.data(Qt::UserRole).toString();
    return m_library->findClip(id);
}

void ClipLibraryWidget::onNewClip() {
    ClipEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Clip clip = dlg.getClip();
        if (!clip.title.isEmpty() && !clip.content.isEmpty()) {
            m_library->addClip(clip.title, clip.content, clip.category, clip.tags);
        }
    }
}

void ClipLibraryWidget::onEditClip() {
    Clip *clip = currentClip();
    if (!clip)
        return;

    ClipEditorDialog dlg(this);
    dlg.setClip(*clip);
    if (dlg.exec() == QDialog::Accepted) {
        Clip updated = dlg.getClip();
        if (!updated.title.isEmpty() && !updated.content.isEmpty()) {
            m_library->updateClip(clip->id, updated.title, updated.content,
                                  updated.category, updated.tags);
        }
    }
}

void ClipLibraryWidget::onDeleteClip() {
    Clip *clip = currentClip();
    if (!clip)
        return;

    if (QMessageBox::question(this, tr("Delete Clip"),
                              tr("Delete clip \"%1\"?").arg(clip->title),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_library->removeClip(clip->id);
    }
}

void ClipLibraryWidget::onInsertClip() {
    Clip *clip = currentClip();
    if (!clip || !m_editor)
        return;

    m_editor->insertPlainText(clip->content);
    m_library->recordUsage(clip->id);
    emit clipInserted(clip->id);
}

void ClipLibraryWidget::onTreeDoubleClicked(const QModelIndex &index) {
    Q_UNUSED(index);
    onInsertClip();
}

void ClipLibraryWidget::onCustomContextMenu(const QPoint &pos) {
    const QModelIndex index = m_treeView->indexAt(pos);
    if (index.isValid()) {
        m_treeView->setCurrentIndex(index);
        m_contextMenu->exec(m_treeView->viewport()->mapToGlobal(pos));
    }
}

} // namespace papiro