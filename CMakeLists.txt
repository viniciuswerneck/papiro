cmake_minimum_required(VERSION 3.21)

project(Papiro
    VERSION 0.4.1
    DESCRIPTION "A focused, cross-platform text editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets PrintSupport LinguistTools)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Core: encoding detection, syntax definitions, macros, clip library (no GUI dependency)
# ---------------------------------------------------------------------------
add_library(papiro_core STATIC
    src/core/ClipLibrary.cpp
    src/core/Encoding.cpp
    src/core/ExternalTool.cpp
    src/core/GitRepo.cpp
    src/core/JavaRunner.cpp
    src/core/LSPClient.cpp
    src/core/LSPManager.cpp
    src/core/LSPMessage.cpp
    src/core/Macro.cpp
    src/core/SpellChecker.cpp
    src/core/SyntaxDefinition.cpp
    src/core/Workspace.cpp
)
target_include_directories(papiro_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Hunspell (optional) — spell checking. Uses the system library when present,
# otherwise a locally extracted copy under ~/.local/hunspell.
# ---------------------------------------------------------------------------
find_path(HUNSPELL_INCLUDE_DIR NAMES hunspell/hunspell.hxx
    HINTS "$ENV{HOME}/.local/hunspell/usr/include")
find_library(HUNSPELL_LIBRARY NAMES hunspell
    HINTS "$ENV{HOME}/.local/hunspell/usr/lib/x86_64-linux-gnu"
          "$ENV{HOME}/.local/hunspell/usr/lib")

if(HUNSPELL_INCLUDE_DIR AND HUNSPELL_LIBRARY)
    target_include_directories(papiro_core PRIVATE ${HUNSPELL_INCLUDE_DIR})
    target_link_libraries(papiro_core PRIVATE ${HUNSPELL_LIBRARY})
    target_compile_definitions(papiro_core PRIVATE PAPIRO_HAVE_HUNSPELL=1)
    message(STATUS "Spell check: hunspell found (${HUNSPELL_LIBRARY})")
else()
    message(STATUS "Spell check: hunspell NOT found — disabled")
endif()

target_link_libraries(papiro_core PUBLIC Qt6::Core Qt6::Gui)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
set(PAPIRO_APP_SOURCES
    src/app/MainWindow.cpp
    src/app/Document.cpp
    src/editor/Editor.cpp
    src/editor/SyntaxHighlighter.cpp
    src/ui/SortLinesDialog.cpp
    src/ui/SplitView.cpp
    src/ui/DiffDialog.cpp
    src/ui/ClipLibraryWidget.cpp
    src/ui/FindReplaceDialog.cpp
    src/ui/GitBlameView.cpp
    src/ui/GitDiffView.cpp
    src/ui/KeyboardShortcutsDialog.cpp
    src/ui/LSPCompletionWidget.cpp
    src/ui/LSPHoverWidget.cpp
    src/ui/LSPPreferencesDialog.cpp
    src/ui/MacroManagerDialog.cpp
    src/ui/PreferencesDialog.cpp
    src/ui/PrintPreviewDialog.cpp
    src/ui/ToolsDialog.cpp
    src/ui/Theme.cpp
    src/ui/FileExplorerWidget.cpp
    src/ui/FluentTitleBar.cpp
)

qt_add_executable(Papiro WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${PAPIRO_APP_SOURCES}
)
target_link_libraries(Papiro PRIVATE papiro_core Qt6::Widgets Qt6::PrintSupport)
target_compile_definitions(Papiro PRIVATE PAPIRO_VERSION="${PROJECT_VERSION}")

qt_add_resources(Papiro "syntax_definitions"
    PREFIX "/syntax"
    BASE "resources/syntax"
    FILES
        resources/syntax/cpp.syn
        resources/syntax/css.syn
        resources/syntax/html.syn
        resources/syntax/java.syn
        resources/syntax/javascript.syn
        resources/syntax/json.syn
        resources/syntax/markdown.syn
        resources/syntax/python.syn
        resources/syntax/text.syn
)

# Fluent theme resources
qt_add_resources(Papiro "fluent_theme"
    PREFIX "/themes"
    BASE "resources/themes"
    FILES
        resources/themes/fluent.json
        resources/themes/fluent.qss
)

# Icon resources
qt_add_resources(Papiro "icons"
    PREFIX "/icons"
    BASE "resources/icons"
    FILES
        resources/icons/close.svg
)

# ---------------------------------------------------------------------------
# Translations (PT / EN / ES) — .ts updated by lupdate, compiled to .qm
# and embedded into the Qt resource system under ":/i18n".
# ---------------------------------------------------------------------------
set(PAPIRO_TS_FILES
    translations/papiro_pt.ts
    translations/papiro_en.ts
    translations/papiro_es.ts
)
qt_add_translations(Papiro
    TS_FILES ${PAPIRO_TS_FILES}
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete
)

install(TARGETS Papiro
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE papiro_core Qt6::Test)
    add_test(NAME core COMMAND test_core)

    # Editor lives in the app target, so compile the two editor sources it
    # needs into the test binary as well.
    qt_add_executable(test_editor
        tests/test_editor.cpp
        src/editor/Editor.cpp
        src/editor/SyntaxHighlighter.cpp
        src/ui/LSPCompletionWidget.cpp
        src/ui/LSPHoverWidget.cpp
        src/ui/GitBlameView.cpp
        src/ui/Theme.cpp
    )
    target_link_libraries(test_editor PRIVATE papiro_core Qt6::Widgets Qt6::Test)
    add_test(NAME editor COMMAND test_editor)

    # Diff dialog: diff algorithm, change navigation and syntax highlighting.
    qt_add_executable(test_diffdialog
        tests/test_diffdialog.cpp
        tests/macro_stub.cpp
        src/ui/DiffDialog.cpp
        src/editor/SyntaxHighlighter.cpp
        src/ui/Theme.cpp
    )
    target_link_libraries(test_diffdialog PRIVATE papiro_core Qt6::Widgets Qt6::Test)
    add_test(NAME diffdialog COMMAND test_diffdialog)
    set_tests_properties(diffdialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # MainWindow integration tests (session restore and friends).
    qt_add_executable(test_mainwindow
        tests/test_mainwindow.cpp
        ${PAPIRO_APP_SOURCES}
    )
    target_link_libraries(test_mainwindow PRIVATE papiro_core Qt6::Widgets
                          Qt6::PrintSupport Qt6::Test)
    add_test(NAME mainwindow COMMAND test_mainwindow)

    # Java runner (JDK detection + compile/run) is QtCore-only.
    qt_add_executable(test_java tests/test_java_runner.cpp tests/macro_stub.cpp)
    target_link_libraries(test_java PRIVATE papiro_core Qt6::Widgets Qt6::Test)
    add_test(NAME java COMMAND test_java)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test-config)

    set_tests_properties(core editor java PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(mainwindow PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config")
endif()

# ---------------------------------------------------------------------------
# Install rules for packaging (must come BEFORE CPack include)
# ---------------------------------------------------------------------------
# Install binary
install(TARGETS Papiro
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

# Install translations (.qm files - fallback, also embedded via qt_add_translations)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/papiro_pt.qm
    ${CMAKE_CURRENT_BINARY_DIR}/papiro_en.qm
    ${CMAKE_CURRENT_BINARY_DIR}/papiro_es.qm
    DESTINATION share/papiro/translations
)

# Install syntax definitions (fallback, also embedded via qt_add_resources)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/syntax/
    DESTINATION share/papiro/syntax
    FILES_MATCHING PATTERN "*.syn"
)

# Install .desktop file
install(FILES resources/papiro.desktop DESTINATION share/applications)

# Install AppStream metainfo
install(FILES resources/papiro.metainfo.xml DESTINATION share/metainfo)

# Install icons (hicolor theme)
install(FILES resources/icons/papiro.svg DESTINATION share/icons/hicolor/scalable/apps RENAME papiro.svg)
install(FILES resources/icons/papiro-16.png DESTINATION share/icons/hicolor/16x16/apps RENAME papiro.png)
install(FILES resources/icons/papiro-24.png DESTINATION share/icons/hicolor/24x24/apps RENAME papiro.png)
install(FILES resources/icons/papiro-32.png DESTINATION share/icons/hicolor/32x32/apps RENAME papiro.png)
install(FILES resources/icons/papiro-48.png DESTINATION share/icons/hicolor/48x48/apps RENAME papiro.png)
install(FILES resources/icons/papiro-64.png DESTINATION share/icons/hicolor/64x64/apps RENAME papiro.png)
install(FILES resources/icons/papiro-128.png DESTINATION share/icons/hicolor/128x128/apps RENAME papiro.png)
install(FILES resources/icons/papiro-256.png DESTINATION share/icons/hicolor/256x256/apps RENAME papiro.png)

# Install copyright file (for DEB)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/copyright
    DESTINATION share/doc/papiro
    RENAME copyright
)

# Install changelog (for DEB) - proper Debian format, compressed
# Compress changelog with gzip (lintian requirement)
execute_process(
    COMMAND gzip -c ${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/changelog
    OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/changelog.gz
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/changelog.gz
    DESTINATION share/doc/papiro
    RENAME changelog.gz
)

# Install man page (compressed for Debian)
execute_process(
    COMMAND gzip -c ${CMAKE_CURRENT_SOURCE_DIR}/docs/papiro.1
    OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/papiro.1.gz
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/papiro.1.gz
    DESTINATION share/man/man1
    RENAME papiro.1.gz
)

# ---------------------------------------------------------------------------
# CPack Packaging Configuration (must be at the END)
# ---------------------------------------------------------------------------

# Common package metadata
set(CPACK_PACKAGE_NAME "Papiro")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Focused, cross-platform text editor")
set(CPACK_PACKAGE_VENDOR "Vinicius Werneck")
set(CPACK_PACKAGE_CONTACT "https://github.com/viniciuswerneck/papiro")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Papiro")

# Strip binary on install (reduces size, fixes lintian warning)
set(CMAKE_INSTALL_STRIP ON)

# Extended description for DEB (must not have lines > 80 chars)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Papiro is a modern, cross-platform text editor designed for developers "
    "and writers who value focus and efficiency. It provides syntax "
    "highlighting for 10+ programming languages, multiple cursors and "
    "selections, macro recording and playback, a clip library for snippets, "
    "spell checking via Hunspell, dark/light themes, and translations for "
    "Portuguese, English, and Spanish.")

# Consistent package file naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Desktop file for Linux packages
set(CPACK_DESKTOP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/papiro.desktop")

# ---------------------------------------------------------------------------
# Linux: DEB generator
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB;RPM;ZIP")

    # DEB-specific
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Vinicius Werneck <vinicius@example.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6, libqt6gui6, libqt6printsupport6, libqt6dbus6, libhunspell-1.7-0, libglib2.0-0, libfontconfig1, libfreetype6, libx11-6, libxcb1, libegl1, libgl1, libxkbcommon0")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_ARCHITECTURE "amd64")
    # Extra control fields file
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/debian-control-extra.txt")

    # Fix directory permissions
    set(CPACK_DEBIAN_FILE_PERMISSIONS
        "/usr/bin/Papiro=755"
        "/usr/share/applications/papiro.desktop=644"
        "/usr/share/metainfo/papiro.metainfo.xml=644"
        "/usr/share/icons/hicolor/256x256/apps/papiro.png=644"
        "/usr/share/icons/hicolor/scalable/apps/papiro.svg=644"
        "/usr/share/papiro/syntax/*.syn=644"
        "/usr/share/papiro/translations/*.qm=644"
        "/usr/share/doc/papiro/copyright=644"
        "/usr/share/doc/papiro/changelog=644"
    )

    # RPM-specific
    set(CPACK_RPM_PACKAGE_SUMMARY "A focused, cross-platform text editor")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.5, qt6-qtprintsupport >= 6.5, hunspell >= 1.7, glib2 >= 2.0, fontconfig, freetype, libX11, libxcb, libEGL, libGL, libxkbcommon")
    set(CPACK_RPM_COMPRESSION_TYPE "zstd")
endif()

# ---------------------------------------------------------------------------
# Windows: NSIS generator
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CPACK_GENERATOR "NSIS;ZIP")

    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\Programs")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_DISPLAY_NAME "Papiro ${PROJECT_VERSION}")
    set(CPACK_NSIS_HELP_LINK "https://github.com/viniciuswerneck/papiro")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/viniciuswerneck/papiro")
    set(CPACK_NSIS_CONTACT "viniciuswerneck@users.noreply.github.com")
    set(CPACK_NSIS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/papiro.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/papiro.ico")
    set(CPACK_NSIS_UNINSTALL_TITLE "Uninstall Papiro")
    set(CPACK_NSIS_COMPRESSOR "zstd")

    # Shortcuts
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "
        CreateShortCut '\$SMPROGRAMS\\Papiro\\Uninstall.lnk' '\$INSTDIR\\uninstall.exe' '' '\$INSTDIR\\uninstall.exe' 0
    ")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "
        Delete '\$SMPROGRAMS\\Papiro\\Uninstall.lnk'
        RMDir '\$SMPROGRAMS\\Papiro'
    ")

    # Portable ZIP
    set(CPACK_ZIP_PACKAGE_NAME "Papiro-${PROJECT_VERSION}-windows-x64-portable")
endif()

# ---------------------------------------------------------------------------
# Portable ZIP (cross-platform default)
# ---------------------------------------------------------------------------
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_ZIP_PACKAGE_NAME "Papiro-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Include CPack at the very end
include(CPack)


