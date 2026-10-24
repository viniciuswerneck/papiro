#pragma once

#include <QDateTime>
#include <QDialog>
#include <QString>
#include <QVector>

class QCheckBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class QPrintPreviewWidget;
class QPrinter;
class QSpinBox;
class QToolBar;

namespace papiro {

/*! Print preview with configurable headers, footers, margins and page breaks. */
class PrintPreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit PrintPreviewDialog(QWidget *parent = nullptr);
    ~PrintPreviewDialog() override;

    /*! Sets the text shown in the preview and the metadata used by the
        ${filename}, ${filepath} and ${title} tokens. */
    void setContent(const QString &text, const QString &filePath,
                    const QString &title = QString());

    /*! Replaces the supported ${...} tokens, leaving unknown tokens untouched. */
    static QString expandTokens(const QString &textTemplate,
                                const QString &fileName, const QString &filePath,
                                const QString &title, const QDateTime &when,
                                int page, int totalPages);

private slots:
    void updatePreview();
    void printDocument();

private:
    void createUi();
    void loadSettings();
    void saveSettings();

    QString pageBodyText(QVector<int> &breakOffsets) const;
    void paintDocument(QPrinter *printer);

    QString m_content;
    QString m_filePath;
    QString m_fileName;
    QString m_title;

    QString m_headerTemplate;
    QString m_footerTemplate;
    int m_marginTopMm = 20;
    int m_marginBottomMm = 20;
    int m_marginLeftMm = 20;
    int m_marginRightMm = 20;
    QString m_fontFamily;
    int m_fontSize = 10;
    bool m_pageBreakOnFormFeed = true;

    QPrinter *m_printer = nullptr;
    QPrintPreviewWidget *m_preview = nullptr;
    QLineEdit *m_headerEdit = nullptr;
    QLineEdit *m_footerEdit = nullptr;
    QSpinBox *m_marginTop = nullptr;
    QSpinBox *m_marginBottom = nullptr;
    QSpinBox *m_marginLeft = nullptr;
    QSpinBox *m_marginRight = nullptr;
    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QCheckBox *m_formFeedCheck = nullptr;
    QLabel *m_pageLabel = nullptr;
};

} // namespace papiro
