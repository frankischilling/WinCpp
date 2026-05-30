# CMakeList.txt : CMake project for WinCpp, include source and define
# project specific logic here.
#

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)

FetchContent_Declare(
  scintilla
  GIT_REPOSITORY https://github.com/mirror/scintilla.git
  GIT_TAG rel-5-5-2
)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)

FetchContent_Declare(
  pcre2
  GIT_REPOSITORY https://github.com/PCRE2Project/pcre2.git
  GIT_TAG pcre2-10.44
)

set(PCRE2_BUILD_PCRE2_8 ON CACHE BOOL "Build PCRE2 8-bit" FORCE)
set(PCRE2_BUILD_PCRE2_16 OFF CACHE BOOL "Build PCRE2 16-bit" FORCE)
set(PCRE2_BUILD_PCRE2_32 OFF CACHE BOOL "Build PCRE2 32-bit" FORCE)
set(PCRE2_SUPPORT_JIT OFF CACHE BOOL "Disable PCRE2 JIT" FORCE)

FetchContent_MakeAvailable(yaml-cpp pcre2)

FetchContent_GetProperties(scintilla)
if (NOT scintilla_POPULATED)
  FetchContent_Populate(scintilla)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/Scintilla.cmake)
endif()

set(WINCPP_INCLUDE_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/app"
  "${CMAKE_CURRENT_LIST_DIR}/core"
  "${CMAKE_CURRENT_LIST_DIR}/editor"
  "${CMAKE_CURRENT_LIST_DIR}/platform"
  "${CMAKE_CURRENT_LIST_DIR}/project"
  "${CMAKE_CURRENT_LIST_DIR}/storage"
  "${CMAKE_CURRENT_LIST_DIR}/syntax_engine"
  "${CMAKE_CURRENT_LIST_DIR}/ui"
)

# Add source to this project's executable.
add_executable (WinCpp WIN32
  "WinCpp.manifest"
  "app/WinCpp.cpp"
  "app/WinCpp.h"
  "app/MainWindow.h"
  "app/MainWindow.cpp"
  "app/MainWindowConstants.h"
  "app/MainWindowDialogs.cpp"
  "app/MainWindowDocuments.cpp"
  "app/MainWindowInternal.h"
  "app/MainWindowInternal.cpp"
  "app/MainWindowMessages.cpp"
  "app/MainWindowSession.cpp"
  "core/AppIds.h"
  "core/CommandRegistry.h"
  "core/CommandRegistry.cpp"
  "editor/DocumentCollectionLogic.h"
  "editor/DocumentCollectionLogic.cpp"
  "editor/DocumentNaming.h"
  "editor/DocumentNaming.cpp"
  "editor/EditorSearch.h"
  "editor/EditorSearch.cpp"
  "editor/EditorSettings.h"
  "editor/EditorSettings.cpp"
  "editor/EditorSplitDrop.h"
  "editor/EditorSplitDrop.cpp"
  "editor/EditorView.h"
  "editor/EditorView.cpp"
  "editor/EditorWorkspace.h"
  "editor/EditorWorkspace.cpp"
  "editor/EditorWorkspaceDrag.cpp"
  "editor/EditorWorkspaceTabs.cpp"
  "editor/FoldingLogic.h"
  "editor/FoldingLogic.cpp"
  "editor/IndentLogic.h"
  "editor/IndentLogic.cpp"
  "platform/ProcessOutput.h"
  "platform/ProcessOutput.cpp"
  "project/MainWindowProject.cpp"
  "project/PathLineParser.h"
  "project/PathLineParser.cpp"
  "project/TreeFilterLogic.h"
  "project/TreeFilterLogic.cpp"
  "storage/ConfigJson.h"
  "storage/ConfigJson.cpp"
  "storage/RecentFilesStore.h"
  "storage/RecentFilesStore.cpp"
  "storage/SessionState.h"
  "storage/SessionState.cpp"
  "syntax_engine/RegexPattern.h"
  "syntax_engine/RegexPattern.cpp"
  "syntax_engine/SyntaxHighlighter.h"
  "syntax_engine/SyntaxHighlighter.cpp"
  "syntax_engine/SyntaxRegistry.h"
  "syntax_engine/SyntaxRegistry.cpp"
  "syntax_engine/ThemeLoader.h"
  "syntax_engine/ThemeLoader.cpp"
  "ui/CommandPalette.h"
  "ui/CommandPalette.cpp"
  "ui/DialogLayout.h"
  "ui/DialogLayout.cpp"
  "ui/MainWindowChrome.cpp"
  "ui/StatusBarModel.h"
  "ui/StatusBarModel.cpp"
  "ui/TabBar.h"
  "ui/TabBar.cpp"
  "ui/TabBarLogic.h"
  "ui/TabBarLogic.cpp"
  "ui/UiHelpers.h"
  "ui/UiHelpers.cpp"
  "ui/UiMetrics.h"
  "ui/UiMetrics.cpp"
  "ui/UiTheme.h"
  "ui/UiTheme.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET WinCpp PROPERTY CXX_STANDARD 20)
endif()

target_compile_definitions(WinCpp PRIVATE UNICODE _UNICODE INCLUDE_DEPRECATED_FEATURES)
target_link_libraries(WinCpp PRIVATE comctl32 shell32 ole32 msimg32 uxtheme scintilla yaml-cpp pcre2-8)
target_include_directories(WinCpp PRIVATE
  ${WINCPP_INCLUDE_DIRS}
  "${scintilla_SOURCE_DIR}/include"
  "${scintilla_SOURCE_DIR}/src"
  "${scintilla_SOURCE_DIR}/win32"
)

add_custom_command(TARGET WinCpp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_LIST_DIR}/syntax"
          "$<TARGET_FILE_DIR:WinCpp>/syntax"
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../tests ${CMAKE_CURRENT_BINARY_DIR}/tests)

