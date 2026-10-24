#pragma once

#include "core/ClipLibrary.h"
#include "core/Encoding.h"
#include "core/Macro.h"
#include "core/GitRepo.h"
#include "core/LSPManager.h"
#include "ui/FindReplaceDialog.h"
#include "ui/KeyboardShortcutsDialog.h"
#include "ui/LSPPreferencesDialog.h"

#include <QFont>
#include <QMainWindow>
#include <QMap>
#include <QRegularExpression>
#include <QShortcut>
#include <QStringList>

class QAction;
class QActionGroup;
class QDockWidget;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;

namespace papiro {
class GitDiffView;
class JavaRunner;

class Document;
class Editor;
class ClipLibraryWidget;
class FileExplorerWidget;
class SpellChecker;
class ToolManager;
struct Preferences;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void newDocument();
    void openFile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openFiles();
    void saveCurrentDocument();
    void saveCurrentDocumentAs();
    void saveAllDocuments();
    void markDocumentClean();
    void closeCurrentTab();
    void closeTab(int index);
    void updateWindowTitle();
    void updateStatus();
    void updateActions();
    void updateDocumentMenuState();
    void showFindReplace();
    void findNext() { findInDocument(false); }
    void findPrevious() { findInDocument(true); }
    void replaceCurrent();
    void replaceAll();
    void replaceAllInSelection(Editor *editor, const QRegularExpression &expression, int start, int end);
    void replaceAllInBlockSelection(Editor *editor, const QRegularExpression &expression);
    void replaceAllInDocument(Editor *editor, const QRegularExpression &expression);
    void goToLine();
    void goToLineColumn();
    void chooseFont();
    void openPreferences();
    void toggleWordWrap(bool enabled);
    void toggleLineNumbers(bool enabled);
    void toggleWhitespaceVisible(bool enabled);
    void toggleCursorFreePositioning(bool enabled);
    void toggleSpellCheck(bool enabled);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void toggleFold();
    void foldAll();
    void unfoldAll();
    void sortLines();
    void splitView(Qt::Orientation orientation = Qt::Horizontal);
    void splitVertical();
    void splitHorizontal();
    void unsplitView();
    void cloneView();
    void diffFiles();
    void openRecentFile();
    void clearRecentFiles();
    // LSP
    void openLSPPreferences();
    void updateLSPStatus();
    // Macro actions
    void startMacroRecording();
    void stopMacroRecording();
    void playMacro();
    void playBuiltinMacro(int index);
    void saveMacroToSlot(int index);
    void manageMacros();
    void updateMacroActions();
    // Clip Library
    void toggleClipLibrary(bool visible);
    // File Explorer
    void toggleFileExplorer(bool visible);
    void openFolder();
    // External tools
    void configureTools();
    void toggleOutput(bool visible);
    // Search results
    void toggleSearchResults(bool visible);
    void activateSearchResult(QTreeWidgetItem *item, int column);
    // Workspace
    void openWorkspace();
    void saveWorkspaceAs();
    void openRecentWorkspace();
    void clearRecentWorkspaces();
    // Print
    void printDocument();
    void printPreview();
    // Java
    void runJava();
    // Language
    void setLanguage(const QString &locale);
    // Theme
    void setTheme(const QString &key);
    // Keyboard Shortcuts
    void showKeyboardShortcuts();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    Document *currentDocument() const;
    Document *addDocument();
    bool maybeSave(Document *document);
    bool saveDocument(Document *document, bool saveAs);
    void refreshTabTitle(Document *document);
    void applySettings();
    void saveSettings();
    void restoreSession();
    void saveSession();
    void applyFontToAll();
    void applyPreferences(const Preferences &preferences);
    void updateRecentFiles(const QString &path);
    void rebuildRecentMenu();
    void setEncoding(TextEncoding encoding);
    void setLineEnding(LineEnding eol);
    void findInDocument(bool backward);
    void findAll(const FindRequest &request);
    void findAllInFolder(const FindRequest &request, const QString &folder);
    void clearSearchResults();
    QString expandReplacement(const QString &replacement,
                              const QRegularExpressionMatch &match) const;
    QRegularExpression buildRegex(const FindRequest &request, QString *error) const;
    Editor *currentEditor() const;
    void rebuildToolsMenu();
    void runTool(int index);
    void appendOutput(const QString &text);
    void appendOutputWithErrorParsing(const QString &output,
                                      const QString &errorPattern,
                                      const QString &filePattern,
                                      const QString &linePattern,
                                      const QString &columnPattern,
                                      const QString &workingDirectory);
    void appendJavaOutput(const QString &text);
    bool loadWorkspace(const QString &path);
    void updateRecentWorkspaces(const QString &path);
    void rebuildWorkspaceMenu();

    QTabWidget *m_tabs = nullptr;
    FindReplaceDialog *m_findDialog = nullptr;
    FindRequest m_lastFind;
    QString m_lastReplacement;

    QLabel *m_positionLabel = nullptr;
    QLabel *m_selectionLabel = nullptr;
    QLabel *m_encodingLabel = nullptr;
    QLabel *m_eolLabel = nullptr;
    QLabel *m_syntaxLabel = nullptr;

    QMenu *m_recentMenu = nullptr;
    QMenu *m_encodingMenu = nullptr;
    QMenu *m_eolMenu = nullptr;
    QMenu *m_macroMenu = nullptr;
    QMenu *m_macroSlotMenu = nullptr;
    QMenu *m_toolsMenu = nullptr;
    QMenu *m_workspaceMenu = nullptr;
    
    QActionGroup *m_encodingGroup = nullptr;
    QActionGroup *m_eolGroup = nullptr;
    QList<QAction *> m_recentActions;

    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_saveAllAction = nullptr;
    QAction *m_markCleanAction = nullptr;
    QAction *m_runJavaAction = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_findAction = nullptr;
    QAction *m_findNextAction = nullptr;
    QAction *m_findPreviousAction = nullptr;
    QAction *m_preferencesAction = nullptr;
    QAction *m_wordWrapAction = nullptr;
    QAction *m_lineNumbersAction = nullptr;
    QAction *m_whitespaceVisibleAction = nullptr;
    QAction *m_cursorFreePositioningAction = nullptr;
    QAction *m_spellCheckAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
    QAction *m_toggleFoldAction = nullptr;
    QAction *m_foldAllAction = nullptr;
    QAction *m_unfoldAllAction = nullptr;
    QAction *m_splitViewAction = nullptr;
    QAction *m_splitVerticalAction = nullptr;
    QAction *m_splitHorizontalAction = nullptr;
    QAction *m_unsplitViewAction = nullptr;
    QAction *m_cloneViewAction = nullptr;
    QAction *m_diffAction = nullptr;
    // Macro actions
    QAction *m_recordMacroAction = nullptr;
    QAction *m_stopRecordMacroAction = nullptr;
    QAction *m_playMacroAction = nullptr;
    QAction *m_manageMacrosAction = nullptr;
    QAction *m_macroSlotActions[MacroManager::MaxBuiltinMacros] = {nullptr};
    // Clip Library
    QAction *m_toggleClipLibraryAction = nullptr;
    // File Explorer
    QAction *m_openFolderAction = nullptr;
    QAction *m_toggleFileExplorerAction = nullptr;
    // Output / tools
    QAction *m_configureToolsAction = nullptr;
    QAction *m_toggleOutputAction = nullptr;
    // Search results
    QAction *m_toggleSearchResultsAction = nullptr;
    // Session
    QAction *m_restoreSessionAction = nullptr;
    // Workspace
    QAction *m_openWorkspaceAction = nullptr;
    QAction *m_saveWorkspaceAction = nullptr;
    // Print
    QAction *m_printAction = nullptr;
    QAction *m_printPreviewAction = nullptr;
    QMenu *m_languageMenu = nullptr;
    QActionGroup *m_languageGroup = nullptr;
    QMenu *m_themeMenu = nullptr;
    QActionGroup *m_themeGroup = nullptr;
    
    QAction *m_keyboardShortcutsAction = nullptr;

    QStringList m_recentFiles;
    QStringList m_recentWorkspaces;
    QString m_workspacePath;
    QList<QAction *> m_recentWorkspaceActions;
    QFont m_editorFont;
    bool m_wordWrap = false;
    bool m_lineNumbers = true;
    bool m_whitespaceVisible = false;
    bool m_cursorFreePositioning = false;
    bool m_spellCheck = true;
    bool m_searchMatchCase = false;
    bool m_searchWholeWord = false;
    bool m_searchRegex = false;
    bool m_searchMultiline = false;
    bool m_searchWrap = true;
    int m_largeFileThresholdMB = 16;

    MacroManager *m_macroManager = nullptr;
    ClipLibrary *m_clipLibrary = nullptr;
    ClipLibraryWidget *m_clipLibraryWidget = nullptr;
    QDockWidget *m_clipLibraryDock = nullptr;
    FileExplorerWidget *m_fileExplorer = nullptr;
    QDockWidget *m_fileExplorerDock = nullptr;
    ToolManager *m_toolManager = nullptr;
    QTextBrowser *m_outputView = nullptr;
    QDockWidget *m_outputDock = nullptr;
    QTreeWidget *m_searchResultsView = nullptr;
    QDockWidget *m_searchResultsDock = nullptr;
    SpellChecker *m_spellChecker = nullptr;
    LSPManager *m_lspManager = nullptr;
    JavaRunner *m_javaRunner = nullptr;
    QString m_javaOutputBuffer;
    QLabel *m_lspStatusLabel = nullptr;

    QMap<Macro *, QShortcut *> m_customShortcuts;

    void registerCustomShortcut(Macro *macro, const QKeySequence &shortcut);
    void unregisterCustomShortcut(Macro *macro);
    void registerAllCustomShortcuts();
    void unregisterAllCustomShortcuts();
};

} // namespace papiro
