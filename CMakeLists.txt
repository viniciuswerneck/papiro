cmake_minimum_required(VERSION 3.21)

project(Papiro
    VERSION 0.1.0
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
    src/ui/ClipLibraryWidget.cpp
    src/ui/Theme.cpp
    src/ui/FileExplorerWidget.cpp
    src/ui/MacroManagerDialog.cpp
    src/ui/PreferencesDialog.cpp
    src/ui/ToolsDialog.cpp
    src/ui/FindReplaceDialog.cpp
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
        src/ui/Theme.cpp
    )
    target_link_libraries(test_editor PRIVATE papiro_core Qt6::Widgets Qt6::Test)
    add_test(NAME editor COMMAND test_editor)

    # MainWindow integration tests (session restore and friends).
    qt_add_executable(test_mainwindow
        tests/test_mainwindow.cpp
        ${PAPIRO_APP_SOURCES}
    )
    target_link_libraries(test_mainwindow PRIVATE papiro_core Qt6::Widgets
                          Qt6::PrintSupport Qt6::Test)
    add_test(NAME mainwindow COMMAND test_mainwindow)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test-config)

    set_tests_properties(core editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(mainwindow PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config")
endif()

# ---------------------------------------------------------------------------
# CPack Packaging Configuration
# ---------------------------------------------------------------------------
include(CPack)

# Common package metadata
set(CPACK_PACKAGE_NAME "Papiro")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A focused, cross-platform text editor")
set(CPACK_PACKAGE_VENDOR "Papiro Project")
set(CPACK_PACKAGE_CONTACT "https://github.com/usuario/papiro")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Papiro")

# Consistent package file naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# ---------------------------------------------------------------------------
# DEB (Linux)
# ---------------------------------------------------------------------------
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Papiro Project")
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6, libqt6printsupport6, libgl1")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Extra control fields file
set(CPACK_DEBIAN_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/debian-control-extra.txt")

# ---------------------------------------------------------------------------
# RPM (Linux)
# ---------------------------------------------------------------------------
set(CPACK_RPM_PACKAGE_SUMMARY "A focused, cross-platform text editor")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtprintsupport")

# ---------------------------------------------------------------------------
# NSIS (Windows)
# ---------------------------------------------------------------------------
set(CPACK_NSIS_PACKAGE_NAME "Papiro")
set(CPACK_NSIS_DISPLAY_NAME "Papiro Text Editor")
set(CPACK_NSIS_HELP_URL "https://github.com/usuario/papiro")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/usuario/papiro")
set(CPACK_NSIS_CONTACT "usuario@exemplo.com")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/papiro.ico")
set(CPACK_NSIS_UNINSTALL_TITLE "Uninstall Papiro")
set(CPACK_NSIS_COMPRESSOR "zstd")

# ---------------------------------------------------------------------------
# Portable ZIP (cross-platform)
# ---------------------------------------------------------------------------
set(CPACK_GENERATOR "ZIP")
set(CPACK_ZIP_PACKAGE_NAME "Papiro-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# ---------------------------------------------------------------------------
# Install rules for packaging
# ---------------------------------------------------------------------------
# Install binary
install(TARGETS Papiro RUNTIME DESTINATION bin)

# Install .desktop file
install(FILES resources/papiro.desktop DESTINATION share/applications)

# Install AppStream metainfo
install(FILES resources/papiro.metainfo.xml DESTINATION share/metainfo)

# Install icons
install(FILES resources/icons/papiro.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES resources/icons/papiro.png DESTINATION share/icons/hicolor/256x256/apps)


