include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(WINCPP_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/EditorSplitDrop.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/RegexPattern.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/SyntaxRegistry.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/RecentFilesStore.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/ConfigJson.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/EditorSettings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/IndentLogic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/EditorSearch.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/StatusBarModel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/DocumentCollectionLogic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/PathLineParser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/CommandRegistry.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/TreeFilterLogic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/SessionState.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/ProcessOutput.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/FoldingLogic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/ThemeLoader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/TabBarLogic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/EditorView.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/SyntaxHighlighter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/UiHelpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/EditorWorkspace.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/TabBar.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/DocumentNaming.cpp
)

set(WINCPP_TEST_HEADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp)

add_executable(WinCppTests
  WinCppTestsMain.cpp
  EditorSplitDropTests.cpp
  RegexPatternTests.cpp
  SyntaxRegistryTests.cpp
  RecentFilesStoreTests.cpp
  EditorSettingsTests.cpp
  IndentLogicTests.cpp
  EditorSearchTests.cpp
  StatusBarModelTests.cpp
  DocumentCollectionLogicTests.cpp
  PathLineParserTests.cpp
  CommandRegistryTests.cpp
  TreeFilterLogicTests.cpp
  SessionStateTests.cpp
  ProcessOutputTests.cpp
  FoldingLogicTests.cpp
  ThemeLoaderTests.cpp
  TabBarLogicTests.cpp
  TabBarMessageTests.cpp
  EditorViewTests.cpp
  SyntaxHighlighterTests.cpp
  DocumentNamingTests.cpp
  TabBarTests.cpp
  EditorWorkspaceTests.cpp
  UiHelpersTests.cpp
  ${WINCPP_TEST_SOURCES}
)

target_compile_definitions(WinCppTests PRIVATE
  UNICODE
  _UNICODE
  INCLUDE_DEPRECATED_FEATURES
  WINCPP_TEST_BUILD
  WINCPP_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_include_directories(WinCppTests PRIVATE
  ${WINCPP_TEST_HEADERS_DIR}
  "${scintilla_SOURCE_DIR}/include"
  "${scintilla_SOURCE_DIR}/src"
  "${scintilla_SOURCE_DIR}/win32"
)

target_link_libraries(WinCppTests PRIVATE
  GTest::gtest
  yaml-cpp
  pcre2-8
  scintilla
  comctl32
  shell32
  ole32
  oleaut32
  uuid
  msimg32
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET WinCppTests PROPERTY CXX_STANDARD 20)
endif()

include(GoogleTest)
gtest_discover_tests(WinCppTests)

add_custom_command(TARGET WinCppTests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/syntax"
          "$<TARGET_FILE_DIR:WinCppTests>/syntax"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/../WinCpp/syntax"
          "$<TARGET_FILE_DIR:WinCppTests>/syntax_full"
)
