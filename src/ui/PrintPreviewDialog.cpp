#include "ui/PrintPreviewDialog.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPageLayout>
#include <QPainter>
#include <QPalette>
#include <QPrintDialog>
#include <QPrintPreviewWidget>
#include <QPrinter>
#include <QRegularExpression>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QToolBar>
#include <QVBoxLayout>

namespace papiro {
namespace {

constexpr int DefaultFontPointSize = 10;
constexpr int DefaultMarginMm = 20;
constexpr qreal MillimetersPerInch = 25.4;

QIcon previewIcon(const QString &themeName, QStyle::StandardPixmap fallback, QWidget *widget)
{
    const QIcon icon = QIcon::fromTheme(themeName);
    if (!icon.isNull())
        return icon;
    return widget->style()->standardIcon(fallback);
}

QFont documentFont(const QString &family, int pointSize)
{
    QFont font;
    if (family.isEmpty())
        font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    else
        font.setFamily(family);
    font.setPointSize(pointSize > 0 ? pointSize : DefaultFontPointSize);
    font.setStyleHint(QFont::Monospace);
    return font;
}

/*! Draws one header/footer line inside \a rect, elided to the available width. */
void drawHeaderFooterLine(QPainter &painter, const QRectF &rect, const QString &text,
                          Qt::Alignment alignment)
{
    if (text.isEmpty() || rect.isEmpty())
        return;

    painter.save();
    painter.setClipRect(rect);
    painter.setPen(Qt::black);

    const QFontMetrics metrics = painter.fontMetrics();
    const QString elided = metrics.elidedText(text, Qt::ElideRight, qMax(0, int(rect.width())));

    qreal x = rect.left();
    if (alignment & Qt::AlignRight)
        x = rect.right() - metrics.horizontalAdvance(elided);
    else if (alignment & Qt::AlignHCenter)
        x = rect.center().x() - metrics.horizontalAdvance(elided) / 2.0;

    const qreal baseline = rect.center().y() + (metrics.ascent() - metrics.descent()) / 2.0;
    painter.drawText(QPointF(x, baseline), elided);
    painter.restore();
}

} // namespace

PrintPreviewDialog::PrintPreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("PrintPreviewDialog"));
    setWindowTitle(tr("Print Preview"));
    setModal(true);
    resize(960, 760);

    loadSettings();

    m_printer = new QPrinter(QPrinter::HighResolution);
    m_printer->setFullPage(true);
    m_printer->setDocName(tr("Papiro Document"));

    m_preview = new QPrintPreviewWidget(m_printer, this);
    m_preview->setObjectName(QStringLiteral("PrintPreviewWidget"));

    createUi();

    connect(m_preview, &QPrintPreviewWidget::paintRequested, this,
            &PrintPreviewDialog::paintDocument);
    connect(m_preview, &QPrintPreviewWidget::previewChanged, this, [this] {
        if (!m_pageLabel)
            return;
        const int pages = m_preview ? m_preview->pageCount() : 0;
        if (pages <= 0)
            m_pageLabel->setText(tr("No pages"));
        else
            m_pageLabel->setText(tr("Page %1 of %2")
                                     .arg(qBound(1, m_preview->currentPage(), pages))
                                     .arg(pages));
    });

    updatePreview();
}

PrintPreviewDialog::~PrintPreviewDialog()
{
    saveSettings();
}

void PrintPreviewDialog::setContent(const QString &text, const QString &filePath,
                                    const QString &title)
{
    m_content = text;
    m_filePath = filePath;
    m_fileName = QFileInfo(filePath).fileName();
    m_title = title;

    if (!m_fileName.isEmpty())
        m_printer->setDocName(m_fileName);
    else if (!title.isEmpty())
        m_printer->setDocName(title);

    updatePreview();
}

QString PrintPreviewDialog::expandTokens(const QString &textTemplate,
                                         const QString &fileName, const QString &filePath,
                                         const QString &title, const QDateTime &when,
                                         int page, int totalPages)
{
    const QHash<QString, QString> values = {
        {QStringLiteral("filename"), fileName},
        {QStringLiteral("filepath"), filePath},
        {QStringLiteral("title"), title},
        {QStringLiteral("date"), when.date().toString(Qt::ISODate)},
        {QStringLiteral("time"), when.time().toString(Qt::ISODate)},
        {QStringLiteral("page"), QString::number(page)},
        {QStringLiteral("totalPages"), QString::number(totalPages)},
    };

    static const QRegularExpression token(QStringLiteral("\\$\\{([A-Za-z]+)\\}"));
    QString result;
    result.reserve(textTemplate.size());

    int last = 0;
    QRegularExpressionMatchIterator matches = token.globalMatch(textTemplate);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result += textTemplate.mid(last, match.capturedStart() - last);
        const auto found = values.constFind(match.captured(1));
        if (found != values.constEnd())
            result += found.value();
        else
            result += match.captured(0); // Unknown tokens stay literal.
        last = match.capturedEnd();
    }
    result += textTemplate.mid(last);
    return result;
}

void PrintPreviewDialog::createUi()
{
    auto *toolbar = new QToolBar(this);
    toolbar->setObjectName(QStringLiteral("PrintPreviewToolBar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto *printAction = new QAction(
        previewIcon(QStringLiteral("document-print"), QStyle::SP_FileIcon, this),
        tr("Print..."), this);
    printAction->setObjectName(QStringLiteral("PrintPreviewPrintAction"));
    connect(printAction, &QAction::triggered, this, &PrintPreviewDialog::printDocument);
    toolbar->addAction(printAction);

    auto *closeAction = new QAction(
        previewIcon(QStringLiteral("window-close"), QStyle::SP_DialogCloseButton, this),
        tr("Close"), this);
    closeAction->setObjectName(QStringLiteral("PrintPreviewCloseAction"));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);
    toolbar->addAction(closeAction);

    toolbar->addSeparator();

    auto *zoomOutAction = new QAction(
        previewIcon(QStringLiteral("zoom-out"), QStyle::SP_ArrowLeft, this),
        tr("Zoom Out"), this);
    connect(zoomOutAction, &QAction::triggered, this, [this] {
        if (m_preview)
            m_preview->zoomOut();
    });
    toolbar->addAction(zoomOutAction);

    auto *zoomInAction = new QAction(
        previewIcon(QStringLiteral("zoom-in"), QStyle::SP_ArrowRight, this),
        tr("Zoom In"), this);
    connect(zoomInAction, &QAction::triggered, this, [this] {
        if (m_preview)
            m_preview->zoomIn();
    });
    toolbar->addAction(zoomInAction);

    auto *fitPageAction = new QAction(tr("Fit Page"), this);
    fitPageAction->setCheckable(true);
    auto *fitWidthAction = new QAction(tr("Fit Width"), this);
    fitWidthAction->setCheckable(true);
    auto *zoomGroup = new QActionGroup(this);
    zoomGroup->setExclusive(true);
    zoomGroup->addAction(fitPageAction);
    zoomGroup->addAction(fitWidthAction);
    fitPageAction->setChecked(true);
    connect(fitPageAction, &QAction::triggered, this, [this] {
        if (m_preview)
            m_preview->setZoomMode(QPrintPreviewWidget::FitInView);
    });
    connect(fitWidthAction, &QAction::triggered, this, [this] {
        if (m_preview)
            m_preview->setZoomMode(QPrintPreviewWidget::FitToWidth);
    });
    toolbar->addAction(fitPageAction);
    toolbar->addAction(fitWidthAction);

    toolbar->addSeparator();
    m_pageLabel = new QLabel(tr("No pages"), this);
    m_pageLabel->setObjectName(QStringLiteral("PrintPreviewPageLabel"));
    m_pageLabel->setMinimumWidth(110);
    toolbar->addWidget(m_pageLabel);

    // --- Page setup --------------------------------------------------------
    auto *setupGroup = new QGroupBox(tr("Page Setup"), this);
    setupGroup->setObjectName(QStringLiteral("PrintPreviewPageSetup"));

    m_headerEdit = new QLineEdit(m_headerTemplate, this);
    m_headerEdit->setObjectName(QStringLiteral("PrintPreviewHeaderEdit"));
    m_headerEdit->setPlaceholderText(tr("Header template, e.g. ${filename}"));
    m_headerEdit->setClearButtonEnabled(true);

    m_footerEdit = new QLineEdit(m_footerTemplate, this);
    m_footerEdit->setObjectName(QStringLiteral("PrintPreviewFooterEdit"));
    m_footerEdit->setPlaceholderText(tr("Footer template, e.g. ${page} / ${totalPages}"));
    m_footerEdit->setClearButtonEnabled(true);

    auto *headerLabel = new QLabel(tr("&Header:"), this);
    headerLabel->setBuddy(m_headerEdit);
    auto *footerLabel = new QLabel(tr("&Footer:"), this);
    footerLabel->setBuddy(m_footerEdit);

    auto *marginGrid = new QGridLayout;
    marginGrid->setContentsMargins(0, 0, 0, 0);
    struct MarginField {
        QString label;
        int value;
        QSpinBox **target;
        QString objectName;
    };
    const QVector<MarginField> margins = {
        {tr("&Top:"), m_marginTopMm, &m_marginTop, QStringLiteral("PrintPreviewMarginTop")},
        {tr("&Bottom:"), m_marginBottomMm, &m_marginBottom,
         QStringLiteral("PrintPreviewMarginBottom")},
        {tr("&Left:"), m_marginLeftMm, &m_marginLeft, QStringLiteral("PrintPreviewMarginLeft")},
        {tr("&Right:"), m_marginRightMm, &m_marginRight,
         QStringLiteral("PrintPreviewMarginRight")},
    };
    int marginIndex = 0;
    for (const MarginField &margin : margins) {
        auto *label = new QLabel(margin.label, this);
        auto *spin = new QSpinBox(this);
        spin->setRange(0, 100);
        spin->setValue(margin.value);
        spin->setSuffix(tr(" mm"));
        spin->setObjectName(margin.objectName);
        label->setBuddy(spin);
        *margin.target = spin;
        marginGrid->addWidget(label, marginIndex / 2, (marginIndex % 2) * 2);
        marginGrid->addWidget(spin, marginIndex / 2, (marginIndex % 2) * 2 + 1);
        ++marginIndex;
    }

    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setObjectName(QStringLiteral("PrintPreviewFontFamily"));
    if (!m_fontFamily.isEmpty()) {
        const QFont configured(m_fontFamily);
        m_fontCombo->setCurrentFont(configured);
    }

    m_fontSizeSpin = new QSpinBox(this);
    m_fontSizeSpin->setObjectName(QStringLiteral("PrintPreviewFontSize"));
    m_fontSizeSpin->setRange(4, 72);
    m_fontSizeSpin->setValue(m_fontSize);
    m_fontSizeSpin->setSuffix(tr(" pt"));

    m_formFeedCheck = new QCheckBox(tr("Page break on form feed (\\f)"), this);
    m_formFeedCheck->setObjectName(QStringLiteral("PrintPreviewFormFeedCheck"));
    m_formFeedCheck->setChecked(m_pageBreakOnFormFeed);

    auto *fontRow = new QHBoxLayout;
    fontRow->setContentsMargins(0, 0, 0, 0);
    auto *fontLabel = new QLabel(tr("&Font:"), this);
    fontLabel->setBuddy(m_fontCombo);
    fontRow->addWidget(fontLabel);
    fontRow->addWidget(m_fontCombo, 1);
    fontRow->addWidget(m_fontSizeSpin);

    auto *optionsLayout = new QGridLayout;
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->addLayout(fontRow, 0, 0, 1, 4);
    optionsLayout->addWidget(m_formFeedCheck, 1, 0, 1, 4);

    auto *setupLayout = new QGridLayout(setupGroup);
    setupLayout->addWidget(headerLabel, 0, 0);
    setupLayout->addWidget(m_headerEdit, 0, 1, 1, 3);
    setupLayout->addWidget(footerLabel, 1, 0);
    setupLayout->addWidget(m_footerEdit, 1, 1, 1, 3);
    setupLayout->addLayout(marginGrid, 2, 0, 2, 2);
    setupLayout->addLayout(optionsLayout, 2, 2, 2, 2);
    setupLayout->setColumnStretch(1, 1);
    setupLayout->setColumnStretch(3, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(toolbar);
    layout->addWidget(setupGroup);
    layout->addWidget(m_preview, 1);

    connect(m_headerEdit, &QLineEdit::textChanged, this, &PrintPreviewDialog::updatePreview);
    connect(m_footerEdit, &QLineEdit::textChanged, this, &PrintPreviewDialog::updatePreview);
    connect(m_marginTop, &QSpinBox::valueChanged, this, &PrintPreviewDialog::updatePreview);
    connect(m_marginBottom, &QSpinBox::valueChanged, this, &PrintPreviewDialog::updatePreview);
    connect(m_marginLeft, &QSpinBox::valueChanged, this, &PrintPreviewDialog::updatePreview);
    connect(m_marginRight, &QSpinBox::valueChanged, this, &PrintPreviewDialog::updatePreview);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this,
            &PrintPreviewDialog::updatePreview);
    connect(m_fontSizeSpin, &QSpinBox::valueChanged, this, &PrintPreviewDialog::updatePreview);
    connect(m_formFeedCheck, &QCheckBox::toggled, this, &PrintPreviewDialog::updatePreview);

    m_preview->setZoomMode(QPrintPreviewWidget::FitInView);
}

void PrintPreviewDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("print"));
    m_headerTemplate = settings.value(QStringLiteral("headerTemplate"),
                                      QStringLiteral("${filename}")).toString();
    m_footerTemplate = settings.value(QStringLiteral("footerTemplate"),
                                      QStringLiteral("${page} / ${totalPages}")).toString();
    m_marginTopMm = settings.value(QStringLiteral("marginTop"), DefaultMarginMm).toInt();
    m_marginBottomMm = settings.value(QStringLiteral("marginBottom"), DefaultMarginMm).toInt();
    m_marginLeftMm = settings.value(QStringLiteral("marginLeft"), DefaultMarginMm).toInt();
    m_marginRightMm = settings.value(QStringLiteral("marginRight"), DefaultMarginMm).toInt();
    m_fontFamily = settings.value(QStringLiteral("fontFamily")).toString();
    m_fontSize = settings.value(QStringLiteral("fontSize"), DefaultFontPointSize).toInt();
    m_pageBreakOnFormFeed = settings.value(QStringLiteral("pageBreakOnFormFeed"), true).toBool();
    settings.endGroup();

    const auto clampMargin = [](int value) { return qBound(0, value, 100); };
    m_marginTopMm = clampMargin(m_marginTopMm);
    m_marginBottomMm = clampMargin(m_marginBottomMm);
    m_marginLeftMm = clampMargin(m_marginLeftMm);
    m_marginRightMm = clampMargin(m_marginRightMm);
    m_fontSize = qBound(4, m_fontSize, 72);
}

void PrintPreviewDialog::saveSettings()
{
    if (!m_headerEdit)
        return; // The constructor failed before the UI existed.

    QSettings settings;
    settings.beginGroup(QStringLiteral("print"));
    settings.setValue(QStringLiteral("headerTemplate"), m_headerEdit->text());
    settings.setValue(QStringLiteral("footerTemplate"), m_footerEdit->text());
    settings.setValue(QStringLiteral("marginTop"), m_marginTop->value());
    settings.setValue(QStringLiteral("marginBottom"), m_marginBottom->value());
    settings.setValue(QStringLiteral("marginLeft"), m_marginLeft->value());
    settings.setValue(QStringLiteral("marginRight"), m_marginRight->value());
    settings.setValue(QStringLiteral("fontFamily"), m_fontCombo->currentFont().family());
    settings.setValue(QStringLiteral("fontSize"), m_fontSizeSpin->value());
    settings.setValue(QStringLiteral("pageBreakOnFormFeed"), m_formFeedCheck->isChecked());
    settings.endGroup();
}

void PrintPreviewDialog::updatePreview()
{
    if (!m_preview || !m_headerEdit)
        return;

    m_headerTemplate = m_headerEdit->text();
    m_footerTemplate = m_footerEdit->text();
    m_marginTopMm = m_marginTop->value();
    m_marginBottomMm = m_marginBottom->value();
    m_marginLeftMm = m_marginLeft->value();
    m_marginRightMm = m_marginRight->value();
    m_fontFamily = m_fontCombo->currentFont().family();
    m_fontSize = m_fontSizeSpin->value();
    m_pageBreakOnFormFeed = m_formFeedCheck->isChecked();

    m_preview->updatePreview();
}

QString PrintPreviewDialog::pageBodyText(QVector<int> &breakOffsets) const
{
    breakOffsets.clear();

    QString source = m_content;
    source.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    source.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QString body;
    body.reserve(source.size());
    for (int i = 0; i < source.size(); ++i) {
        const QChar character = source.at(i);
        if (character != QChar::FormFeed) {
            body += character;
            continue;
        }

        // Form feeds are never printed; when enabled, they force the next
        // block to start on a fresh page.
        if (!m_pageBreakOnFormFeed)
            continue;

        if (body.isEmpty()) {
            breakOffsets.append(0);
        } else {
            if (!body.endsWith(QLatin1Char('\n')))
                body += QLatin1Char('\n');
            breakOffsets.append(body.size());
        }
    }
    return body;
}

void PrintPreviewDialog::paintDocument(QPrinter *printer)
{
    if (!printer)
        return;

    QPainter painter;
    if (!painter.begin(printer))
        return;

    const int resolution = printer->resolution();
    const QRectF paperRect(printer->pageLayout().fullRectPixels(resolution));
    const qreal pixelsPerMm = resolution / MillimetersPerInch;

    const qreal left = m_marginLeftMm * pixelsPerMm;
    const qreal right = m_marginRightMm * pixelsPerMm;
    const qreal top = m_marginTopMm * pixelsPerMm;
    const qreal bottom = m_marginBottomMm * pixelsPerMm;

    const qreal bodyWidth = paperRect.width() - left - right;
    const qreal bodyHeight = paperRect.height() - top - bottom;
    const bool validBody = bodyWidth > 1.0 && bodyHeight > 1.0;

    const QRectF bodyRect(paperRect.left() + left, paperRect.top() + top,
                          qMax<qreal>(1.0, bodyWidth), qMax<qreal>(1.0, bodyHeight));
    const QRectF headerRect(paperRect.left() + left, paperRect.top(),
                            bodyRect.width(), qMax<qreal>(0.0, top));
    const QRectF footerRect(paperRect.left() + left,
                            paperRect.top() + paperRect.height() - bottom,
                            bodyRect.width(), qMax<qreal>(0.0, bottom));

    QTextDocument document;
    document.setDocumentMargin(0);
    QFont bodyFont = documentFont(m_fontFamily, m_fontSize);
    document.setDefaultFont(bodyFont);
    document.documentLayout()->setPaintDevice(printer);

    QVector<int> breakOffsets;
    const QString body = pageBodyText(breakOffsets);
    document.setPlainText(body);

    for (int offset : breakOffsets) {
        if (offset < 0 || offset >= document.characterCount())
            continue;
        QTextBlock block = document.findBlock(offset);
        if (!block.isValid())
            continue;
        QTextBlockFormat format = block.blockFormat();
        format.setPageBreakPolicy(format.pageBreakPolicy()
                                  | QTextFormat::PageBreak_AlwaysBefore);
        QTextCursor cursor(block);
        cursor.setBlockFormat(format);
    }

    int pages = 1;
    if (validBody) {
        document.setPageSize(QSizeF(bodyRect.width(), bodyRect.height()));
        pages = qMax(1, document.pageCount());
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QString fileName = m_fileName;
    const QString filePath = m_filePath;
    const QString title = m_title;

    painter.setFont(bodyFont);
    for (int page = 0; page < pages; ++page) {
        if (page > 0)
            printer->newPage();

        painter.setFont(bodyFont);

        // Header and footer live inside the page margins, on every page.
        if (top > 0.0) {
            const QString header = expandTokens(m_headerTemplate, fileName, filePath, title, now,
                                                page + 1, pages);
            drawHeaderFooterLine(painter, headerRect, header, Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (bottom > 0.0) {
            const QString footer = expandTokens(m_footerTemplate, fileName, filePath, title, now,
                                                page + 1, pages);
            drawHeaderFooterLine(painter, footerRect, footer, Qt::AlignRight | Qt::AlignVCenter);
        }

        if (!validBody)
            continue;

        // Clip the body to the area between the margins so it can never
        // overlap the header or the footer, then draw the matching page.
        painter.save();
        painter.setClipRect(bodyRect);
        painter.translate(bodyRect.left(), bodyRect.top() - page * bodyRect.height());
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette.setColor(QPalette::Base, Qt::white);
        context.palette.setColor(QPalette::Text, Qt::black);
        context.clip = QRectF(0, page * bodyRect.height(),
                              bodyRect.width(), bodyRect.height());
        document.documentLayout()->draw(&painter, context);
        painter.restore();
    }

    painter.end();
}

void PrintPreviewDialog::printDocument()
{
    if (!m_printer)
        return;

    QPrintDialog dialog(m_printer, this);
    dialog.setWindowTitle(tr("Print"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (m_preview)
        m_preview->print();
}

} // namespace papiro
