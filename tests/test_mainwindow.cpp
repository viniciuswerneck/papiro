#include "app/Document.h"
#include "app/MainWindow.h"
#include "editor/Editor.h"
#include "ui/FindReplaceDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScopedPointer>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

using namespace papiro;

class TestMainWindow : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void restoresPreviousSession();
    void disabledSessionStartsEmpty();
    void findResultsPanel();
    void findInFolder();
    void macroRecordingPersists();
    void macroShortcuts();
    void preferencesDialogRoundTrip();
    void preferencesApplyToWindow();
    void themeActionPersists();

private:
    QTemporaryDir m_settingsDirectory;
};

void TestMainWindow::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
    QCoreApplication::setOrganizationName(QStringLiteral("PapiroTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PapiroTest"));
}

void TestMainWindow::init()
{
    QSettings settings;
    settings.clear();
    Theme::setMode(Theme::Mode::Light);
}

void TestMainWindow::cleanup()
{
    Theme::apply(Theme::Mode::Light);
}

void TestMainWindow::restoresPreviousSession()
{
    QTemporaryDir documents;
    QVERIFY(documents.isValid());
    const QString path = documents.filePath(QStringLiteral("nota.txt"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("alpha\nbeta\ngamma\n");
    file.close();

    {
        MainWindow window;
        window.show();
        window.openFile(path);

        auto *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 1);

        auto *document = qobject_cast<Document *>(tabs->currentWidget());
        QVERIFY(document);
        document->editor()->gotoLine(2, 3);

        QVERIFY(window.close());
    }

    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("session/files")).toStringList(),
             QStringList({path}));
    QCOMPARE(settings.value(QStringLiteral("session/active")).toInt(), 0);

    MainWindow restored;
    restored.show();

    auto *tabs = restored.findChild<QTabWidget *>();
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    auto *document = qobject_cast<Document *>(tabs->currentWidget());
    QVERIFY(document);
    QCOMPARE(document->filePath(), QFileInfo(path).absoluteFilePath());
    QCOMPARE(document->editor()->currentLine(), 2);
    QCOMPARE(document->editor()->currentColumn(), 3);
    QCOMPARE(document->editor()->toPlainText(), QStringLiteral("alpha\nbeta\ngamma\n"));
}

void TestMainWindow::disabledSessionStartsEmpty()
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/restore"), false);

    MainWindow window;
    window.show();

    auto *tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    auto *document = qobject_cast<Document *>(tabs->currentWidget());
    QVERIFY(document);
    QVERIFY(document->filePath().isEmpty());
}

void TestMainWindow::findResultsPanel()
{
    QTemporaryDir documents;
    QVERIFY(documents.isValid());
    const QString path = documents.filePath(QStringLiteral("busca.txt"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("alpha beta\ngamma alpha\n");
    file.close();

    MainWindow window;
    window.show();
    window.openFile(path);

    QVERIFY(QMetaObject::invokeMethod(&window, "showFindReplace", Qt::DirectConnection));

    auto *dialog = window.findChild<FindReplaceDialog *>();
    QVERIFY(dialog);
    auto *findEdit = dialog->findChild<QLineEdit *>(QStringLiteral("FindLineEdit"));
    QVERIFY(findEdit);
    findEdit->setText(QStringLiteral("alpha"));

    auto *findAll = dialog->findChild<QPushButton *>(QStringLiteral("FindAllButton"));
    QVERIFY(findAll);
    findAll->click();

    auto *tree = window.findChild<QTreeWidget *>(QStringLiteral("SearchResultsView"));
    QVERIFY(tree);
    QCOMPARE(tree->topLevelItemCount(), 1);
    QTreeWidgetItem *root = tree->topLevelItem(0);
    QCOMPARE(root->childCount(), 2);

    QTreeWidgetItem *second = root->child(1);
    tree->itemActivated(second, 0);

    auto *tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    auto *document = qobject_cast<Document *>(tabs->currentWidget());
    QVERIFY(document);
    QCOMPARE(document->editor()->currentLine(), 2);
}

void TestMainWindow::findInFolder()
{
    QTemporaryDir documents;
    QVERIFY(documents.isValid());
    QDir root(documents.path());
    QVERIFY(root.mkpath(QStringLiteral("sub")));

    const QString firstPath = root.filePath(QStringLiteral("a.txt"));
    QFile first(firstPath);
    QVERIFY(first.open(QIODevice::WriteOnly));
    first.write("alpha\nbeta alpha\n");
    first.close();

    const QString nestedPath = root.filePath(QStringLiteral("sub/b.txt"));
    QFile nested(nestedPath);
    QVERIFY(nested.open(QIODevice::WriteOnly));
    nested.write("alpha again\n");
    nested.close();

    MainWindow window;
    window.show();
    QVERIFY(QMetaObject::invokeMethod(&window, "showFindReplace", Qt::DirectConnection));

    auto *dialog = window.findChild<FindReplaceDialog *>();
    QVERIFY(dialog);
    auto *findEdit = dialog->findChild<QLineEdit *>(QStringLiteral("FindLineEdit"));
    QVERIFY(findEdit);
    findEdit->setText(QStringLiteral("alpha"));
    dialog->setSearchFolder(documents.path());

    auto *findInFolder = dialog->findChild<QPushButton *>(
        QStringLiteral("FindInFolderButton"));
    QVERIFY(findInFolder);
    findInFolder->click();

    auto *tree = window.findChild<QTreeWidget *>(QStringLiteral("SearchResultsView"));
    QVERIFY(tree);
    QCOMPARE(tree->topLevelItemCount(), 2);

    int totalMatches = 0;
    QTreeWidgetItem *nestedRoot = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = tree->topLevelItem(i);
        totalMatches += item->childCount();
        if (item->text(0).contains(QStringLiteral("b.txt")))
            nestedRoot = item;
    }
    QCOMPARE(totalMatches, 3);
    QVERIFY(nestedRoot);
    QCOMPARE(nestedRoot->childCount(), 1);

    tree->itemActivated(nestedRoot->child(0), 0);

    auto *tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    auto *document = qobject_cast<Document *>(tabs->currentWidget());
    QVERIFY(document);
    QCOMPARE(document->filePath(), QFileInfo(nestedPath).absoluteFilePath());
    QCOMPARE(document->editor()->currentLine(), 1);
    QCOMPARE(document->editor()->currentColumn(), 1);
}

void TestMainWindow::macroRecordingPersists()
{
    const QString macroDirectory = MacroManager::defaultDirectory();
    QDir(macroDirectory).removeRecursively();

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    auto *document = qobject_cast<Document *>(tabs->currentWidget());
    QVERIFY(document);
    document->editor()->setFocus();

    QVERIFY(QMetaObject::invokeMethod(&window, "startMacroRecording", Qt::DirectConnection));
    QTest::keyClicks(document->editor(), QStringLiteral("abc"));
    QVERIFY(QMetaObject::invokeMethod(&window, "stopMacroRecording", Qt::DirectConnection));

    const QStringList files = QDir(macroDirectory).entryList({"*.tpm"}, QDir::Files);
    QCOMPARE(files.size(), 1);

    QString error;
    QScopedPointer<Macro> macro(
        Macro::loadFromFile(QDir(macroDirectory).filePath(files.first()), &error));
    QVERIFY2(macro, qPrintable(error));
    QVERIFY(!macro->actions().isEmpty());

    QVERIFY(QMetaObject::invokeMethod(&window, "saveMacroToSlot", Qt::DirectConnection,
                                      Q_ARG(int, 0)));

    QFile manifest(QDir(macroDirectory).filePath(QStringLiteral("slots.json")));
    QVERIFY(manifest.open(QIODevice::ReadOnly));
    QVERIFY(manifest.readAll().contains("\"slot\": 0"));

    QDir(macroDirectory).removeRecursively();
}

void TestMainWindow::macroShortcuts()
{
    const QString macroDirectory = MacroManager::defaultDirectory();
    QDir(macroDirectory).removeRecursively();

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    QApplication::setActiveWindow(&window);

    auto *tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    auto *document = qobject_cast<Document *>(tabs->currentWidget());
    QVERIFY(document);
    Editor *editor = document->editor();
    editor->setFocus();
    QTest::qWait(20);

    QAction *recordAction = nullptr;
    QAction *stopAction = nullptr;
    QAction *playAction = nullptr;
    for (QAction *action : window.findChildren<QAction *>()) {
        if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_F9))
            recordAction = action;
        else if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_F10))
            stopAction = action;
        else if (action->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9))
            playAction = action;
    }
    QVERIFY(recordAction);
    QVERIFY(stopAction);
    QVERIFY(playAction);

    QTest::keyClick(editor, Qt::Key_F9, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!recordAction->isEnabled(), 2000);

    QTest::keyClicks(editor, QStringLiteral("abc"));
    QTest::keyClick(editor, Qt::Key_F10, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(recordAction->isEnabled(), 2000);

    editor->selectAll();
    QTest::keyClick(editor, Qt::Key_Delete);
    QVERIFY(editor->toPlainText().isEmpty());

    QTest::keyClick(editor, Qt::Key_F9, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE_WITH_TIMEOUT(editor->toPlainText(), QStringLiteral("abc"), 2000);

    QVERIFY(QMetaObject::invokeMethod(&window, "saveMacroToSlot", Qt::DirectConnection,
                                      Q_ARG(int, 0)));
    editor->selectAll();
    QTest::keyClick(editor, Qt::Key_Delete);
    QVERIFY(editor->toPlainText().isEmpty());

    QTest::keyClick(editor, Qt::Key_1, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE_WITH_TIMEOUT(editor->toPlainText(), QStringLiteral("abc"), 2000);

    QDir(macroDirectory).removeRecursively();
}

void TestMainWindow::preferencesDialogRoundTrip()
{
    Preferences current;
    current.editorFont = QFont(QStringLiteral("Monospace"), 12);
    current.theme = Theme::Mode::Light;
    current.restoreSession = true;
    current.wordWrap = false;
    current.lineNumbers = true;
    current.spellCheck = true;
    current.searchMatchCase = false;
    current.searchWholeWord = false;
    current.searchRegex = false;
    current.searchWrap = true;

    PreferencesDialog dialog(current);

    auto *fontLabel = dialog.findChild<QLabel *>(QStringLiteral("PreferencesFontLabel"));
    QVERIFY(fontLabel);
    QVERIFY(fontLabel->text().contains(QStringLiteral("Monospace")));

    auto *theme = dialog.findChild<QComboBox *>(QStringLiteral("PreferencesThemeCombo"));
    auto *restore = dialog.findChild<QCheckBox *>(QStringLiteral("PreferencesRestoreSession"));
    auto *wordWrap = dialog.findChild<QCheckBox *>(QStringLiteral("PreferencesWordWrap"));
    auto *matchCase = dialog.findChild<QCheckBox *>(QStringLiteral("PreferencesMatchCase"));
    auto *wholeWord = dialog.findChild<QCheckBox *>(QStringLiteral("PreferencesWholeWord"));
    QVERIFY(theme);
    QVERIFY(restore);
    QVERIFY(wordWrap);
    QVERIFY(matchCase);
    QVERIFY(wholeWord);

    theme->setCurrentIndex(theme->findData(QStringLiteral("dark")));
    restore->setChecked(false);
    wordWrap->setChecked(true);
    matchCase->setChecked(true);
    wholeWord->setChecked(true);

    const Preferences result = dialog.preferences();
    QCOMPARE(result.theme, Theme::Mode::Dark);
    QVERIFY(!result.restoreSession);
    QVERIFY(result.wordWrap);
    QVERIFY(result.searchMatchCase);
    QVERIFY(result.searchWholeWord);
    QCOMPARE(result.editorFont.family(), QStringLiteral("Monospace"));
}

void TestMainWindow::preferencesApplyToWindow()
{
    {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QVERIFY(QMetaObject::invokeMethod(&window, "showFindReplace", Qt::DirectConnection));
        auto *findDialog = window.findChild<FindReplaceDialog *>();
        QVERIFY(findDialog);

        bool dialogSeen = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = window.findChild<PreferencesDialog *>();
            dialogSeen = dialog != nullptr;
            if (!dialog)
                return;

            auto *theme = dialog->findChild<QComboBox *>(
                QStringLiteral("PreferencesThemeCombo"));
            auto *restore = dialog->findChild<QCheckBox *>(
                QStringLiteral("PreferencesRestoreSession"));
            auto *matchCase = dialog->findChild<QCheckBox *>(
                QStringLiteral("PreferencesMatchCase"));
            auto *wholeWord = dialog->findChild<QCheckBox *>(
                QStringLiteral("PreferencesWholeWord"));
            auto *wrap = dialog->findChild<QCheckBox *>(QStringLiteral("PreferencesWrap"));
            QVERIFY(theme);
            QVERIFY(restore);
            QVERIFY(matchCase);
            QVERIFY(wholeWord);
            QVERIFY(wrap);

            theme->setCurrentIndex(theme->findData(QStringLiteral("dark")));
            restore->setChecked(false);
            matchCase->setChecked(true);
            wholeWord->setChecked(true);
            wrap->setChecked(false);
            dialog->accept();
        });

        QVERIFY(QMetaObject::invokeMethod(&window, "openPreferences", Qt::DirectConnection));
        QVERIFY(dialogSeen);

        QCOMPARE(Theme::mode(), Theme::Mode::Dark);
        QSettings settings;
        QCOMPARE(settings.value(QStringLiteral("ui/theme")).toString(), QStringLiteral("dark"));
        QVERIFY(settings.value(QStringLiteral("search/matchCase")).toBool());
        QVERIFY(settings.value(QStringLiteral("search/wholeWord")).toBool());
        QVERIFY(!settings.value(QStringLiteral("search/wrap"), true).toBool());
        QVERIFY(!settings.value(QStringLiteral("session/restore"), true).toBool());

        auto *dialogMatchCase = findDialog->findChild<QCheckBox *>(
            QStringLiteral("FindMatchCaseCheck"));
        auto *dialogWholeWord = findDialog->findChild<QCheckBox *>(
            QStringLiteral("FindWholeWordCheck"));
        auto *dialogWrap = findDialog->findChild<QCheckBox *>(QStringLiteral("FindWrapCheck"));
        QVERIFY(dialogMatchCase);
        QVERIFY(dialogWholeWord);
        QVERIFY(dialogWrap);
        QVERIFY(dialogMatchCase->isChecked());
        QVERIFY(dialogWholeWord->isChecked());
        QVERIFY(!dialogWrap->isChecked());
    }

    QSettings settings;
    settings.remove(QStringLiteral("search"));
    settings.setValue(QStringLiteral("ui/theme"), QStringLiteral("light"));
    settings.setValue(QStringLiteral("session/restore"), true);
    Theme::apply(Theme::Mode::Light);
}

void TestMainWindow::themeActionPersists()
{
    MainWindow window;
    window.show();

    QAction *darkAction = nullptr;
    QAction *lightAction = nullptr;
    for (QAction *action : window.findChildren<QAction *>()) {
        const QString key = action->data().toString();
        if (key == QStringLiteral("dark"))
            darkAction = action;
        else if (key == QStringLiteral("light"))
            lightAction = action;
    }
    QVERIFY(darkAction);
    QVERIFY(lightAction);
    QVERIFY(lightAction->isChecked());

    darkAction->trigger();
    QCOMPARE(Theme::mode(), Theme::Mode::Dark);
    QVERIFY(darkAction->isChecked());
    QCOMPARE(qApp->palette().color(QPalette::Base),
             Theme::palette(Theme::Mode::Dark).color(QPalette::Base));

    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("ui/theme")).toString(), QStringLiteral("dark"));

    lightAction->trigger();
    QCOMPARE(Theme::mode(), Theme::Mode::Light);
    QCOMPARE(settings.value(QStringLiteral("ui/theme")).toString(), QStringLiteral("light"));
}

QTEST_MAIN(TestMainWindow)

#include "test_mainwindow.moc"
