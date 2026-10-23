#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;

namespace papiro {

struct FindRequest {
    QString pattern;
    bool matchCase = false;
    bool wholeWord = false;
    bool regex = false;
    bool wrap = true;
};

class FindReplaceDialog : public QDialog {
    Q_OBJECT

public:
    explicit FindReplaceDialog(QWidget *parent = nullptr);

    void setFindText(const QString &text);
    void focusFindField();

    FindRequest request() const;

signals:
    void findNextRequested(const papiro::FindRequest &request, bool backward);
    void replaceRequested(const papiro::FindRequest &request, const QString &replacement);
    void replaceAllRequested(const papiro::FindRequest &request, const QString &replacement);

private:
    void emitFind(bool backward);
    void emitReplace();
    void emitReplaceAll();

    QLineEdit *m_find = nullptr;
    QLineEdit *m_replace = nullptr;
    QCheckBox *m_matchCase = nullptr;
    QCheckBox *m_wholeWord = nullptr;
    QCheckBox *m_regex = nullptr;
    QCheckBox *m_wrap = nullptr;
};

} // namespace papiro
