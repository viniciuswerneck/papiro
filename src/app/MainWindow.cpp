#include "app/MainWindow.h"

#include "app/Document.h"
#include "core/ClipLibrary.h"
#include "core/ExternalTool.h"
#include "core/Macro.h"
#include "core/SpellChecker.h"
#include "core/SyntaxDefinition.h"
#include "core/Workspace.h"
#include "editor/Editor.h"
#include "ui/ClipLibraryWidget.h"
#include "ui/FileExplorerWidget.h"
#include "ui/ToolsDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontDialog>
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
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolBar>
#include <QUrl>

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

    // Load macros
    m_macroManager->loadFromDirectory(MacroManager::defaultDirectory());

    // Load clip library
    m_clipLibrary->load();

    // Load external tool definitions
    m_toolManager->load();
    rebuildToolsMenu();
    rebuildWorkspaceMenu();

    m_wordWrapAction->setChecked(m_wordWrap);
    m_lineNumbersAction->setChecked(m_lineNumbers);
    m_spellCheckAction->setChecked(m_spellCheck);

    newDocument();
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
                                  themedIcon(QStringLiteral("document-open"),
                                             QStyle::SP_DialogOpenButton, this));
    connect(openAction, &QAction::triggered, this, &MainWindow::openFiles);

    m_saveAction = makeAction(tr("&Save"), QKeySequence::Save,
                              themedIcon(QStringLiteral("document-save"),
                                         QStyle::SP_DialogSaveButton, this));
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
            document->editor()->cut();
    });

    m_copyAction = makeAction(tr("&Copy"), QKeySequence::Copy,
                              themedIcon(QStringLiteral("edit-copy"), QStyle::SP_FileIcon, this));
    connect(m_copyAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->copy();
    });

    m_pasteAction = makeAction(tr("&Paste"), QKeySequence::Paste,
                               themedIcon(QStringLiteral("edit-paste"), QStyle::SP_ArrowDown, this));
    connect(m_pasteAction, &QAction::triggered, this, [this] {
        if (auto *document = currentDocument())
            document->editor()->paste();
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

    // Workspace
    m_openWorkspaceAction = makeAction(tr("Open &Workspace..."),
                                       QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(m_openWorkspaceAction, &QAction::triggered, this, &MainWindow::openWorkspace);

    m_saveWorkspaceAction = makeAction(tr("Save Workspace &As..."),
                                       QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(m_saveWorkspaceAction, &QAction::triggered, this, &MainWindow::saveWorkspaceAs);

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

    // File
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addAction(m_openFolderAction);
    m_recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    m_workspaceMenu = fileMenu->addMenu(tr("Recent &Workspaces"));
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
    m_languageMenu = viewMenu->addMenu(tr("&Language"));
    m_languageMenu->addActions(m_languageGroup->actions());

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

    // Help
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
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

    connect(document, &Document::modificationChanged, this, [this, document](bool) {
        refreshTabTitle(document);
        if (document == currentDocument()) {
            updateWindowTitle();
            updateActions();
        }
    });
    connect(document, &Document::pathChanged, this, [this, document](const QString &) {
        refreshTabTitle(document);
        if (document == currentDocument()) {
            updateWindowTitle();
            updateStatus();
        }
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
    updateWindowTitle();
    updateStatus();
    updateActions();
    updateDocumentMenuState();
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
    const bool hasSelection = hasDocument && editor->textCursor().hasSelection();
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
    }

    if (Document *document = currentDocument()) {
        const QString selection = document->editor()->textCursor().selectedText();
        if (!selection.isEmpty() && !selection.contains(QChar::ParagraphSeparator))
            m_findDialog->setFindText(selection);
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
    settings.endGroup();

    settings.beginGroup(QStringLiteral("editor"));
    settings.setValue(QStringLiteral("wordWrap"), m_wordWrap);
    settings.setValue(QStringLiteral("lineNumbers"), m_lineNumbers);
    settings.setValue(QStringLiteral("spellCheck"), m_spellCheck);
    settings.setValue(QStringLiteral("fontFamily"), m_editorFont.family());
    settings.setValue(QStringLiteral("fontSize"), m_editorFont.pointSize());
    settings.endGroup();

    settings.setValue(QStringLiteral("files/recent"), m_recentFiles);
    settings.setValue(QStringLiteral("workspaces/recent"), m_recentWorkspaces);
    if (m_fileExplorer)
        settings.setValue(QStringLiteral("explorer/root"), m_fileExplorer->rootPath());
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
        statusBar()->showMessage(tr("Macro recorded: %1 actions").arg(macro->actions().size()), 4000);
    } else {
        statusBar()->showMessage(tr("Macro recording cancelled (empty)"), 3000);
        if (macro) {
            macro->deleteLater();
        }
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
    m_macroSlotActions[index]->setText(tr("Macro %1").arg(index + 1));
    statusBar()->showMessage(tr("Macro saved to slot %1").arg(index + 1), 3000);
}

void MainWindow::manageMacros()
{
    if (!m_macroManager)
        return;

    // Simple dialog to manage macros - for now just show info
    const auto &userMacros = m_macroManager->userMacros();
    QString msg = tr("Total user macros: %1\n").arg(userMacros.size());
    msg += tr("\nBuilt-in slots (Ctrl+Shift+1..9):\n");
    for (int i = 0; i < MacroManager::MaxBuiltinMacros; ++i) {
        Macro *m = m_macroManager->builtinMacro(i);
        if (m)
            msg += tr("  Slot %1: %2 (%3 actions)\n").arg(i + 1).arg(m->name()).arg(m->actions().size());
        else
            msg += tr("  Slot %1: (empty)\n").arg(i + 1);
    }
    QMessageBox::information(this, tr("Manage Macros"), msg);
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
    connect(m_fileExplorerDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_toggleFileExplorerAction->blockSignals(true);
        m_toggleFileExplorerAction->setChecked(visible);
        m_toggleFileExplorerAction->blockSignals(false);
    });

    QSettings explorerSettings;
    const QString explorerRoot = explorerSettings.value(QStringLiteral("explorer/root")).toString();
    if (!explorerRoot.isEmpty() && QFileInfo(explorerRoot).isDir())
        m_fileExplorer->setRootPath(explorerRoot);

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

} // namespace papiro
