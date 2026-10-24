#include "app/MainWindow.h"

#include "app/Document.h"
#include "core/ClipLibrary.h"
#include "core/ExternalTool.h"
#include "core/GitRepo.h"
#include "core/Macro.h"
#include "core/SpellChecker.h"
#include "core/SyntaxDefinition.h"
#include "core/Workspace.h"
#include "editor/Editor.h"
#include "ui/ClipLibraryWidget.h"
#include "ui/FileExplorerWidget.h"
#include "ui/GitDiffView.h"
#include "ui/MacroManagerDialog.h"
#include "ui/Theme.h"
#include "ui/PreferencesDialog.h"
#include "ui/KeyboardShortcutsDialog.h"
#include "ui/Theme.h"
#include "ui/ToolsDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDirIterator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontDialog>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QProcess>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QTimer>
#include <QVBoxLayout>

namespace papiro {
namespace {

QIcon themedIcon(const QString &themeName, QStyle::StandardPixmap fallback, QWidget *widget)
{
    const QIcon icon = QIcon::fromTheme(themeName);
    if (!icon.isNull())
        return icon;
    return widget->style()->standardIcon(fallback);
}

QString displayPath(const QString &path)
{
    const QFileInfo info(path);
    const QString native = QDir::toNativeSeparators(info.absoluteFilePath());
    return native;
}

bool looksLikeBinary(const QByteArray &data)
{
    const bool utf16Bom = data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF");
    if (utf16Bom)
        return false;
    if (data.contains('\0'))
        return true;

    const int sample = qMin(data.size(), 4096);
    int controlCharacters = 0;
    for (int i = 0; i < sample; ++i) {
        const uchar byte = static_cast<uchar>(data.at(i));
        if (byte < 0x09 || (byte > 0x0D && byte < 0x20))
            ++controlCharacters;
    }
    return sample > 0 && controlCharacters * 10 > sample;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabs(new QTabWidget(this))
    , m_macroManager(new MacroManager(this))
    , m_clipLibrary(new ClipLibrary(this))
    , m_toolManager(new ToolManager(this))
    , m_spellChecker(new SpellChecker(this))
{
    SyntaxRepository::instance().load();
    setAcceptDrops(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        updateWindowTitle();
        updateStatus();
        updateActions();
        updateDocumentMenuState();
        updateGitStatus();
    });

    applySettings();
    createActions();
    createStatusBar();
    createDockWidgets();

    // Load the spell-check dictionary that matches the interface language.
    QSettings spellSettings;
    QString spellLanguage = spellSettings.value(QStringLiteral("ui/language")).toString();
    if (spellLanguage.isEmpty())
        spellLanguage = QLocale::system().name();
    m_spellChecker->load(SpellChecker::defaultDictionaryFor(spellLanguage));

    // Load macros and keep the slot actions/labels in sync with the manager.
    m_macroManager->loadFromDirectory(MacroManager::defaultDirectory());

    // Connect MacroManager signals for dynamic custom shortcuts
    connect(m_macroManager, &MacroManager::shortcutChanged, this,
            [this](Macro *macro, const QKeySequence &shortcut) {
                registerCustomShortcut(macro, shortcut);
            });

    connect(m_macroManager, &MacroManager::macroListChanged, this, [this] {
        unregisterAllCustomShortcuts();
        registerAllCustomShortcuts();
        updateMacroActions();
    });

    connect(m_macroManager, &MacroManager::macroListChanged, this,
            &MainWindow::updateMacroActions);
    updateMacroActions();

    // Register existing custom shortcuts on startup
    registerAllCustomShortcuts();

    // Load clip library
    m_clipLibrary->load();

    // Load external tool definitions
    m_toolManager->load();
    rebuildToolsMenu();
    rebuildWorkspaceMenu();

    m_wordWrapAction->setChecked(m_wordWrap);
    m_lineNumbersAction->setChecked(m_lineNumbers);
    m_spellCheckAction->setChecked(m_spellCheck);

    // Restore dock/toolbar layout once all docks exist, then reopen the
    // documents from the previous session (or start with an empty document).
    QSettings windowSettings;
    const QByteArray windowState = windowSettings.value(QStringLiteral("window/state")).toByteArray();
    if (!windowState.isEmpty())
        restoreState(windowState);

    restoreSession();
    applyFontToAll();
    rebuildRecentMenu();
    updateWindowTitle();
    updateStatus();
    updateActions();
    resize(1100, 750);
}

// ---------------------------------------------------------------------------
// Actions, menus and status bar
// ---------------------------------------------------------------------------

void MainWindow::createActions()
{
    auto makeAction = [this](const QString &text, const QKeySequence &shortcut = QKeySequence(),
                             const QIcon &icon = QIcon()) {
        auto *action = new QAction(text, this);
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        if (!icon.isNull())
            action->setIcon(icon);
        return action;
    };

    auto *newAction = makeAction(tr("&New"), QKeySequence::New,
                                 themedIcon(QStringLiteral("document-new"), QStyle::SP_FileIcon,
                                            this));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);

    auto *openAction = makeAction(tr("&Open..."), QKeySequence::Open,
                                  style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(openAction, &QAction::triggered, this, &MainWindow::openFiles);

    m_saveAction = makeAction(tr("&Save"), QKeySequence::Save,
                              style()->standardIcon(QStyle::SP_DriveFDIcon));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveCurrentDocument);

    m_saveAsAction = makeAction(tr("Save &As..."), QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveCurrentDocumentAs);

    m_saveAllAction = makeAction(tr("Save A&ll"));
    connect(m_saveAllAction, &QAction::triggered, this, &MainWindow::saveAllDocuments);

    m_closeAction = makeAction(tr("&Close Tab"), QKeySequence::Close);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    auto *exitAction = makeAction(tr("E&xit"), QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    m_undoAction = makeAction(tr("&Undo"), QKeySequence::Undo,
                              themedIcon(QStringLiteral("edit-undo"), QStyle::SP_ArrowBack, this));
    connect(m_undoAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->undo();
    });

    m_redoAction = makeAction(tr("&Redo"), QKeySequence::Redo,
                              themedIcon(QStringLiteral("edit-redo"), QStyle::SP_ArrowForward, this));
    connect(m_redoAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->redo();
    });

    m_cutAction = makeAction(tr("Cu&t"), QKeySequence::Cut,
                             themedIcon(QStringLiteral("edit-cut"), QStyle::SP_ArrowUp, this));
    connect(m_cutAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->cutSelection();
    });

    m_copyAction = makeAction(tr("&Copy"), QKeySequence::Copy,
                              themedIcon(QStringLiteral("edit-copy"), QStyle::SP_FileIcon, this));
    connect(m_copyAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->copySelection();
    });

    m_pasteAction = makeAction(tr("&Paste"), QKeySequence::Paste,
                               themedIcon(QStringLiteral("edit-paste"), QStyle::SP_ArrowDown, this));
    connect(m_pasteAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->pasteSelection();
    });

    auto *selectAllAction = makeAction(tr("Select &All"), QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->selectAll();
    });

    auto *duplicateAction = makeAction(tr("D&uplicate Line"), QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(duplicateAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->duplicateCurrentLine();
    });

    auto *deleteLineAction = makeAction(tr("Delete &Line"),
                                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    connect(deleteLineAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->deleteCurrentLine();
    });

    auto *gotoAction = makeAction(tr("&Go to Line..."), QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(gotoAction, &QAction::triggered, this, &MainWindow::goToLine);

    m_findAction = makeAction(tr("&Find / Replace..."), QKeySequence::Find,
                              themedIcon(QStringLiteral("edit-find"), QStyle::SP_FileDialogContentsView,
                                         this));
    connect(m_findAction, &QAction::triggered, this, &MainWindow::showFindReplace);

    m_findNextAction = makeAction(tr("Find &Next"), QKeySequence(Qt::Key_F3));
    connect(m_findNextAction, &QAction::triggered, this, &MainWindow::findNext);

    m_findPreviousAction = makeAction(tr("Find &Previous"),
                                      QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(m_findPreviousAction, &QAction::triggered, this, &MainWindow::findPrevious);

    auto *replaceAction = makeAction(tr("&Replace..."), QKeySequence(Qt::CTRL | Qt::Key_H));
    connect(replaceAction, &QAction::triggered, this, &MainWindow::showFindReplace);

    m_preferencesAction = makeAction(tr("&Preferences..."),
                                     QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::openPreferences);

    m_wordWrapAction = makeAction(tr("&Word Wrap"));
    m_wordWrapAction->setCheckable(true);
    connect(m_wordWrapAction, &QAction::toggled, this, &MainWindow::toggleWordWrap);

    m_lineNumbersAction = makeAction(tr("&Line Numbers"));
    m_lineNumbersAction->setCheckable(true);
    connect(m_lineNumbersAction, &QAction::toggled, this, &MainWindow::toggleLineNumbers);

    m_spellCheckAction = makeAction(tr("S&pell Check"), QKeySequence(Qt::Key_F7));
    m_spellCheckAction->setCheckable(true);
    connect(m_spellCheckAction, &QAction::toggled, this, &MainWindow::toggleSpellCheck);

    m_toggleFoldAction = makeAction(tr("Toggle &Fold"),
                                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Period));
    connect(m_toggleFoldAction, &QAction::triggered, this, &MainWindow::toggleFold);

    m_foldAllAction = makeAction(tr("Fold &All"));
    connect(m_foldAllAction, &QAction::triggered, this, &MainWindow::foldAll);

    m_unfoldAllAction = makeAction(tr("&Unfold All"));
    connect(m_unfoldAllAction, &QAction::triggered, this, &MainWindow::unfoldAll);

    m_zoomInAction = makeAction(tr("Zoom &In"), QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    m_zoomOutAction = makeAction(tr("Zoom &Out"), QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    m_zoomResetAction = makeAction(tr("&Reset Zoom"), QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::zoomReset);

    auto *fontAction = makeAction(tr("&Font..."));
    connect(fontAction, &QAction::triggered, this, &MainWindow::chooseFont);

    auto *aboutAction = makeAction(tr("&About Papiro"));
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this, tr("About Papiro"),
            tr("<h3>Papiro %1</h3>"
               "<p>A focused, cross-platform text editor.</p>"
               "<p>Independent implementation written from scratch. Not affiliated with, "
               "endorsed by, or containing code or assets from any commercial editor.</p>"
               "<p>Built with Qt %2.</p>")
                .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion())));
    });

    auto *aboutQtAction = makeAction(tr("About &Qt"));
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);

    // Keyboard Shortcuts
    m_keyboardShortcutsAction = makeAction(tr("&Keyboard Shortcuts..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_K));
    connect(m_keyboardShortcutsAction, &QAction::triggered, this, &MainWindow::showKeyboardShortcuts);

    // Macro actions
    m_recordMacroAction = makeAction(tr("Start &Recording"), QKeySequence(Qt::CTRL | Qt::Key_F9));
    m_recordMacroAction->setShortcutVisibleInContextMenu(true);
    connect(m_recordMacroAction, &QAction::triggered, this, &MainWindow::startMacroRecording);

    m_stopRecordMacroAction = makeAction(tr("Stop Recording"), QKeySequence(Qt::CTRL | Qt::Key_F10));
    m_stopRecordMacroAction->setEnabled(false);
    m_stopRecordMacroAction->setShortcutVisibleInContextMenu(true);
    connect(m_stopRecordMacroAction, &QAction::triggered, this, &MainWindow::stopMacroRecording);

    m_playMacroAction = makeAction(tr("&Play Macro"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9));
    m_playMacroAction->setShortcutVisibleInContextMenu(true);
    connect(m_playMacroAction, &QAction::triggered, this, &MainWindow::playMacro);

    m_manageMacrosAction = makeAction(tr("&Manage Macros..."));
    connect(m_manageMacrosAction, &QAction::triggered, this, &MainWindow::manageMacros);

    // Built-in macro slots (Ctrl+Shift+1..9)
    for (int i = 0; i < MacroManager::MaxBuiltinMacros; ++i) {
        const int slotNum = i + 1;
        m_macroSlotActions[i] = makeAction(tr("Macro %1").arg(slotNum),
                                           QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1 + i));
        m_macroSlotActions[i]->setData(i);
        connect(m_macroSlotActions[i], &QAction::triggered, this, [this, i] {
            playBuiltinMacro(i);
        });
    }

    // File Explorer
    m_openFolderAction = makeAction(tr("Open F&older..."),
                                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(m_openFolderAction, &QAction::triggered, this, &MainWindow::openFolder);

    m_toggleFileExplorerAction = makeAction(tr("File E&xplorer"),
                                            QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E));
    m_toggleFileExplorerAction->setCheckable(true);
    m_toggleFileExplorerAction->setChecked(true);
    connect(m_toggleFileExplorerAction, &QAction::toggled, this, &MainWindow::toggleFileExplorer);

    // External tools / output
    m_configureToolsAction = makeAction(tr("&Configure Tools..."));
    connect(m_configureToolsAction, &QAction::triggered, this, &MainWindow::configureTools);

    m_toggleOutputAction = makeAction(tr("&Output"),
                                      QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O));
    m_toggleOutputAction->setCheckable(true);
    m_toggleOutputAction->setChecked(false);
    connect(m_toggleOutputAction, &QAction::toggled, this, &MainWindow::toggleOutput);

    m_toggleSearchResultsAction = makeAction(tr("Search &Results"),
                                             QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));
    m_toggleSearchResultsAction->setCheckable(true);
    m_toggleSearchResultsAction->setChecked(false);
    connect(m_toggleSearchResultsAction, &QAction::toggled, this,
            &MainWindow::toggleSearchResults);

    // Workspace
    m_openWorkspaceAction = makeAction(tr("Open &Workspace..."),
                                       QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(m_openWorkspaceAction, &QAction::triggered, this, &MainWindow::openWorkspace);

    m_saveWorkspaceAction = makeAction(tr("Save Workspace &As..."),
                                       QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(m_saveWorkspaceAction, &QAction::triggered, this, &MainWindow::saveWorkspaceAs);

    // Session
    m_restoreSessionAction = makeAction(tr("Restore Previous Session"));
    m_restoreSessionAction->setCheckable(true);
    {
        QSettings sessionSettings;
        m_restoreSessionAction->setChecked(
            sessionSettings.value(QStringLiteral("session/restore"), true).toBool());
    }
    connect(m_restoreSessionAction, &QAction::toggled, this, [](bool enabled) {
        QSettings sessionSettings;
        sessionSettings.setValue(QStringLiteral("session/restore"), enabled);
    });

    // Print
    m_printAction = makeAction(tr("&Print..."), QKeySequence::Print);
    connect(m_printAction, &QAction::triggered, this, &MainWindow::printDocument);

    m_printPreviewAction = makeAction(tr("Print Pre&view..."));
    connect(m_printPreviewAction, &QAction::triggered, this, &MainWindow::printPreview);

    // Clip Library
    m_toggleClipLibraryAction = makeAction(tr("Clip &Library"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));
    m_toggleClipLibraryAction->setCheckable(true);
    m_toggleClipLibraryAction->setChecked(true);
    connect(m_toggleClipLibraryAction, &QAction::toggled, this, &MainWindow::toggleClipLibrary);

    // Git actions
    m_gitCommitAction = makeAction(tr("&Commit..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C),
                                   themedIcon(QStringLiteral("vcs-commit"), QStyle::SP_DialogApplyButton, this));
    connect(m_gitCommitAction, &QAction::triggered, this, &MainWindow::gitCommit);

    m_gitPushAction = makeAction(tr("&Push"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P),
                                 themedIcon(QStringLiteral("vcs-push"), QStyle::SP_ArrowUp, this));
    connect(m_gitPushAction, &QAction::triggered, this, &MainWindow::gitPush);

    m_gitPullAction = makeAction(tr("Pul&l"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L),
                                 themedIcon(QStringLiteral("vcs-pull"), QStyle::SP_ArrowDown, this));
    connect(m_gitPullAction, &QAction::triggered, this, &MainWindow::gitPull);

    m_gitBranchAction = makeAction(tr("&Branch..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B),
                                   themedIcon(QStringLiteral("vcs-branch"), QStyle::SP_DirIcon, this));
    connect(m_gitBranchAction, &QAction::triggered, this, &MainWindow::gitBranch);

    m_gitLogAction = makeAction(tr("&Log..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G),
                                themedIcon(QStringLiteral("vcs-log"), QStyle::SP_FileDialogListView, this));
    connect(m_gitLogAction, &QAction::triggered, this, &MainWindow::gitLog);

    m_gitBlameAction = makeAction(tr("&Blame"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B),
                                  themedIcon(QStringLiteral("vcs-annotate"), QStyle::SP_FileDialogDetailedView, this));
    connect(m_gitBlameAction, &QAction::triggered, this, &MainWindow::gitBlame);

    m_gitDiffAction = makeAction(tr("&Diff"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D),
                                 themedIcon(QStringLiteral("vcs-diff"), QStyle::SP_FileDialogContentsView, this));
    connect(m_gitDiffAction, &QAction::triggered, this, &MainWindow::gitDiff);

    m_gitToggleAction = makeAction(tr("&Git"), QKeySequence());
    m_gitToggleAction->setCheckable(true);
    m_gitToggleAction->setChecked(false);

    m_toggleGitDiffAction = makeAction(tr("Diff View"), QKeySequence(),
                                       themedIcon(QStringLiteral("vcs-diff"), QStyle::SP_FileDialogContentsView, this));
    m_toggleGitDiffAction->setCheckable(true);
    m_toggleGitDiffAction->setChecked(false);
    connect(m_toggleGitDiffAction, &QAction::toggled, this, [this](bool visible) {
        if (m_gitDiffDock)
            m_gitDiffDock->setVisible(visible);
    });

    // Language selection
    m_languageGroup = new QActionGroup(this);
    m_languageGroup->setExclusive(true);
    const struct {
        const char *label;
        const char *locale;
    } languages[] = {
        {"English", "en"},
        {"Português", "pt"},
        {"Español", "es"},
    };
    QSettings langSettings;
    const QString currentLang = langSettings.value(QStringLiteral("ui/language")).toString();
    for (const auto &entry : languages) {
        auto *action = new QAction(QString::fromUtf8(entry.label), this);
        action->setCheckable(true);
        action->setData(QString::fromLatin1(entry.locale));
        if (currentLang == QLatin1String(entry.locale))
            action->setChecked(true);
        m_languageGroup->addAction(action);
    }
    connect(m_languageGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        setLanguage(action->data().toString());
    });

    // Theme selection
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    const struct {
        const char *label;
        Theme::Mode mode;
    } themes[] = {
        {QT_TRANSLATE_NOOP("papiro::MainWindow", "&Light"), Theme::Mode::Light},
        {QT_TRANSLATE_NOOP("papiro::MainWindow", "&Dark"), Theme::Mode::Dark},
    };
    QSettings themeSettings;
    const Theme::Mode currentTheme = Theme::fromKey(
        themeSettings.value(QStringLiteral("ui/theme")).toString());
    for (const auto &entry : themes) {
        auto *action = new QAction(tr(entry.label), this);
        action->setCheckable(true);
        action->setData(Theme::key(entry.mode));
        if (entry.mode == currentTheme)
            action->setChecked(true);
        m_themeGroup->addAction(action);
    }
    connect(m_themeGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        setTheme(action->data().toString());
    });

    // File
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addAction(m_openFolderAction);
    m_recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    m_workspaceMenu = fileMenu->addMenu(tr("Recent &Workspaces"));
    fileMenu->addAction(m_restoreSessionAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_openWorkspaceAction);
    fileMenu->addAction(m_saveWorkspaceAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addAction(m_saveAllAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_printAction);
    fileMenu->addAction(m_printPreviewAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_closeAction);
    fileMenu->addAction(exitAction);

    // Edit
    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addSeparator();
    editMenu->addAction(selectAllAction);
    editMenu->addAction(duplicateAction);
    editMenu->addAction(deleteLineAction);
    editMenu->addSeparator();
    editMenu->addAction(gotoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_preferencesAction);

    // Search
    auto *searchMenu = menuBar()->addMenu(tr("&Search"));
    searchMenu->addAction(m_findAction);
    searchMenu->addAction(m_findNextAction);
    searchMenu->addAction(m_findPreviousAction);
    searchMenu->addAction(replaceAction);

    // Document
    auto *documentMenu = menuBar()->addMenu(tr("&Document"));

    m_encodingMenu = documentMenu->addMenu(tr("&Encoding"));
    m_encodingGroup = new QActionGroup(this);
    m_encodingGroup->setExclusive(true);
    const struct {
        TextEncoding encoding;
        const char *label;
    } encodings[] = {
        {TextEncoding::Utf8, "UTF-8"},
        {TextEncoding::Utf8Bom, "UTF-8 with BOM"},
        {TextEncoding::Utf16LE, "UTF-16 LE"},
        {TextEncoding::Utf16BE, "UTF-16 BE"},
        {TextEncoding::Windows1252, "Windows-1252"},
    };
    for (const auto &entry : encodings) {
        auto *action = m_encodingMenu->addAction(QString::fromLatin1(entry.label));
        action->setCheckable(true);
        action->setData(static_cast<int>(entry.encoding));
        m_encodingGroup->addAction(action);
    }
    connect(m_encodingGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        setEncoding(static_cast<TextEncoding>(action->data().toInt()));
    });

    m_eolMenu = documentMenu->addMenu(tr("Line &Endings"));
    m_eolGroup = new QActionGroup(this);
    m_eolGroup->setExclusive(true);
    const struct {
        LineEnding eol;
        const char *label;
    } endings[] = {
        {LineEnding::Lf, "LF (Unix)"},
        {LineEnding::CrLf, "CRLF (Windows)"},
        {LineEnding::Cr, "CR (Classic Mac)"},
    };
    for (const auto &entry : endings) {
        auto *action = m_eolMenu->addAction(QString::fromLatin1(entry.label));
        action->setCheckable(true);
        action->setData(static_cast<int>(entry.eol));
        m_eolGroup->addAction(action);
    }
    connect(m_eolGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        setLineEnding(static_cast<LineEnding>(action->data().toInt()));
    });

    connect(m_encodingMenu, &QMenu::aboutToShow, this, &MainWindow::updateDocumentMenuState);
    connect(m_eolMenu, &QMenu::aboutToShow, this, &MainWindow::updateDocumentMenuState);

    auto *foldingMenu = documentMenu->addMenu(tr("&Folding"));
    foldingMenu->addAction(m_toggleFoldAction);
    foldingMenu->addAction(m_foldAllAction);
    foldingMenu->addAction(m_unfoldAllAction);

    // View
    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_wordWrapAction);
    viewMenu->addAction(m_lineNumbersAction);
    viewMenu->addAction(m_spellCheckAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);
    viewMenu->addSeparator();
    viewMenu->addAction(fontAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_toggleFileExplorerAction);
    viewMenu->addAction(m_toggleClipLibraryAction);
    viewMenu->addAction(m_toggleOutputAction);
    viewMenu->addAction(m_toggleSearchResultsAction);
    m_languageMenu = viewMenu->addMenu(tr("&Language"));
    m_languageMenu->addActions(m_languageGroup->actions());
    m_themeMenu = viewMenu->addMenu(tr("&Theme"));
    m_themeMenu->addActions(m_themeGroup->actions());

    // Macros
    m_macroMenu = menuBar()->addMenu(tr("&Macros"));
    m_macroMenu->addAction(m_recordMacroAction);
    m_macroMenu->addAction(m_stopRecordMacroAction);
    m_macroMenu->addSeparator();
    m_macroMenu->addAction(m_playMacroAction);
    m_macroMenu->addSeparator();
    m_macroSlotMenu = m_macroMenu->addMenu(tr("Macro &Slots"));
    for (int i = 0; i < MacroManager::MaxBuiltinMacros; ++i)
        m_macroSlotMenu->addAction(m_macroSlotActions[i]);
    m_macroMenu->addSeparator();
    m_macroMenu->addAction(m_manageMacrosAction);

    // Tools
    m_toolsMenu = menuBar()->addMenu(tr("&Tools"));
    m_toolsMenu->addAction(m_configureToolsAction);

    // Git
    m_gitMenu = menuBar()->addMenu(tr("&Git"));
    m_gitMenu->addAction(m_gitCommitAction);
    m_gitMenu->addAction(m_gitPushAction);
    m_gitMenu->addAction(m_gitPullAction);
    m_gitMenu->addSeparator();
    m_gitMenu->addAction(m_gitBranchAction);
    m_gitMenu->addAction(m_gitLogAction);
    m_gitMenu->addSeparator();
    m_gitMenu->addAction(m_gitDiffAction);
    m_gitMenu->addAction(m_gitBlameAction);
    m_gitMenu->addSeparator();
    m_gitMenu->addAction(m_toggleGitDiffAction);

    // Help
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_keyboardShortcutsAction);
    helpMenu->addSeparator();
    helpMenu->addAction(aboutAction);
    helpMenu->addAction(aboutQtAction);

    // Main toolbar
    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName(QStringLiteral("MainToolbar"));
    toolbar->setMovable(false);
    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addAction(m_saveAction);
    toolbar->addSeparator();
    toolbar->addAction(m_undoAction);
    toolbar->addAction(m_redoAction);
    toolbar->addSeparator();
    toolbar->addAction(m_findAction);
    toolbar->addSeparator();
    toolbar->addAction(m_recordMacroAction);
    toolbar->addAction(m_stopRecordMacroAction);
    toolbar->addAction(m_playMacroAction);
}

void MainWindow::createStatusBar()
{
    m_positionLabel = new QLabel(tr("Ln 1, Col 1"), this);
    m_selectionLabel = new QLabel(this);
    m_encodingLabel = new QLabel(this);
    m_eolLabel = new QLabel(this);
    m_syntaxLabel = new QLabel(this);

    m_positionLabel->setMinimumWidth(110);
    m_selectionLabel->setMinimumWidth(90);
    m_encodingLabel->setMinimumWidth(90);
    m_eolLabel->setMinimumWidth(50);
    m_syntaxLabel->setMinimumWidth(90);

    statusBar()->addWidget(m_positionLabel);
    statusBar()->addWidget(m_selectionLabel, 1);
    statusBar()->addPermanentWidget(m_syntaxLabel);
    statusBar()->addPermanentWidget(m_eolLabel);
    statusBar()->addPermanentWidget(m_encodingLabel);

    // Git status bar widgets (right side, before encoding/eol/syntax)
    m_gitBranchLabel = new QLabel(this);
    m_gitBranchLabel->setMinimumWidth(120);
    m_gitBranchLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_gitBranchLabel->setTextFormat(Qt::RichText);
    m_gitBranchLabel->setToolTip(tr("Git branch"));
    m_gitBranchLabel->hide();

    m_gitDirtyIndicator = new QLabel(this);
    m_gitDirtyIndicator->setFixedWidth(16);
    m_gitDirtyIndicator->setAlignment(Qt::AlignCenter);
    m_gitDirtyIndicator->setToolTip(tr("Repository has uncommitted changes"));
    m_gitDirtyIndicator->hide();

    // Insert before the encoding label (permanent widgets are added right-to-left)
    statusBar()->insertPermanentWidget(0, m_gitDirtyIndicator);
    statusBar()->insertPermanentWidget(0, m_gitBranchLabel);
}

// ---------------------------------------------------------------------------
// Documents and tabs
// ---------------------------------------------------------------------------

Document *MainWindow::currentDocument() const
{
    return qobject_cast<Document *>(m_tabs->currentWidget());
}

Document *MainWindow::addDocument()
{
    auto *document = new Document(m_tabs);
    const int index = m_tabs->addTab(document, document->displayName());
    m_tabs->setCurrentIndex(index);

    Editor *editor = document->editor();
    editor->setBaseFont(m_editorFont);
    editor->setLineNumbersVisible(m_lineNumbers);
    editor->setLineWrapMode(m_wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    editor->setMacroManager(m_macroManager);
    editor->setSpellChecker(m_spellCheck ? m_spellChecker : nullptr);

    connect(editor, &Editor::findRequested, this, &MainWindow::showFindReplace);
    connect(editor, &Editor::gotoLineRequested, this, &MainWindow::goToLine);
    connect(editor, &Editor::blockSelectionChanged, this, [this, document] {
        if (document == currentDocument())
            updateActions();
    });

    connect(document, &Document::modificationChanged, this, [this, document](bool) {
        refreshTabTitle(document);
        if (document == currentDocument()) {
            updateWindowTitle();
            updateActions();
            updateGitStatus();
        }
    });
    connect(document, &Document::pathChanged, this, [this, document](const QString &) {
        refreshTabTitle(document);
        if (document == currentDocument()) {
            updateWindowTitle();
            updateStatus();
        }
    });
    connect(document, &Document::gitBlameReady, this, [this, editor](const QVector<GitBlameLine>& blameLines) {
        editor->setBlame(blameLines);
        editor->toggleBlame();
    });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this, document] {
        if (document == currentDocument())
            updateStatus();
    });
    connect(editor, &QPlainTextEdit::copyAvailable, this, [this, document](bool) {
        if (document == currentDocument())
            updateActions();
    });
    connect(editor->document(), &QTextDocument::undoAvailable, this, [this, document](bool) {
        if (document == currentDocument())
            updateActions();
    });
    connect(editor->document(), &QTextDocument::redoAvailable, this, [this, document](bool) {
        if (document == currentDocument())
            updateActions();
    });

    return document;
}

void MainWindow::newDocument()
{
    addDocument()->editor()->setFocus();
    updateWindowTitle();
    updateStatus();
    updateActions();
    updateGitStatus();
}

void MainWindow::openFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Open File"), QString(),
        tr("Text Files (*.txt *.log *.md *.syn *.ini *.json *.xml *.csv);;All Files (*)"));
    for (const QString &file : files)
        openFile(file);
}

void MainWindow::openFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, tr("Open File"),
                             tr("Cannot open \"%1\": file does not exist.").arg(path));
        return;
    }

    const QString absolute = info.absoluteFilePath();
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *existing = qobject_cast<Document *>(m_tabs->widget(i));
        if (existing && existing->filePath() == absolute) {
            m_tabs->setCurrentIndex(i);
            existing->editor()->setFocus();
            return;
        }
    }

    Document *document = currentDocument();
    const bool reuse = document && document->filePath().isEmpty() && !document->isModified()
        && document->editor()->document()->isEmpty();
    if (!reuse)
        document = addDocument();

    QString error;
    if (!document->loadFile(absolute, &error)) {
        QMessageBox::warning(this, tr("Open File"), error);
        if (!reuse) {
            const int index = m_tabs->indexOf(document);
            if (index >= 0) {
                m_tabs->removeTab(index);
                document->deleteLater();
            }
        }
        return;
    }

    updateRecentFiles(absolute);
    refreshTabTitle(document);
    document->editor()->setFocus();
    updateWindowTitle();
    updateStatus();
    updateActions();
    updateDocumentMenuState();
    updateGitStatus();
}

void MainWindow::refreshTabTitle(Document *document)
{
    if (!document)
        return;
    const int index = m_tabs->indexOf(document);
    if (index < 0)
        return;
    const QString title = document->displayName() + (document->isModified() ? QStringLiteral(" *")
                                                                           : QString());
    m_tabs->setTabText(index, title);
    m_tabs->setTabToolTip(index, document->filePath().isEmpty()
                                   ? tr("Not saved yet")
                                   : QDir::toNativeSeparators(document->filePath()));
}

bool MainWindow::maybeSave(Document *document)
{
    if (!document || !document->isModified())
        return true;

    m_tabs->setCurrentWidget(document);
    QMessageBox box(QMessageBox::Warning, tr("Unsaved Changes"),
                    tr("\"%1\" has unsaved changes.\n\nDo you want to save them?")
                        .arg(document->displayName()),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setDefaultButton(QMessageBox::Save);
    const int result = box.exec();
    if (result == QMessageBox::Cancel)
        return false;
    if (result == QMessageBox::Discard)
        return true;
    return saveDocument(document, false);
}

bool MainWindow::saveDocument(Document *document, bool saveAs)
{
    if (!document)
        return false;

    QString path = document->filePath();
    if (saveAs || path.isEmpty()) {
        const QString suggested = path.isEmpty() ? document->displayName() : path;
        path = QFileDialog::getSaveFileName(
            this, tr("Save File"), suggested,
            tr("Text Files (*.txt);;All Files (*)"));
        if (path.isEmpty())
            return false;
    }

    QString error;
    if (!document->saveFile(path, &error)) {
        QMessageBox::warning(this, tr("Save File"), error);
        return false;
    }

    document->applySyntaxFor(path);
    updateRecentFiles(document->filePath());
    refreshTabTitle(document);
    updateWindowTitle();
    updateStatus();
    updateDocumentMenuState();
    return true;
}

void MainWindow::saveCurrentDocument()
{
    saveDocument(currentDocument(), false);
}

void MainWindow::saveCurrentDocumentAs()
{
    saveDocument(currentDocument(), true);
}

void MainWindow::saveAllDocuments()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (document && document->isModified()) {
            if (!saveDocument(document, false))
                return;
        }
    }
}

void MainWindow::closeCurrentTab()
{
    closeTab(m_tabs->currentIndex());
}

void MainWindow::closeTab(int index)
{
    auto *document = qobject_cast<Document *>(m_tabs->widget(index));
    if (!document)
        return;
    if (!maybeSave(document))
        return;

    m_tabs->removeTab(index);
    document->deleteLater();
    clearSearchResults();
    updateWindowTitle();
    updateStatus();
    updateActions();
    updateDocumentMenuState();
    updateGitStatus();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (!maybeSave(document)) {
            event->ignore();
            return;
        }
    }
    saveSettings();
    unregisterAllCustomShortcuts();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            openFile(url.toLocalFile());
    }
    event->acceptProposedAction();
}

// ---------------------------------------------------------------------------
// UI state
// ---------------------------------------------------------------------------

void MainWindow::updateWindowTitle()
{
    QString suffix;
    if (!m_workspacePath.isEmpty())
        suffix = QStringLiteral(" — ") + QFileInfo(m_workspacePath).completeBaseName();

    Document *document = currentDocument();
    if (!document) {
        setWindowTitle(tr("Papiro") + suffix);
        setWindowModified(false);
        return;
    }
    setWindowTitle(tr("%1[*] - Papiro").arg(document->displayName()) + suffix);
    setWindowModified(document->isModified());
}

void MainWindow::updateStatus()
{
    Document *document = currentDocument();
    if (!document) {
        m_positionLabel->clear();
        m_selectionLabel->clear();
        m_encodingLabel->clear();
        m_eolLabel->clear();
        m_syntaxLabel->clear();
        return;
    }

    Editor *editor = document->editor();
    const QTextCursor cursor = editor->textCursor();
    const int selected = cursor.selectedText().size();

    m_positionLabel->setText(tr("Ln %1, Col %2").arg(editor->currentLine()).arg(editor->currentColumn()));
    m_selectionLabel->setText(selected > 0 ? tr("Sel %1").arg(selected) : QString());
    m_encodingLabel->setText(encodingName(document->encoding()));
    m_eolLabel->setText(lineEndingName(document->lineEnding()));

    const QString fileName = document->filePath();
    const SyntaxDefinition *definition = fileName.isEmpty()
        ? nullptr
        : SyntaxRepository::instance().forFile(fileName);
    m_syntaxLabel->setText(definition ? definition->name : tr("Plain Text"));
}

void MainWindow::updateActions()
{
    Document *document = currentDocument();
    Editor *editor = document ? document->editor() : nullptr;
    const bool hasDocument = document != nullptr;

    m_saveAction->setEnabled(hasDocument);
    m_saveAsAction->setEnabled(hasDocument);
    m_saveAllAction->setEnabled(hasDocument);
    m_closeAction->setEnabled(hasDocument);
    m_findAction->setEnabled(hasDocument);
    m_findNextAction->setEnabled(hasDocument);
    m_findPreviousAction->setEnabled(hasDocument);
    m_zoomInAction->setEnabled(hasDocument);
    m_zoomOutAction->setEnabled(hasDocument);
    m_zoomResetAction->setEnabled(hasDocument);

    m_undoAction->setEnabled(hasDocument && editor->document()->isUndoAvailable());
    m_redoAction->setEnabled(hasDocument && editor->document()->isRedoAvailable());
    const bool hasSelection = hasDocument
        && (editor->textCursor().hasSelection() || editor->hasBlockSelection());
    m_cutAction->setEnabled(hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_pasteAction->setEnabled(hasDocument && editor->canPaste());
}

void MainWindow::updateDocumentMenuState()
{
    Document *document = currentDocument();
    const int encoding = document ? static_cast<int>(document->encoding()) : -1;
    const int eol = document ? static_cast<int>(document->lineEnding()) : -1;

    for (QAction *action : m_encodingGroup->actions())
        action->setChecked(action->data().toInt() == encoding);
    for (QAction *action : m_eolGroup->actions())
        action->setChecked(action->data().toInt() == eol);

    m_encodingGroup->setEnabled(document != nullptr);
    m_eolGroup->setEnabled(document != nullptr);
}

// ---------------------------------------------------------------------------
// Search and replace
// ---------------------------------------------------------------------------

QRegularExpression MainWindow::buildRegex(const FindRequest &request, QString *error) const
{
    if (request.pattern.isEmpty()) {
        if (error)
            *error = tr("The search text is empty.");
        return QRegularExpression();
    }

    QString pattern = request.regex ? request.pattern : QRegularExpression::escape(request.pattern);
    if (request.wholeWord)
        pattern = QStringLiteral("\\b(?:%1)\\b").arg(pattern);

    QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;
    if (!request.matchCase)
        options |= QRegularExpression::CaseInsensitiveOption;

    QRegularExpression expression(pattern, options);
    if (error)
        *error = expression.errorString();
    return expression;
}

void MainWindow::findInDocument(bool backward)
{
    Document *document = currentDocument();
    if (!document)
        return;

    if (m_lastFind.pattern.isEmpty()) {
        showFindReplace();
        return;
    }

    QString error;
    const QRegularExpression expression = buildRegex(m_lastFind, &error);
    if (!expression.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(error), 6000);
        return;
    }

    Editor *editor = document->editor();
    QTextDocument *textDocument = editor->document();
    const QTextCursor cursor = editor->textCursor();
    const int start = backward ? cursor.selectionStart() : cursor.selectionEnd();
    const QTextDocument::FindFlags findFlags = backward
        ? QTextDocument::FindFlags(QTextDocument::FindBackward)
        : QTextDocument::FindFlags();

    QTextCursor found = textDocument->find(expression, start, findFlags);
    if (found.isNull() && m_lastFind.wrap) {
        const int wrapPosition = backward ? textDocument->characterCount() : 0;
        found = textDocument->find(expression, wrapPosition, findFlags);
    }

    if (found.isNull()) {
        statusBar()->showMessage(tr("Not found: \"%1\"").arg(m_lastFind.pattern), 5000);
        return;
    }

    editor->setTextCursor(found);
    editor->ensureCursorVisible();
    statusBar()->showMessage(tr("Match found"), 2000);
}

void MainWindow::findAll(const FindRequest &request)
{
    if (!m_searchResultsView)
        return;

    QString error;
    const QRegularExpression expression = buildRegex(request, &error);
    if (!expression.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(error), 6000);
        return;
    }

    m_searchResultsView->clear();

    constexpr int MaxResults = 5000;
    int total = 0;
    int documentCount = 0;
    bool truncated = false;

    for (int i = 0; i < m_tabs->count() && !truncated; ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (!document)
            continue;

        QTextDocument *textDocument = document->editor()->document();
        QTreeWidgetItem *root = nullptr;
        int position = 0;

        while (!truncated) {
            const QTextCursor found = textDocument->find(expression, position);
            if (found.isNull())
                break;

            if (!found.hasSelection()) {
                // Zero-length match: move forward to avoid an endless loop.
                const int next = found.position() + 1;
                if (next <= position)
                    break;
                position = next;
                continue;
            }

            if (!root) {
                root = new QTreeWidgetItem(m_searchResultsView);
                root->setFirstColumnSpanned(true);
                QFont font = root->font(0);
                font.setBold(true);
                root->setFont(0, font);
                root->setToolTip(0, document->displayPath());
                ++documentCount;
            }

            QString preview = found.block().text().trimmed();
            if (preview.size() > 160)
                preview = preview.left(157) + QStringLiteral("...");

            auto *item = new QTreeWidgetItem(root);
            item->setText(0, QString::number(found.blockNumber() + 1));
            item->setText(1, QString::number(found.positionInBlock() + 1));
            item->setText(2, preview);
            item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void *>(document)));
            item->setData(0, Qt::UserRole + 1, found.selectionStart());
            item->setData(0, Qt::UserRole + 2, document->filePath());

            position = found.selectionEnd();
            ++total;
            if (total >= MaxResults)
                truncated = true;
        }

        if (root)
            root->setText(0, tr("%1 (%2)").arg(document->displayName()).arg(root->childCount()));
    }

    if (total == 0) {
        auto *empty = new QTreeWidgetItem(m_searchResultsView);
        empty->setText(0, tr("No matches for \"%1\"").arg(request.pattern));
        empty->setFlags(empty->flags() & ~Qt::ItemIsEnabled);
        statusBar()->showMessage(tr("No matches for \"%1\".").arg(request.pattern), 5000);
    } else {
        QString message = tr("%n match(es) found in %1 document(s).", "", total)
                              .arg(documentCount);
        if (truncated)
            message += QLatin1Char(' ') + tr("Showing the first %1.").arg(total);
        statusBar()->showMessage(message, 6000);
    }

    if (total <= 500)
        m_searchResultsView->expandAll();

    if (m_searchResultsDock) {
        m_searchResultsDock->show();
        m_searchResultsDock->raise();
    }
    if (m_toggleSearchResultsAction)
        m_toggleSearchResultsAction->setChecked(true);
}

void MainWindow::findAllInFolder(const FindRequest &request, const QString &folder)
{
    if (!m_searchResultsView)
        return;

    const QFileInfo folderInfo(folder);
    if (!folderInfo.isDir()) {
        statusBar()->showMessage(tr("Invalid search folder: %1").arg(folder), 6000);
        return;
    }

    QString error;
    const QRegularExpression expression = buildRegex(request, &error);
    if (!expression.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(error), 6000);
        return;
    }

    m_searchResultsView->clear();

    constexpr int MaxResults = 5000;
    constexpr int MaxFiles = 5000;
    constexpr qint64 MaxFileSize = 5 * 1024 * 1024;

    int total = 0;
    int fileCount = 0;
    bool truncated = false;
    bool filesTruncated = false;

    QStringList files;
    QDirIterator iterator(folderInfo.absoluteFilePath(),
                          QDir::Files | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        if (info.size() > MaxFileSize)
            continue;
        files.append(path);
        if (files.size() >= MaxFiles) {
            filesTruncated = true;
            break;
        }
    }
    files.sort();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (const QString &path : files) {
        if (total >= MaxResults) {
            truncated = true;
            break;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = file.read(MaxFileSize + 1);
        if (data.size() > MaxFileSize || looksLikeBinary(data))
            continue;

        const QString text = decodeFile(data).text;
        if (text.isEmpty())
            continue;

        QTreeWidgetItem *root = nullptr;
        const QString relativePath = QDir(folderInfo.absoluteFilePath()).relativeFilePath(path);
        int lineStart = 0;
        int lineNumber = 1;

        while (lineStart <= text.size()) {
            const int lineEnd = text.indexOf(QLatin1Char('\n'), lineStart);
            const QString line = lineEnd < 0 ? text.mid(lineStart)
                                             : text.mid(lineStart, lineEnd - lineStart);

            QRegularExpressionMatchIterator matches = expression.globalMatch(line);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                if (match.capturedLength() == 0)
                    continue;

                if (!root) {
                    root = new QTreeWidgetItem(m_searchResultsView);
                    root->setFirstColumnSpanned(true);
                    QFont font = root->font(0);
                    font.setBold(true);
                    root->setFont(0, font);
                    root->setText(0, relativePath);
                    root->setToolTip(0, displayPath(path));
                    ++fileCount;
                }

                QString preview = line.trimmed();
                if (preview.size() > 160)
                    preview = preview.left(157) + QStringLiteral("...");

                auto *item = new QTreeWidgetItem(root);
                item->setText(0, QString::number(lineNumber));
                item->setText(1, QString::number(match.capturedStart() + 1));
                item->setText(2, preview);
                item->setData(0, Qt::UserRole, QVariant());
                item->setData(0, Qt::UserRole + 1, lineStart + match.capturedStart());
                item->setData(0, Qt::UserRole + 2, displayPath(path));

                ++total;
                if (total >= MaxResults) {
                    truncated = true;
                    break;
                }
            }

            if (truncated)
                break;
            if (lineEnd < 0)
                break;
            lineStart = lineEnd + 1;
            ++lineNumber;
        }

        if (root) {
            root->setText(0, tr("%1 (%2)").arg(relativePath).arg(root->childCount()));
        }
    }
    QApplication::restoreOverrideCursor();

    if (total == 0) {
        auto *empty = new QTreeWidgetItem(m_searchResultsView);
        empty->setText(0, tr("No matches for \"%1\" in %2")
                              .arg(request.pattern, QDir::toNativeSeparators(folder)));
        empty->setFlags(empty->flags() & ~Qt::ItemIsEnabled);
        statusBar()->showMessage(tr("No matches for \"%1\".").arg(request.pattern), 5000);
    } else {
        QString message = tr("%n match(es) found in %1 file(s).", "", total).arg(fileCount);
        if (truncated)
            message += QLatin1Char(' ') + tr("Showing the first %1.").arg(total);
        if (filesTruncated)
            message += QLatin1Char(' ') + tr("Only the first %1 files were scanned.").arg(MaxFiles);
        statusBar()->showMessage(message, 6000);
    }

    if (total <= 500)
        m_searchResultsView->expandAll();

    if (m_searchResultsDock) {
        m_searchResultsDock->show();
        m_searchResultsDock->raise();
    }
    if (m_toggleSearchResultsAction)
        m_toggleSearchResultsAction->setChecked(true);
}

void MainWindow::clearSearchResults()
{
    if (m_searchResultsView)
        m_searchResultsView->clear();
}

void MainWindow::activateSearchResult(QTreeWidgetItem *item, int)
{
    if (!item || !item->parent())
        return;

    auto *document = static_cast<Document *>(item->data(0, Qt::UserRole).value<void *>());
    if (!document || m_tabs->indexOf(document) < 0) {
        const QString path = item->data(0, Qt::UserRole + 2).toString();
        if (path.isEmpty())
            return;
        openFile(path);
        document = currentDocument();
        if (!document || document->filePath() != QFileInfo(path).absoluteFilePath())
            return;
    }

    const int index = m_tabs->indexOf(document);
    if (index < 0)
        return;

    m_tabs->setCurrentIndex(index);

    Editor *editor = document->editor();
    editor->clearBlockSelection();
    const int position = qBound(0, item->data(0, Qt::UserRole + 1).toInt(),
                                qMax(0, editor->document()->characterCount() - 1));
    QTextCursor cursor(editor->document());
    cursor.setPosition(position);
    editor->setTextCursor(cursor);
    editor->centerCursor();
    editor->setFocus();
}

void MainWindow::replaceCurrent()
{
    Document *document = currentDocument();
    if (!document)
        return;

    QString error;
    const QRegularExpression expression = buildRegex(m_lastFind, &error);
    if (!expression.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(error), 6000);
        return;
    }

    Editor *editor = document->editor();
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        findInDocument(false);
        return;
    }

    QString selected = cursor.selectedText();
    selected.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    const QRegularExpressionMatch match = expression.match(selected);
    if (!match.hasMatch() || match.capturedStart() != 0 || match.capturedLength() != selected.size()) {
        findInDocument(false);
        return;
    }

    const QString text = m_lastFind.regex ? expandReplacement(m_lastReplacement, match)
                                          : m_lastReplacement;
    cursor.insertText(text);
    editor->setTextCursor(cursor);
    findInDocument(false);
}

void MainWindow::replaceAll()
{
    Document *document = currentDocument();
    if (!document)
        return;

    QString error;
    const QRegularExpression expression = buildRegex(m_lastFind, &error);
    if (!expression.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(error), 6000);
        return;
    }

    Editor *editor = document->editor();
    QTextDocument *textDocument = editor->document();

    // Check for block/column selection first
    if (editor->hasBlockSelection()) {
        replaceAllInBlockSelection(editor, expression);
        return;
    }

    // Check for normal selection
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        replaceAllInSelection(editor, expression, cursor.selectionStart(), cursor.selectionEnd());
        return;
    }

    // No selection - search entire document (existing behavior)
    replaceAllInDocument(editor, expression);
}

void MainWindow::replaceAllInDocument(Editor *editor, const QRegularExpression &expression)
{
    QTextDocument *textDocument = editor->document();

    QTextCursor group(textDocument);
    group.beginEditBlock();

    int count = 0;
    QTextCursor search = textDocument->find(expression, 0);
    while (!search.isNull() && count < 1000000) {
        if (!search.hasSelection()) {
            QTextCursor next = search;
            next.movePosition(QTextCursor::NextCharacter);
            if (next.position() == search.position())
                break;
            search = textDocument->find(expression, next.position());
            continue;
        }

        QString selected = search.selectedText();
        selected.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
        const QRegularExpressionMatch match = expression.match(selected);
        const QString text = (m_lastFind.regex && match.hasMatch())
            ? expandReplacement(m_lastReplacement, match)
            : m_lastReplacement;

        search.insertText(text);
        ++count;
        search = textDocument->find(expression, search.position());
    }

    group.endEditBlock();
    statusBar()->showMessage(tr("%n replacement(s) made.", "", count), 5000);
}

void MainWindow::replaceAllInSelection(Editor *editor, const QRegularExpression &expression, int start, int end)
{
    QTextDocument *textDocument = editor->document();

    QTextCursor group(textDocument);
    group.beginEditBlock();

    int count = 0;
    int searchStart = start;
    int searchEnd = end;

    QTextCursor search = editor->document()->find(expression, searchStart);
    while (!search.isNull() && search.anchor() < searchEnd) {
        if (!search.hasSelection()) {
            QTextCursor next = search;
            next.movePosition(QTextCursor::NextCharacter);
            if (next.position() == search.position())
                break;
            search = textDocument->find(expression, next.position());
            continue;
        }

        // Ensure the match is within the selection range
        if (search.anchor() >= searchEnd || search.position() > searchEnd)
            break;

        QString selected = search.selectedText();
        selected.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
        const QRegularExpressionMatch match = expression.match(selected);
        const QString text = (m_lastFind.regex && match.hasMatch())
            ? expandReplacement(m_lastReplacement, match)
            : m_lastReplacement;

        search.insertText(text);
        ++count;
        // Continue searching from the end of the replacement
        search = textDocument->find(expression, search.position());
    }

    group.endEditBlock();
    statusBar()->showMessage(tr("%n replacement(s) made in selection.", "", count), 5000);
}

void MainWindow::replaceAllInBlockSelection(Editor *editor, const QRegularExpression &expression)
{
    QTextDocument *textDocument = editor->document();

    QTextCursor group(textDocument);
    group.beginEditBlock();

    int count = 0;

    // Get block selection boundaries
    const auto blockPoints = editor->getBlockSelectionPoints();
    if (!blockPoints.first.isValid() || !blockPoints.second.isValid()) {
        group.endEditBlock();
        return;
    }

    int topBlock = qMin(blockPoints.first.block, blockPoints.second.block);
    int bottomBlock = qMax(blockPoints.first.block, blockPoints.second.block);
    int leftColumn = qMin(blockPoints.first.column, blockPoints.second.column);
    int rightColumn = qMax(blockPoints.first.column, blockPoints.second.column);

    // Iterate through each line in the block selection
    for (int blockNum = topBlock; blockNum <= bottomBlock; ++blockNum) {
        QTextBlock block = editor->document()->findBlockByNumber(blockNum);
        if (!block.isValid())
            continue;

        QString lineText = block.text();
        if (lineText.isEmpty())
            continue;

        // Extract the column range for this line
        int lineLength = lineText.length();
        int colStart = qMin(leftColumn, lineLength);
        int colEnd = qMin(rightColumn, lineLength);
        if (colStart >= colEnd)
            continue;

        QString selectedText = lineText.mid(colStart, colEnd - colStart);
        if (selectedText.isEmpty())
            continue;

        // Search for matches within the selected column range
        QRegularExpressionMatchIterator it = expression.globalMatch(selectedText);
        QStringList replacements;
        QList<int> matchStarts, matchEnds;

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            matchStarts << match.capturedStart();
            matchEnds << match.capturedEnd();
            QString matched = match.captured(0);
            matched.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
            QString replacementText = (m_lastFind.regex && match.hasMatch())
                ? expandReplacement(m_lastReplacement, match)
                : m_lastReplacement;
            replacements << replacementText;
        }

        if (replacements.isEmpty())
            continue;

        // Build new line text with replacements (from right to left to maintain positions)
        QString newLine = lineText;
        for (int i = replacements.size() - 1; i >= 0; --i) {
            int absStart = colStart + matchStarts[i];
            int absEnd = colStart + matchEnds[i];
            newLine.replace(absStart, absEnd - absStart, replacements[i]);
            ++count;
        }

        // Apply the new line text
        QTextCursor cursor(textDocument);
        cursor.setPosition(block.position() + colStart);
        cursor.setPosition(block.position() + colEnd, QTextCursor::KeepAnchor);
        cursor.insertText(newLine.mid(colStart, colEnd - colStart));
    }

    group.endEditBlock();
    statusBar()->showMessage(tr("%n replacement(s) made in block selection.", "", count), 5000);
}

QString MainWindow::expandReplacement(const QString &replacement,
                                      const QRegularExpressionMatch &match) const
{
    QString result;
    result.reserve(replacement.size());
    for (int i = 0; i < replacement.size(); ++i) {
        const QChar ch = replacement.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < replacement.size()) {
            const QChar next = replacement.at(i + 1);
            if (next.isDigit()) {
                result += match.captured(next.digitValue());
                ++i;
                continue;
            }
            if (next == QLatin1Char('\\')) {
                result += QLatin1Char('\\');
                ++i;
                continue;
            }
            if (next == QLatin1Char('n')) {
                result += QLatin1Char('\n');
                ++i;
                continue;
            }
            if (next == QLatin1Char('t')) {
                result += QLatin1Char('\t');
                ++i;
                continue;
            }
        }
        result += ch;
    }
    return result;
}

void MainWindow::showFindReplace()
{
    if (!m_findDialog) {
        m_findDialog = new FindReplaceDialog(this);
        m_findDialog->setSearchOptions(m_searchMatchCase, m_searchWholeWord, m_searchRegex,
                                       m_searchWrap);
        connect(m_findDialog, &FindReplaceDialog::findNextRequested, this,
                [this](const FindRequest &request, bool backward) {
                    m_lastFind = request;
                    findInDocument(backward);
                });
        connect(m_findDialog, &FindReplaceDialog::replaceRequested, this,
                [this](const FindRequest &request, const QString &replacement) {
                    m_lastFind = request;
                    m_lastReplacement = replacement;
                    replaceCurrent();
                });
        connect(m_findDialog, &FindReplaceDialog::replaceAllRequested, this,
                [this](const FindRequest &request, const QString &replacement) {
                    m_lastFind = request;
                    m_lastReplacement = replacement;
                    replaceAll();
                });
        connect(m_findDialog, &FindReplaceDialog::findAllRequested, this,
                [this](const FindRequest &request) {
                    m_lastFind = request;
                    findAll(request);
                });
        connect(m_findDialog, &FindReplaceDialog::findAllInFolderRequested, this,
                [this](const FindRequest &request, const QString &folder) {
                    m_lastFind = request;
                    findAllInFolder(request, folder);
                });
    }

    if (Document *document = currentDocument()) {
        const QString selection = document->editor()->textCursor().selectedText();
        if (!selection.isEmpty() && !selection.contains(QChar::ParagraphSeparator))
            m_findDialog->setFindText(selection);
    }

    const QString currentFolder = m_findDialog->searchFolder();
    if (currentFolder.isEmpty() || !QFileInfo(currentFolder).isDir()) {
        QString defaultFolder;
        if (Document *document = currentDocument(); document && !document->filePath().isEmpty())
            defaultFolder = QFileInfo(document->filePath()).absolutePath();
        else if (m_fileExplorer && !m_fileExplorer->rootPath().isEmpty())
            defaultFolder = m_fileExplorer->rootPath();
        if (defaultFolder.isEmpty())
            defaultFolder = QDir::homePath();
        m_findDialog->setSearchFolder(defaultFolder);
    }

    m_findDialog->show();
    m_findDialog->raise();
    m_findDialog->activateWindow();
    m_findDialog->focusFindField();
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void MainWindow::goToLine()
{
    Document *document = currentDocument();
    if (!document)
        return;

    Editor *editor = document->editor();
    bool ok = false;
    const int line = QInputDialog::getInt(this, tr("Go to Line"), tr("Line number:"),
                                          editor->currentLine(), 1, qMax(1, editor->blockCount()), 1,
                                          &ok);
    if (ok)
        editor->gotoLine(line);
}

void MainWindow::chooseFont()
{
    bool ok = false;
    const QFont font = QFontDialog::getFont(&ok, m_editorFont, this, tr("Editor Font"));
    if (!ok)
        return;
    m_editorFont = font;
    applyFontToAll();
}

void MainWindow::openPreferences()
{
    Preferences current;
    current.editorFont = m_editorFont;
    current.theme = Theme::mode();
    current.restoreSession = m_restoreSessionAction->isChecked();
    current.wordWrap = m_wordWrap;
    current.lineNumbers = m_lineNumbers;
    current.spellCheck = m_spellCheck;
    current.searchMatchCase = m_searchMatchCase;
    current.searchWholeWord = m_searchWholeWord;
    current.searchRegex = m_searchRegex;
    current.searchWrap = m_searchWrap;

    PreferencesDialog dialog(current, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    applyPreferences(dialog.preferences());
}

void MainWindow::applyPreferences(const Preferences &preferences)
{
    if (preferences.editorFont != m_editorFont) {
        m_editorFont = preferences.editorFont;
        applyFontToAll();
    }

    if (preferences.wordWrap != m_wordWrap)
        m_wordWrapAction->setChecked(preferences.wordWrap);
    if (preferences.lineNumbers != m_lineNumbers)
        m_lineNumbersAction->setChecked(preferences.lineNumbers);
    if (preferences.spellCheck != m_spellCheck)
        m_spellCheckAction->setChecked(preferences.spellCheck);
    if (preferences.restoreSession != m_restoreSessionAction->isChecked())
        m_restoreSessionAction->setChecked(preferences.restoreSession);

    const QString themeKey = Theme::key(preferences.theme);
    if (themeKey != Theme::key(Theme::mode())) {
        setTheme(themeKey);
        for (QAction *action : m_themeGroup->actions()) {
            if (action->data().toString() == themeKey)
                action->setChecked(true);
        }
    }

    m_searchMatchCase = preferences.searchMatchCase;
    m_searchWholeWord = preferences.searchWholeWord;
    m_searchRegex = preferences.searchRegex;
    m_searchWrap = preferences.searchWrap;
    if (m_findDialog) {
        m_findDialog->setSearchOptions(m_searchMatchCase, m_searchWholeWord, m_searchRegex,
                                       m_searchWrap);
    }

    saveSettings();
}

void MainWindow::toggleWordWrap(bool enabled)
{
    m_wordWrap = enabled;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (document)
            document->editor()->setLineWrapMode(enabled ? QPlainTextEdit::WidgetWidth
                                                         : QPlainTextEdit::NoWrap);
    }
}

void MainWindow::toggleLineNumbers(bool enabled)
{
    m_lineNumbers = enabled;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (document)
            document->editor()->setLineNumbersVisible(enabled);
    }
}

void MainWindow::toggleSpellCheck(bool enabled)
{
    m_spellCheck = enabled;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (document)
            document->editor()->setSpellChecker(enabled ? m_spellChecker : nullptr);
    }
}

void MainWindow::zoomIn()
{
    if (auto *document = currentDocument())
        document->editor()->zoomIn(1);
}

void MainWindow::zoomOut()
{
    if (auto *document = currentDocument())
        document->editor()->zoomOut(1);
}

void MainWindow::zoomReset()
{
    if (auto *document = currentDocument())
        document->editor()->setBaseFont(m_editorFont);
}

void MainWindow::toggleFold()
{
    if (auto *editor = currentEditor())
        editor->toggleFoldAtCursor();
}

void MainWindow::foldAll()
{
    if (auto *editor = currentEditor())
        editor->foldAll();
}

void MainWindow::unfoldAll()
{
    if (auto *editor = currentEditor())
        editor->unfoldAll();
}

void MainWindow::setEncoding(TextEncoding encoding)
{
    if (auto *document = currentDocument()) {
        document->setEncoding(encoding);
        updateStatus();
        updateDocumentMenuState();
        refreshTabTitle(document);
        updateWindowTitle();
        updateActions();
    }
}

void MainWindow::setLineEnding(LineEnding eol)
{
    if (auto *document = currentDocument()) {
        document->setLineEnding(eol);
        updateStatus();
        updateDocumentMenuState();
        refreshTabTitle(document);
        updateWindowTitle();
        updateActions();
    }
}

// ---------------------------------------------------------------------------
// Recent files and settings
// ---------------------------------------------------------------------------

void MainWindow::updateRecentFiles(const QString &path)
{
    if (path.isEmpty())
        return;
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 10)
        m_recentFiles.removeLast();
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu)
        return;

    m_recentMenu->clear();
    m_recentActions.clear();

    if (m_recentFiles.isEmpty()) {
        auto *empty = m_recentMenu->addAction(tr("(no recent files)"));
        empty->setEnabled(false);
        return;
    }

    for (int i = 0; i < m_recentFiles.size(); ++i) {
        const QString path = m_recentFiles.at(i);
        auto *action = m_recentMenu->addAction(tr("&%1  %2").arg(i + 1).arg(displayPath(path)));
        action->setData(path);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        m_recentActions.append(action);
    }

    m_recentMenu->addSeparator();
    auto *clear = m_recentMenu->addAction(tr("Clear Recent Files"));
    connect(clear, &QAction::triggered, this, &MainWindow::clearRecentFiles);
}

void MainWindow::openRecentFile()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    const QString path = action->data().toString();
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Open Recent"),
                             tr("The file \"%1\" no longer exists.").arg(displayPath(path)));
        m_recentFiles.removeAll(path);
        rebuildRecentMenu();
        return;
    }
    openFile(path);
}

void MainWindow::clearRecentFiles()
{
    m_recentFiles.clear();
    rebuildRecentMenu();
}

void MainWindow::applySettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("window"));
    const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
    settings.endGroup();

    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    settings.beginGroup(QStringLiteral("editor"));
    m_wordWrap = settings.value(QStringLiteral("wordWrap"), false).toBool();
    m_lineNumbers = settings.value(QStringLiteral("lineNumbers"), true).toBool();
    m_spellCheck = settings.value(QStringLiteral("spellCheck"), true).toBool();

    const QString defaultFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    const QString family = settings.value(QStringLiteral("fontFamily"), defaultFamily).toString();
    const int size = settings.value(QStringLiteral("fontSize"), 11).toInt();

    m_editorFont = QFont(family, size);
    m_editorFont.setStyleHint(QFont::Monospace);
    m_editorFont.setFixedPitch(true);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("search"));
    m_searchMatchCase = settings.value(QStringLiteral("matchCase"), false).toBool();
    m_searchWholeWord = settings.value(QStringLiteral("wholeWord"), false).toBool();
    m_searchRegex = settings.value(QStringLiteral("regex"), false).toBool();
    m_searchWrap = settings.value(QStringLiteral("wrap"), true).toBool();
    settings.endGroup();

    m_recentFiles = settings.value(QStringLiteral("files/recent")).toStringList();
    m_recentWorkspaces = settings.value(QStringLiteral("workspaces/recent")).toStringList();

    // Remove entries that disappeared while the editor was closed.
    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        if (!QFileInfo::exists(m_recentFiles.at(i)))
            m_recentFiles.removeAt(i);
    }
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("window"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("state"), saveState());
    settings.endGroup();

    settings.beginGroup(QStringLiteral("editor"));
    settings.setValue(QStringLiteral("wordWrap"), m_wordWrap);
    settings.setValue(QStringLiteral("lineNumbers"), m_lineNumbers);
    settings.setValue(QStringLiteral("spellCheck"), m_spellCheck);
    settings.setValue(QStringLiteral("fontFamily"), m_editorFont.family());
    settings.setValue(QStringLiteral("fontSize"), m_editorFont.pointSize());
    settings.endGroup();

    settings.beginGroup(QStringLiteral("search"));
    settings.setValue(QStringLiteral("matchCase"), m_searchMatchCase);
    settings.setValue(QStringLiteral("wholeWord"), m_searchWholeWord);
    settings.setValue(QStringLiteral("regex"), m_searchRegex);
    settings.setValue(QStringLiteral("wrap"), m_searchWrap);
    settings.endGroup();

    settings.setValue(QStringLiteral("files/recent"), m_recentFiles);
    settings.setValue(QStringLiteral("workspaces/recent"), m_recentWorkspaces);
    if (m_fileExplorer)
        settings.setValue(QStringLiteral("explorer/root"), m_fileExplorer->rootPath());

    saveSession();
}

void MainWindow::restoreSession()
{
    QSettings settings;
    if (!settings.value(QStringLiteral("session/restore"), true).toBool()) {
        newDocument();
        return;
    }

    const QStringList files = settings.value(QStringLiteral("session/files")).toStringList();
    const QVariantList lines = settings.value(QStringLiteral("session/lines")).toList();
    const QVariantList columns = settings.value(QStringLiteral("session/columns")).toList();
    const int savedActive = settings.value(QStringLiteral("session/active"), -1).toInt();

    // Restoring must not reorder the recent-files list.
    const QStringList recentBefore = m_recentFiles;

    int activeTab = -1;
    for (int i = 0; i < files.size(); ++i) {
        const QString path = files.at(i);
        if (!QFileInfo(path).isFile())
            continue;

        openFile(path);

        Document *document = currentDocument();
        if (!document || document->filePath() != QFileInfo(path).absoluteFilePath())
            continue;

        const int line = i < lines.size() ? qMax(1, lines.at(i).toInt()) : 1;
        const int column = i < columns.size() ? qMax(1, columns.at(i).toInt()) : 1;
        document->editor()->gotoLine(line, column);

        if (i == savedActive)
            activeTab = m_tabs->currentIndex();
    }

    if (m_tabs->count() == 0)
        newDocument();
    else if (activeTab >= 0 && activeTab < m_tabs->count())
        m_tabs->setCurrentIndex(activeTab);

    const QString workspace = settings.value(QStringLiteral("session/workspace")).toString();
    if (!workspace.isEmpty() && QFileInfo(workspace).isFile())
        m_workspacePath = workspace;

    m_recentFiles = recentBefore;
    rebuildRecentMenu();

    updateWindowTitle();
}

void MainWindow::saveSession()
{
    QSettings settings;
    if (!settings.value(QStringLiteral("session/restore"), true).toBool()) {
        settings.remove(QStringLiteral("session/files"));
        settings.remove(QStringLiteral("session/lines"));
        settings.remove(QStringLiteral("session/columns"));
        settings.remove(QStringLiteral("session/active"));
        settings.remove(QStringLiteral("session/workspace"));
        return;
    }

    QStringList files;
    QVariantList lines;
    QVariantList columns;
    int active = -1;
    Document *current = currentDocument();

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (!document || document->filePath().isEmpty())
            continue;

        files.append(document->filePath());
        lines.append(document->editor()->currentLine());
        columns.append(document->editor()->currentColumn());
        if (document == current)
            active = files.size() - 1;
    }

    if (active < 0 && !files.isEmpty())
        active = 0;

    settings.setValue(QStringLiteral("session/files"), files);
    settings.setValue(QStringLiteral("session/lines"), lines);
    settings.setValue(QStringLiteral("session/columns"), columns);
    settings.setValue(QStringLiteral("session/active"), active);
    settings.setValue(QStringLiteral("session/workspace"), m_workspacePath);
}

void MainWindow::applyFontToAll()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (document)
            document->editor()->setBaseFont(m_editorFont);
    }
}

Editor *MainWindow::currentEditor() const
{
    if (auto *document = currentDocument())
        return document->editor();
    return nullptr;
}

// ---------------------------------------------------------------------------
// Macro commands
// ---------------------------------------------------------------------------

void MainWindow::startMacroRecording()
{
    if (!m_macroManager)
        return;

    m_macroManager->startRecording();
    if (auto *macro = m_macroManager->recordingMacro()) {
        m_recordMacroAction->setEnabled(false);
        m_stopRecordMacroAction->setEnabled(true);
        m_playMacroAction->setEnabled(false);
        statusBar()->showMessage(tr("Macro recording started..."), 3000);
    }
}

void MainWindow::stopMacroRecording()
{
    if (!m_macroManager)
        return;

    Macro *macro = m_macroManager->stopRecording();
    m_recordMacroAction->setEnabled(true);
    m_stopRecordMacroAction->setEnabled(false);
    m_playMacroAction->setEnabled(true);

    if (macro && !macro->actions().isEmpty()) {
        const bool saved = m_macroManager->saveToDirectory(MacroManager::defaultDirectory());
        if (saved) {
            statusBar()->showMessage(
                tr("Macro recorded: %1 actions").arg(macro->actions().size()), 4000);
        } else {
            statusBar()->showMessage(
                tr("Macro recorded, but it could not be saved to disk."), 6000);
        }
    } else {
        statusBar()->showMessage(tr("Macro recording cancelled (empty)"), 3000);
        if (macro)
            m_macroManager->removeUserMacro(macro);
    }
}

void MainWindow::playMacro()
{
    if (!m_macroManager)
        return;

    // Play the last recorded macro (if any)
    const auto &userMacros = m_macroManager->userMacros();
    if (!userMacros.isEmpty()) {
        Macro *last = userMacros.last();
        if (auto *editor = currentEditor()) {
            m_macroManager->playMacro(last, editor);
            statusBar()->showMessage(tr("Macro played: %1 actions").arg(last->actions().size()), 3000);
        }
    }
}

void MainWindow::playBuiltinMacro(int index)
{
    if (!m_macroManager)
        return;

    if (auto *editor = currentEditor()) {
        m_macroManager->playBuiltin(index, editor);
        statusBar()->showMessage(tr("Macro %1 played").arg(index + 1), 2000);
    }
}

void MainWindow::saveMacroToSlot(int index)
{
    if (!m_macroManager || index < 0 || index >= MacroManager::MaxBuiltinMacros)
        return;

    const auto &userMacros = m_macroManager->userMacros();
    if (userMacros.isEmpty())
        return;

    Macro *last = userMacros.last();
    m_macroManager->setBuiltinMacro(index, last);
    const bool saved = m_macroManager->saveToDirectory(MacroManager::defaultDirectory());
    statusBar()->showMessage(saved
                                 ? tr("Macro saved to slot %1").arg(index + 1)
                                 : tr("Macro assigned to slot %1, but the assignment could not be saved.")
                                       .arg(index + 1),
                             5000);
}

void MainWindow::manageMacros()
{
    if (!m_macroManager)
        return;

    MacroManagerDialog dialog(m_macroManager, this);
    connect(&dialog, &MacroManagerDialog::playRequested, this, [this](Macro *macro) {
        if (auto *editor = currentEditor())
            m_macroManager->playMacro(macro, editor);
    });
    connect(&dialog, &MacroManagerDialog::macrosChanged, this,
            &MainWindow::updateMacroActions);
    dialog.exec();
    updateMacroActions();
}

void MainWindow::updateMacroActions()
{
    if (!m_macroManager)
        return;

    for (int i = 0; i < MacroManager::MaxBuiltinMacros; ++i) {
        if (!m_macroSlotActions[i])
            continue;
        Macro *macro = m_macroManager->builtinMacro(i);
        if (macro) {
            m_macroSlotActions[i]->setText(
                tr("Macro %1: %2").arg(i + 1).arg(macro->name()));
            m_macroSlotActions[i]->setToolTip(
                tr("Play \"%1\" (Ctrl+Shift+%2)").arg(macro->name()).arg(i + 1));
        } else {
            m_macroSlotActions[i]->setText(tr("Macro %1 (empty)").arg(i + 1));
            m_macroSlotActions[i]->setToolTip(
                tr("No macro assigned to Ctrl+Shift+%1").arg(i + 1));
        }
    }
}

void MainWindow::registerCustomShortcut(Macro *macro, const QKeySequence &shortcut)
{
    if (!macro || shortcut.isEmpty())
        return;
    unregisterCustomShortcut(macro);

    auto *sc = new QShortcut(shortcut, this);
    sc->setContext(Qt::ApplicationShortcut);
    connect(sc, &QShortcut::activated, this, [this, macro]() {
        if (auto *editor = currentEditor())
            m_macroManager->playMacro(macro, editor);
    });
    m_customShortcuts[macro] = sc;
}

void MainWindow::unregisterCustomShortcut(Macro *macro)
{
    if (auto it = m_customShortcuts.find(macro); it != m_customShortcuts.end()) {
        it.value()->deleteLater();
        m_customShortcuts.erase(it);
    }
}

void MainWindow::registerAllCustomShortcuts()
{
    if (!m_macroManager)
        return;
    for (auto it = m_macroManager->allCustomShortcuts().constBegin();
         it != m_macroManager->allCustomShortcuts().constEnd(); ++it) {
        registerCustomShortcut(it.key(), it.value());
    }
}

void MainWindow::unregisterAllCustomShortcuts()
{
    for (auto *sc : m_customShortcuts)
        sc->deleteLater();
    m_customShortcuts.clear();
}

// ---------------------------------------------------------------------------
// Clip Library
// ---------------------------------------------------------------------------

void MainWindow::createDockWidgets()
{
    // File explorer on the left.
    m_fileExplorer = new FileExplorerWidget(this);
    m_fileExplorerDock = new QDockWidget(tr("File Explorer"), this);
    m_fileExplorerDock->setObjectName(QStringLiteral("FileExplorerDock"));
    m_fileExplorerDock->setWidget(m_fileExplorer);
    m_fileExplorerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileExplorerDock);

    connect(m_fileExplorer, &FileExplorerWidget::fileActivated, this, &MainWindow::openFile);
    connect(m_fileExplorer, &FileExplorerWidget::findInFolderRequested, this,
            [this](const QString &folder) {
                showFindReplace();
                if (m_findDialog)
                    m_findDialog->setSearchFolder(folder);
            });
    connect(m_fileExplorer, &FileExplorerWidget::diffRequested, this,
            [this](const QString &relativePath) {
                if (m_gitRepo && m_gitRepo->isValid()) {
                    QVector<GitDiffFile> diffs = m_gitRepo->diff(relativePath);
                    if (!diffs.isEmpty()) {
                        showGitDiff(diffs);
                    }
                }
            });
    connect(m_fileExplorerDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_toggleFileExplorerAction->blockSignals(true);
        m_toggleFileExplorerAction->setChecked(visible);
        m_toggleFileExplorerAction->blockSignals(false);
    });

    QSettings explorerSettings;
    const QString explorerRoot = explorerSettings.value(QStringLiteral("explorer/root")).toString();
    if (!explorerRoot.isEmpty() && QFileInfo(explorerRoot).isDir())
        m_fileExplorer->setRootPath(explorerRoot);
    
    // Connect GitRepo to file explorer if we already have one
    if (m_gitRepo && m_gitRepo->isValid()) {
        m_fileExplorer->setGitRepo(m_gitRepo);
    }

    // Output pane for external tools.
    m_outputView = new QPlainTextEdit(this);
    m_outputView->setReadOnly(true);
    m_outputView->setMaximumBlockCount(5000);
    QFont outputFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    outputFont.setPointSize(10);
    m_outputView->setFont(outputFont);

    m_outputDock = new QDockWidget(tr("Output"), this);
    m_outputDock->setObjectName(QStringLiteral("OutputDock"));
    m_outputDock->setWidget(m_outputView);
    m_outputDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_outputDock);
    m_outputDock->hide();

    connect(m_outputDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_toggleOutputAction->blockSignals(true);
        m_toggleOutputAction->setChecked(visible);
        m_toggleOutputAction->blockSignals(false);
    });

    // Search results pane, sharing the bottom area with the output pane.
    m_searchResultsView = new QTreeWidget(this);
    m_searchResultsView->setObjectName(QStringLiteral("SearchResultsView"));
    m_searchResultsView->setColumnCount(3);
    m_searchResultsView->setHeaderLabels({tr("Line"), tr("Col"), tr("Text")});
    m_searchResultsView->setRootIsDecorated(true);
    m_searchResultsView->setUniformRowHeights(true);
    m_searchResultsView->setAlternatingRowColors(true);
    m_searchResultsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_searchResultsView->header()->setStretchLastSection(true);

    m_searchResultsDock = new QDockWidget(tr("Search Results"), this);
    m_searchResultsDock->setObjectName(QStringLiteral("SearchResultsDock"));
    m_searchResultsDock->setWidget(m_searchResultsView);
    m_searchResultsDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_searchResultsDock);
    m_searchResultsDock->hide();
    tabifyDockWidget(m_outputDock, m_searchResultsDock);

    connect(m_searchResultsView, &QTreeWidget::itemActivated, this,
            &MainWindow::activateSearchResult);
    connect(m_searchResultsDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_toggleSearchResultsAction->blockSignals(true);
        m_toggleSearchResultsAction->setChecked(visible);
        m_toggleSearchResultsAction->blockSignals(false);
    });

    m_clipLibraryWidget = new ClipLibraryWidget(m_clipLibrary, this);
    m_clipLibraryWidget->setEditor(currentEditor());

    m_clipLibraryDock = new QDockWidget(tr("Clip Library"), this);
    m_clipLibraryDock->setObjectName(QStringLiteral("ClipLibraryDock"));
    m_clipLibraryDock->setWidget(m_clipLibraryWidget);
    m_clipLibraryDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_clipLibraryDock);

    connect(m_clipLibraryDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_toggleClipLibraryAction->blockSignals(true);
        m_toggleClipLibraryAction->setChecked(visible);
        m_toggleClipLibraryAction->blockSignals(false);
    });

    // Update editor when tab changes
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        if (m_clipLibraryWidget)
            m_clipLibraryWidget->setEditor(currentEditor());
    });

    // Git Diff View
    m_gitDiffView = new papiro::GitDiffView(this);
    m_gitDiffDock = new QDockWidget(tr("Git Diff"), this);
    m_gitDiffDock->setObjectName(QStringLiteral("GitDiffDock"));
    m_gitDiffDock->setWidget(m_gitDiffView);
    m_gitDiffDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_gitDiffDock);
    m_gitDiffDock->hide();
    tabifyDockWidget(m_searchResultsDock, m_gitDiffDock);

    connect(m_gitDiffView, &papiro::GitDiffView::openFileRequested, this, &MainWindow::onGitDiffOpenFile);
    connect(m_gitDiffDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_toggleGitDiffAction->blockSignals(true);
        m_toggleGitDiffAction->setChecked(visible);
        m_toggleGitDiffAction->blockSignals(false);
    });
}

void MainWindow::toggleClipLibrary(bool visible)
{
    if (m_clipLibraryDock) {
        m_clipLibraryDock->setVisible(visible);
    }
}

void MainWindow::toggleFileExplorer(bool visible)
{
    if (m_fileExplorerDock)
        m_fileExplorerDock->setVisible(visible);
}

void MainWindow::openFolder()
{
    QSettings settings;
    const QString start = settings.value(QStringLiteral("explorer/root"), QDir::homePath()).toString();
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Open Folder"), start);
    if (directory.isEmpty())
        return;

    if (m_fileExplorer)
        m_fileExplorer->setRootPath(directory);
    if (m_fileExplorerDock)
        m_fileExplorerDock->setVisible(true);
    if (m_toggleFileExplorerAction)
        m_toggleFileExplorerAction->setChecked(true);
    settings.setValue(QStringLiteral("explorer/root"), directory);
    updateGitStatus();
}

void MainWindow::configureTools()
{
    ToolsDialog dialog(m_toolManager, this);
    if (dialog.exec() == QDialog::Accepted)
        rebuildToolsMenu();
}

void MainWindow::toggleOutput(bool visible)
{
    if (m_outputDock)
        m_outputDock->setVisible(visible);
}

void MainWindow::toggleSearchResults(bool visible)
{
    if (m_searchResultsDock)
        m_searchResultsDock->setVisible(visible);
}

void MainWindow::openWorkspace()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Workspace"), QDir::homePath(), Workspace::fileFilter());
    if (!path.isEmpty())
        loadWorkspace(path);
}

bool MainWindow::loadWorkspace(const QString &path)
{
    QString error;
    const Workspace workspace = Workspace::load(path, &error);
    if (!workspace.isValid()) {
        QMessageBox::warning(this, tr("Open Workspace"),
                             error.isEmpty() ? tr("The workspace is empty.") : error);
        return false;
    }

    // Ask about unsaved changes before replacing the current session.
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (!maybeSave(document))
            return false;
    }

    while (m_tabs->count() > 0) {
        QWidget *widget = m_tabs->widget(0);
        m_tabs->removeTab(0);
        widget->deleteLater();
    }
    clearSearchResults();

    if (!workspace.rootPath.isEmpty() && QFileInfo(workspace.rootPath).isDir()) {
        if (m_fileExplorer)
            m_fileExplorer->setRootPath(workspace.rootPath);
        if (m_fileExplorerDock)
            m_fileExplorerDock->setVisible(true);
        if (m_toggleFileExplorerAction)
            m_toggleFileExplorerAction->setChecked(true);
    }

    for (const QString &file : workspace.files) {
        if (QFileInfo(file).isFile())
            openFile(file);
    }
    if (m_tabs->count() == 0)
        newDocument();

    if (workspace.activeIndex >= 0 && workspace.activeIndex < m_tabs->count())
        m_tabs->setCurrentIndex(workspace.activeIndex);

    m_workspacePath = path;
    updateRecentWorkspaces(path);
    updateWindowTitle();
    statusBar()->showMessage(
        tr("Workspace opened: %1").arg(QFileInfo(path).completeBaseName()), 4000);
    return true;
}

void MainWindow::saveWorkspaceAs()
{
    QString suggested = m_workspacePath;
    if (suggested.isEmpty()) {
        const Document *document = currentDocument();
        if (document && !document->filePath().isEmpty())
            suggested = QFileInfo(document->filePath()).absolutePath()
                + QStringLiteral("/untitled.") + Workspace::defaultSuffix();
        else
            suggested = QDir::homePath() + QStringLiteral("/untitled.")
                + Workspace::defaultSuffix();
    }

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Workspace"), suggested, Workspace::fileFilter());
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".ppw"), Qt::CaseInsensitive))
        path += QStringLiteral(".ppw");

    Workspace workspace;
    workspace.name = QFileInfo(path).completeBaseName();
    workspace.activeIndex = m_tabs->currentIndex();
    if (m_fileExplorer)
        workspace.rootPath = m_fileExplorer->rootPath();

    int skipped = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *document = qobject_cast<Document *>(m_tabs->widget(i));
        if (!document)
            continue;
        if (document->filePath().isEmpty()) {
            ++skipped;
            continue;
        }
        workspace.files.append(document->filePath());
    }

    QString error;
    if (!workspace.save(path, &error)) {
        QMessageBox::warning(this, tr("Save Workspace"), error);
        return;
    }

    m_workspacePath = path;
    updateRecentWorkspaces(path);
    updateWindowTitle();

    QString message = tr("Workspace saved: %1").arg(QFileInfo(path).completeBaseName());
    if (skipped > 0)
        message += QStringLiteral(" ") + tr("(%1 unsaved file(s) skipped)").arg(skipped);
    statusBar()->showMessage(message, 4000);
}

void MainWindow::printDocument()
{
    Document *document = currentDocument();
    if (!document)
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(document->displayName());
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    document->editor()->print(&printer);
}

void MainWindow::printPreview()
{
    Document *document = currentDocument();
    if (!document)
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(document->displayName());
    QPrintPreviewDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Preview"));
    connect(&dialog, &QPrintPreviewDialog::paintRequested, document->editor(),
            [document](QPrinter *printer) { document->editor()->print(printer); });
    dialog.exec();
}

void MainWindow::updateRecentWorkspaces(const QString &path)
{
    if (path.isEmpty())
        return;
    m_recentWorkspaces.removeAll(path);
    m_recentWorkspaces.prepend(path);
    while (m_recentWorkspaces.size() > 10)
        m_recentWorkspaces.removeLast();
    rebuildWorkspaceMenu();
}

void MainWindow::rebuildWorkspaceMenu()
{
    if (!m_workspaceMenu)
        return;

    m_workspaceMenu->clear();
    m_recentWorkspaceActions.clear();

    if (m_recentWorkspaces.isEmpty()) {
        QAction *empty = m_workspaceMenu->addAction(tr("(no recent workspaces)"));
        empty->setEnabled(false);
        return;
    }

    for (int i = 0; i < m_recentWorkspaces.size(); ++i) {
        const QString path = m_recentWorkspaces.at(i);
        QAction *action = m_workspaceMenu->addAction(
            tr("&%1  %2").arg(i + 1).arg(displayPath(path)));
        action->setData(path);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentWorkspace);
        m_recentWorkspaceActions.append(action);
    }

    m_workspaceMenu->addSeparator();
    QAction *clear = m_workspaceMenu->addAction(tr("Clear Recent Workspaces"));
    connect(clear, &QAction::triggered, this, &MainWindow::clearRecentWorkspaces);
}

void MainWindow::openRecentWorkspace()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    const QString path = action->data().toString();
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Open Workspace"),
                             tr("The workspace \"%1\" no longer exists.").arg(displayPath(path)));
        m_recentWorkspaces.removeAll(path);
        rebuildWorkspaceMenu();
        return;
    }
    loadWorkspace(path);
}

void MainWindow::clearRecentWorkspaces()
{
    m_recentWorkspaces.clear();
    rebuildWorkspaceMenu();
}

void MainWindow::rebuildToolsMenu()
{
    if (!m_toolsMenu || !m_toolManager)
        return;

    m_toolsMenu->clear();
    m_toolsMenu->addAction(m_configureToolsAction);
    m_toolsMenu->addSeparator();

    const QList<ExternalTool> &tools = m_toolManager->tools();
    if (tools.isEmpty()) {
        QAction *empty = m_toolsMenu->addAction(tr("(no tools configured)"));
        empty->setEnabled(false);
        return;
    }

    for (int i = 0; i < tools.size(); ++i) {
        const ExternalTool &tool = tools.at(i);
        QAction *action = m_toolsMenu->addAction(tool.name);
        if (!tool.isValid())
            action->setEnabled(false);
        connect(action, &QAction::triggered, this, [this, i] { runTool(i); });
    }
}

void MainWindow::runTool(int index)
{
    if (!m_toolManager)
        return;
    const QList<ExternalTool> tools = m_toolManager->tools();
    if (index < 0 || index >= tools.size())
        return;

    const ExternalTool tool = tools.at(index);
    if (!tool.isValid()) {
        QMessageBox::warning(this, tr("External Tools"),
                             tr("This tool is missing a name or a command."));
        return;
    }

    Document *document = currentDocument();
    if (document && tool.saveBeforeRun && document->isModified())
        saveCurrentDocument();

    const QString filePath = document ? document->filePath() : QString();
    const QFileInfo info(filePath);

    auto expand = [&](QString text) {
        text.replace(QStringLiteral("$FILEDIR"), info.absolutePath());
        text.replace(QStringLiteral("$FILENAME"), info.completeBaseName());
        text.replace(QStringLiteral("$FILEEXT"), info.suffix());
        text.replace(QStringLiteral("$FILE"), filePath);
        return text;
    };

    const QString program = expand(tool.command);
    const QStringList arguments = QProcess::splitCommand(expand(tool.arguments));
    QString workingDirectory = expand(tool.workingDirectory);
    if (workingDirectory.isEmpty() && !filePath.isEmpty())
        workingDirectory = info.absolutePath();

    if (m_outputView) {
        m_outputView->appendPlainText(
            QStringLiteral("> %1 %2").arg(program, arguments.join(QLatin1Char(' '))).trimmed());
        if (m_outputDock)
            m_outputDock->show();
    }

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    if (!workingDirectory.isEmpty())
        process->setWorkingDirectory(workingDirectory);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        if (!m_outputView)
            return;
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        if (!output.isEmpty())
            m_outputView->appendPlainText(output.trimmed());
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, program](int exitCode, QProcess::ExitStatus status) {
                if (m_outputView) {
                    if (status == QProcess::NormalExit) {
                        m_outputView->appendPlainText(
                            tr("%1 finished with exit code %2.").arg(program).arg(exitCode));
                    } else {
                        m_outputView->appendPlainText(tr("%1 terminated unexpectedly.").arg(program));
                    }
                }
                process->deleteLater();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, program](QProcess::ProcessError) {
                if (m_outputView) {
                    m_outputView->appendPlainText(
                        tr("Failed to run %1: %2").arg(program, process->errorString()));
                }
            });

    process->start(program, arguments);
}

void MainWindow::setTheme(const QString &key)
{
    const Theme::Mode mode = Theme::fromKey(key);
    Theme::apply(mode);

    QSettings settings;
    settings.setValue(QStringLiteral("ui/theme"), Theme::key(mode));

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *document = qobject_cast<Document *>(m_tabs->widget(i)))
            document->editor()->refreshTheme();
    }
    if (m_outputView)
        m_outputView->viewport()->update();
    for (QWidget *widget : QApplication::allWidgets())
        widget->update();
    updateStatus();
}

void MainWindow::setLanguage(const QString &locale)
{
    QSettings settings;
    if (settings.value(QStringLiteral("ui/language")).toString() == locale)
        return;
    settings.setValue(QStringLiteral("ui/language"), locale);

    QMessageBox::information(
        this, tr("Language"),
        tr("The interface language will change after restarting Papiro."));
}

// ---------------------------------------------------------------------------
// Git integration
// ---------------------------------------------------------------------------

void MainWindow::detectGitRepo(const QString &filePath)
{
    if (filePath.isEmpty()) {
        clearGitRepo();
        return;
    }

    QFileInfo info(filePath);
    QString dir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    
    // Walk up the directory tree to find a .git directory
    QDir currentDir(dir);
    while (currentDir.exists()) {
        QString gitDir = currentDir.absoluteFilePath(".git");
        if (QFileInfo::exists(gitDir)) {
            QString repoPath = currentDir.absolutePath();
            if (repoPath != m_currentRepoPath) {
                clearGitRepo();
                m_currentRepoPath = repoPath;
                m_gitRepo = new GitRepo(repoPath, this);
                if (m_gitRepo->isValid()) {
                    connect(m_gitRepo, &GitRepo::repositoryChanged, this, &MainWindow::onGitRepositoryChanged);
                    // Connect to file explorer
                    if (m_fileExplorer) {
                        m_fileExplorer->setGitRepo(m_gitRepo);
                    }
                    updateGitStatus();
                } else {
                    clearGitRepo();
                }
            }
            return;
        }
        
        QString parentPath = currentDir.absolutePath();
        if (!currentDir.cdUp() || currentDir.absolutePath() == parentPath) {
            break; // Reached filesystem root
        }
    }
    
    // No git repo found
    clearGitRepo();
}

void MainWindow::clearGitRepo()
{
    if (m_gitRepo) {
        m_gitRepo->deleteLater();
        m_gitRepo = nullptr;
    }
    m_currentRepoPath.clear();
    m_gitBranchLabel->hide();
    m_gitDirtyIndicator->hide();
    
    // Clear git repo from file explorer
    if (m_fileExplorer) {
        m_fileExplorer->setGitRepo(nullptr);
    }
}

void MainWindow::updateGitStatus()
{
    Document *document = currentDocument();
    if (!document || document->filePath().isEmpty()) {
        detectGitRepo(m_fileExplorer ? m_fileExplorer->rootPath() : QString());
        return;
    }
    
    detectGitRepo(document->filePath());
    
    if (!m_gitRepo || !m_gitRepo->isValid()) {
        m_gitBranchLabel->hide();
        m_gitDirtyIndicator->hide();
        return;
    }
    
    // Get current branch
    GitBranch branch = m_gitRepo->currentBranch();
    QString branchName = branch.isCurrent ? branch.displayName : tr("(no branch)");
    
    // Check if repo has uncommitted changes
    QVector<GitFileStatus> status = m_gitRepo->status(true, false);
    bool dirty = false;
    for (const GitFileStatus &s : status) {
        if (!s.isClean()) {
            dirty = true;
            break;
        }
    }
    
    updateGitBranchLabel(branchName, dirty);
}

void MainWindow::updateGitBranchLabel(const QString &branch, bool dirty)
{
    if (branch.isEmpty()) {
        m_gitBranchLabel->hide();
        m_gitDirtyIndicator->hide();
        return;
    }
    
    QString text = branch;
    if (dirty) {
        text += QStringLiteral(" <span style=\"color:#ffaa00;\">●</span>");
        m_gitDirtyIndicator->setText(QStringLiteral("●"));
        m_gitDirtyIndicator->setStyleSheet("color: #ffaa00;");
        m_gitDirtyIndicator->setToolTip(tr("Repository has uncommitted changes"));
        m_gitDirtyIndicator->show();
    } else {
        m_gitDirtyIndicator->setText(QStringLiteral("●"));
        m_gitDirtyIndicator->setStyleSheet("color: #4caf50;");
        m_gitDirtyIndicator->setToolTip(tr("Repository is clean"));
        m_gitDirtyIndicator->show();
    }
    
    m_gitBranchLabel->setText(text);
    m_gitBranchLabel->setToolTip(tr("Branch: %1\nStatus: %2").arg(branch).arg(dirty ? tr("Modified") : tr("Clean")));
    m_gitBranchLabel->show();
}

void MainWindow::onGitRepositoryChanged()
{
    updateGitStatus();
    
    // Also update file explorer if it's using our GitFileSystemModel
    if (m_fileExplorer && m_gitRepo) {
        m_fileExplorer->setGitRepo(m_gitRepo);
    }
}

void MainWindow::gitCommit()
{
    if (!m_gitRepo || !m_gitRepo->isValid())
        return;
    
    bool ok = false;
    QString message = QInputDialog::getMultiLineText(this, tr("Git Commit"),
                                                      tr("Commit message:"), QString(), &ok);
    if (!ok || message.trimmed().isEmpty())
        return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool success = m_gitRepo->commit(message.trimmed());
    QApplication::restoreOverrideCursor();
    
    if (success) {
        statusBar()->showMessage(tr("Commit successful"), 3000);
        updateGitStatus();
    } else {
        QMessageBox::warning(this, tr("Git Commit Failed"),
                             tr("Failed to commit. Check the output for details."));
    }
}

void MainWindow::gitPush()
{
    if (!m_gitRepo || !m_gitRepo->isValid())
        return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool success = m_gitRepo->push("origin");
    QApplication::restoreOverrideCursor();
    
    if (success) {
        statusBar()->showMessage(tr("Push successful"), 3000);
    } else {
        QMessageBox::warning(this, tr("Git Push Failed"),
                             tr("Failed to push. Check the output for details."));
    }
}

void MainWindow::gitPull()
{
    if (!m_gitRepo || !m_gitRepo->isValid())
        return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool success = m_gitRepo->pull("origin");
    QApplication::restoreOverrideCursor();
    
    if (success) {
        statusBar()->showMessage(tr("Pull successful"), 3000);
        updateGitStatus();
    } else {
        QMessageBox::warning(this, tr("Git Pull Failed"),
                             tr("Failed to pull. Check the output for details."));
    }
}

void MainWindow::gitBranch()
{
    if (!m_gitRepo || !m_gitRepo->isValid())
        return;
    
    QVector<GitBranch> branches = m_gitRepo->branches();
    QStringList branchNames;
    int currentIndex = -1;
    for (int i = 0; i < branches.size(); ++i) {
        const GitBranch &b = branches.at(i);
        QString name = b.displayName;
        if (b.isCurrent) {
            name = QStringLiteral("* %1").arg(name);
            currentIndex = i;
        }
        if (b.ahead > 0 || b.behind > 0) {
            name += QStringLiteral(" [↑%1 ↓%2]").arg(b.ahead).arg(b.behind);
        }
        branchNames << name;
    }
    
    bool ok = false;
    QString selected = QInputDialog::getItem(this, tr("Git Branch"),
                                             tr("Select branch:"), branchNames, currentIndex, false, &ok);
    if (!ok || selected.isEmpty())
        return;
    
    // Remove the "* " prefix if present
    if (selected.startsWith("* "))
        selected = selected.mid(2);
    
    // Remove ahead/behind info
    int bracketPos = selected.indexOf(" [");
    if (bracketPos > 0)
        selected = selected.left(bracketPos);
    
    // Check if we need to create the branch
    bool create = false;
    for (const GitBranch &b : branches) {
        if (b.displayName == selected && !b.isCurrent) {
            // Switching to existing branch
            QApplication::setOverrideCursor(Qt::WaitCursor);
            bool success = m_gitRepo->checkoutBranch(selected, false);
            QApplication::restoreOverrideCursor();
            if (success) {
                statusBar()->showMessage(tr("Switched to branch: %1").arg(selected), 3000);
                updateGitStatus();
            } else {
                QMessageBox::warning(this, tr("Git Branch Failed"),
                                     tr("Failed to switch branch."));
            }
            return;
        }
    }
    
    // Branch doesn't exist locally, offer to create it
    QMessageBox::StandardButton btn = QMessageBox::question(this, tr("Create Branch"),
        tr("Branch \"%1\" does not exist locally. Create and switch to it?").arg(selected),
        QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        bool success = m_gitRepo->checkoutBranch(selected, true);
        QApplication::restoreOverrideCursor();
        if (success) {
            statusBar()->showMessage(tr("Created and switched to branch: %1").arg(selected), 3000);
            updateGitStatus();
        } else {
            QMessageBox::warning(this, tr("Git Branch Failed"),
                                 tr("Failed to create branch."));
        }
    }
}

void MainWindow::gitLog()
{
    if (!m_gitRepo || !m_gitRepo->isValid())
        return;
    
    QVector<GitCommit> commits = m_gitRepo->log(50);
    if (commits.isEmpty()) {
        QMessageBox::information(this, tr("Git Log"), tr("No commits found."));
        return;
    }
    
    QStringList items;
    for (const GitCommit &c : commits) {
        QString refs = c.refs.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(c.refs.join(", "));
        items << QStringLiteral("%1 %2 - %3%4")
                  .arg(c.shortHash)
                  .arg(c.authorDate.toString(Qt::ISODate))
                  .arg(c.subject)
                  .arg(refs);
    }
    
    bool ok = false;
    QString selected = QInputDialog::getItem(this, tr("Git Log"),
                                             tr("Select commit:"), items, 0, false, &ok);
    if (!ok || selected.isEmpty())
        return;
    
    // For now just show details in a message box
    int index = items.indexOf(selected);
    if (index >= 0 && index < commits.size()) {
        const GitCommit &c = commits.at(index);
        QString details = tr("Commit: %1\nAuthor: %2 <%3>\nDate: %4\n\n%5")
                              .arg(c.hash)
                              .arg(c.authorName)
                              .arg(c.authorEmail)
                              .arg(c.authorDate.toString(Qt::ISODate))
                              .arg(c.subject + (c.body.isEmpty() ? "" : "\n\n" + c.body));
        QMessageBox::information(this, tr("Commit Details"), details);
    }
}

void MainWindow::gitBlame()
{
    Document *document = currentDocument();
    if (!document || document->filePath().isEmpty() || !m_gitRepo || !m_gitRepo->isValid()) {
        QMessageBox::warning(this, tr("Git Blame"),
                             tr("No file open or not in a Git repository."));
        return;
    }

    // Toggle inline blame view
    Editor *editor = document->editor();
    if (editor->isBlameVisible()) {
        editor->toggleBlame();
        return;
    }

    // Request blame asynchronously
    document->requestGitBlame(m_gitRepo);
}

void MainWindow::gitDiff()
{
    Document *document = currentDocument();
    if (!document || document->filePath().isEmpty() || !m_gitRepo || !m_gitRepo->isValid()) {
        QMessageBox::warning(this, tr("Git Diff"),
                             tr("No file open or not in a Git repository."));
        return;
    }

    QVector<GitDiffFile> diffs = m_gitRepo->diff(document->filePath());
    if (diffs.isEmpty()) {
        QMessageBox::information(this, tr("Git Diff"), tr("No changes in this file."));
        return;
    }

    showGitDiff(diffs);
}

void MainWindow::showGitDiff(const QVector<GitDiffFile>& diffs)
{
    if (!m_gitDiffView || !m_gitDiffDock)
        return;

    m_gitDiffView->setDiff(diffs);
    m_gitDiffDock->show();
    m_gitDiffDock->raise();
    m_toggleGitDiffAction->setChecked(true);
}

void MainWindow::onGitDiffOpenFile(const QString& filePath, int line)
{
    // Resolve the file path relative to the repo root
    QString absolutePath = filePath;
    if (m_gitRepo && m_gitRepo->isValid() && !QFileInfo(filePath).isAbsolute()) {
        absolutePath = QDir(m_gitRepo->workTree()).absoluteFilePath(filePath);
    }

    openFile(absolutePath);
    if (auto *document = currentDocument()) {
        document->editor()->gotoLine(line);
    }
}

void MainWindow::showKeyboardShortcuts()
{
    KeyboardShortcutsDialog dialog(this);
    dialog.exec();
}

} // namespace papiro
