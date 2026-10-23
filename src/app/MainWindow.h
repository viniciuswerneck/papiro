#pragma once

#include "core/ClipLibrary.h"
#include "core/Encoding.h"
#include "core/Macro.h"
#include "ui/FindReplaceDialog.h"

#include <QFont>
#include <QMainWindow>
#include <QRegularExpression>
#include <QStringList>

class QAction;
class QActionGroup;
class QDockWidget;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QTabWidget;
class QToolBar;

namespace papiro {

class Document;
class Editor;
class ClipLibraryWidget;
class FileExplorerWidget;
class SpellChecker;
class ToolManager;

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

private slots:
    void openFiles();
    void saveCurrentDocument();
    void saveCurrentDocumentAs();
    void saveAllDocuments();
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
    void goToLine();
    void chooseFont();
    void toggleWordWrap(bool enabled);
    void toggleLineNumbers(bool enabled);
    void toggleSpellCheck(bool enabled);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void toggleFold();
    void foldAll();
    void unfoldAll();
    void openRecentFile();
    void clearRecentFiles();
    // Macro actions
    void startMacroRecording();
    void stopMacroRecording();
    void playMacro();
    void playBuiltinMacro(int index);
    void saveMacroToSlot(int index);
    void manageMacros();
    // Clip Library
    void toggleClipLibrary(bool visible);
    // File Explorer
    void toggleFileExplorer(bool visible);
    void openFolder();
    // External tools
    void configureTools();
    void toggleOutput(bool visible);
    // Workspace
    void openWorkspace();
    void saveWorkspaceAs();
    void openRecentWorkspace();
    void clearRecentWorkspaces();
    // Print
    void printDocument();
    void printPreview();
    // Language
    void setLanguage(const QString &locale);

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
    void applyFontToAll();
    void updateRecentFiles(const QString &path);
    void rebuildRecentMenu();
    void setEncoding(TextEncoding encoding);
    void setLineEnding(LineEnding eol);
    void findInDocument(bool backward);
    QString expandReplacement(const QString &replacement,
                              const QRegularExpressionMatch &match) const;
    QRegularExpression buildRegex(const FindRequest &request, QString *error) const;
    Editor *currentEditor() const;
    void rebuildToolsMenu();
    void runTool(int index);
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
    QAction *m_closeAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_findAction = nullptr;
    QAction *m_findNextAction = nullptr;
    QAction *m_findPreviousAction = nullptr;
    QAction *m_wordWrapAction = nullptr;
    QAction *m_lineNumbersAction = nullptr;
    QAction *m_spellCheckAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
    QAction *m_toggleFoldAction = nullptr;
    QAction *m_foldAllAction = nullptr;
    QAction *m_unfoldAllAction = nullptr;
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
    // Workspace
    QAction *m_openWorkspaceAction = nullptr;
    QAction *m_saveWorkspaceAction = nullptr;
    // Print
    QAction *m_printAction = nullptr;
    QAction *m_printPreviewAction = nullptr;
    QMenu *m_languageMenu = nullptr;
    QActionGroup *m_languageGroup = nullptr;

    QStringList m_recentFiles;
    QStringList m_recentWorkspaces;
    QString m_workspacePath;
    QList<QAction *> m_recentWorkspaceActions;
    QFont m_editorFont;
    bool m_wordWrap = false;
    bool m_lineNumbers = true;
    bool m_spellCheck = true;

    MacroManager *m_macroManager = nullptr;
    ClipLibrary *m_clipLibrary = nullptr;
    ClipLibraryWidget *m_clipLibraryWidget = nullptr;
    QDockWidget *m_clipLibraryDock = nullptr;
    FileExplorerWidget *m_fileExplorer = nullptr;
    QDockWidget *m_fileExplorerDock = nullptr;
    ToolManager *m_toolManager = nullptr;
    QPlainTextEdit *m_outputView = nullptr;
    QDockWidget *m_outputDock = nullptr;
    SpellChecker *m_spellChecker = nullptr;
};

} // namespace papiro
