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

# Add source to this project's executable.
add_executable (WinCpp WIN32
  "WinCpp.cpp"
  "WinCpp.h"
  "AppIds.h"
  "MainWindow.h"
  "MainWindow.cpp"
  "TabBar.h"
  "TabBar.cpp"
  "TabBarLogic.h"
  "TabBarLogic.cpp"
  "EditorWorkspace.h"
  "EditorWorkspace.cpp"
  "EditorSplitDrop.h"
  "EditorSplitDrop.cpp"
  "UiHelpers.h"
  "UiHelpers.cpp"
  "RecentFilesStore.h"
  "RecentFilesStore.cpp"
  "ConfigJson.h"
  "ConfigJson.cpp"
  "EditorSettings.h"
  "EditorSettings.cpp"
  "IndentLogic.h"
  "IndentLogic.cpp"
  "EditorSearch.h"
  "EditorSearch.cpp"
  "StatusBarModel.h"
  "StatusBarModel.cpp"
  "DocumentCollectionLogic.h"
  "DocumentCollectionLogic.cpp"
  "PathLineParser.h"
  "PathLineParser.cpp"
  "CommandRegistry.h"
  "CommandRegistry.cpp"
  "TreeFilterLogic.h"
  "TreeFilterLogic.cpp"
  "SessionState.h"
  "SessionState.cpp"
  "ProcessOutput.h"
  "ProcessOutput.cpp"
  "FoldingLogic.h"
  "FoldingLogic.cpp"
  "ThemeLoader.h"
  "ThemeLoader.cpp"
  "EditorView.h"
  "EditorView.cpp"
  "RegexPattern.h"
  "RegexPattern.cpp"
  "SyntaxRegistry.h"
  "SyntaxRegistry.cpp"
  "SyntaxHighlighter.h"
  "SyntaxHighlighter.cpp"
  "DocumentNaming.h"
  "DocumentNaming.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET WinCpp PROPERTY CXX_STANDARD 20)
endif()

target_compile_definitions(WinCpp PRIVATE UNICODE _UNICODE INCLUDE_DEPRECATED_FEATURES)
target_link_libraries(WinCpp PRIVATE comctl32 shell32 ole32 msimg32 scintilla yaml-cpp pcre2-8)
target_include_directories(WinCpp PRIVATE
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

