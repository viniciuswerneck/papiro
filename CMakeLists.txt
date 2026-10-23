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
qt_add_executable(Papiro WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/Document.cpp
    src/editor/Editor.cpp
    src/editor/SyntaxHighlighter.cpp
    src/ui/ClipLibraryWidget.cpp
    src/ui/FileExplorerWidget.cpp
    src/ui/ToolsDialog.cpp
    src/ui/FindReplaceDialog.cpp
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
# Tests (disabled for now)
# ---------------------------------------------------------------------------
# include(CTest)
# if(BUILD_TESTING)
#     find_package(Qt6 REQUIRED COMPONENTS Test)
#
#     qt_add_executable(test_core tests/test_core.cpp)
#     target_link_libraries(test_core PRIVATE papiro_core Qt6::Test)
#     add_test(NAME core COMMAND test_core)
# endif()
