#include <gtest/gtest.h>

#include "EditorView.h"
#include "EditorSettings.h"

#include <Scintilla.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
std::filesystem::path FixturesDir()
{
  return std::filesystem::path(WINCPP_TEST_FIXTURES_DIR);
}

class EditorViewTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    parent_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 800, 600, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(parent_, nullptr);
    ShowWindow(parent_, SW_SHOWNOACTIVATE);
    UpdateWindow(parent_);
    ASSERT_TRUE(editor_.Create(parent_, GetModuleHandleW(nullptr)));
  }

  void TearDown() override
  {
    if (parent_)
    {
      DestroyWindow(parent_);
      parent_ = nullptr;
    }
  }

  HWND parent_ = nullptr;
  EditorView editor_;
};
}

TEST_F(EditorViewTest, CreateInitializesHwnd)
{
  EXPECT_NE(editor_.Hwnd(), nullptr);
  EXPECT_FALSE(editor_.IsModified());
}

TEST_F(EditorViewTest, LoadAndSaveRoundTrip)
{
  const auto path = FixturesDir() / "sample.txt";
  std::string error;
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), &error)) << error;
  EXPECT_FALSE(editor_.IsModified());

  const auto outPath = std::filesystem::temp_directory_path() / L"WinCppTests_out.txt";
  std::filesystem::remove(outPath);
  ASSERT_TRUE(editor_.SaveToFile(outPath.wstring(), &error)) << error;
  EXPECT_FALSE(editor_.IsModified());

  std::ifstream in(outPath, std::ios::binary);
  ASSERT_TRUE(in.is_open());
  const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();
  EXPECT_NE(saved.find("hello world"), std::string::npos);
  std::filesystem::remove(outPath);
}

TEST_F(EditorViewTest, CreateDocumentReturnsDistinctBuffer)
{
  void* const original = editor_.GetDocumentPointer();
  ASSERT_NE(original, nullptr);

  void* const doc = editor_.CreateDocument();
  ASSERT_NE(doc, nullptr);
  EXPECT_NE(doc, original);

  editor_.ReleaseDocument(doc);
}

TEST_F(EditorViewTest, LoadFromFileIntoDocumentPreservesViewDocument)
{
  void* const primary = editor_.CreateDocument();
  editor_.SetDocument(primary);

  void* const aux = editor_.CreateDocument();
  ASSERT_NE(aux, nullptr);

  const auto path = FixturesDir() / "sample.cpp";
  ASSERT_TRUE(std::filesystem::exists(path));
  std::string error;
  ASSERT_TRUE(editor_.LoadFromFileIntoDocument(aux, path.wstring(), &error)) << error;
  EXPECT_EQ(editor_.GetDocumentPointer(), primary);

  editor_.SetDocument(aux);
  EXPECT_GT(SendMessage(editor_.Hwnd(), SCI_GETTEXTLENGTH, 0, 0), 0);
  editor_.SetDocument(primary);
  editor_.ReleaseDocument(aux);
}

TEST_F(EditorViewTest, FindNextForwardAndBackward)
{
  const auto path = FixturesDir() / "sample.txt";
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));

  EXPECT_TRUE(editor_.FindNext(L"line", false, false, true));
  EXPECT_TRUE(editor_.FindNext(L"line", false, false, false));
}

TEST_F(EditorViewTest, FindNextRespectsMatchCase)
{
  const auto path = FixturesDir() / "sample.txt";
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));

  EXPECT_TRUE(editor_.FindNext(L"hello", true, false, true));
  EXPECT_FALSE(editor_.FindNext(L"Hello", true, false, true));
}

TEST_F(EditorViewTest, ReplaceSelectionReplacesActiveMatch)
{
  const auto path = FixturesDir() / "sample.txt";
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));
  ASSERT_TRUE(editor_.FindNext(L"line", false, false, true));

  EXPECT_TRUE(editor_.ReplaceSelection(L"line", L"row", false, false));
  EXPECT_TRUE(editor_.FindNext(L"row", false, false, true));
}

TEST_F(EditorViewTest, ReplaceAll)
{
  const auto path = FixturesDir() / "sample.txt";
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));

  const int count = editor_.ReplaceAll(L"line", L"row", false, false);
  EXPECT_GE(count, 1);
}

TEST_F(EditorViewTest, GoToLineAndWordWrap)
{
  const auto path = FixturesDir() / "sample.txt";
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));

  editor_.GoToLine(2);
  EXPECT_EQ(editor_.GetCurrentLine(), 2);

  editor_.SetWordWrap(true);
  EXPECT_TRUE(editor_.IsWordWrapEnabled());
  editor_.SetWordWrap(false);
  EXPECT_FALSE(editor_.IsWordWrapEnabled());
}

TEST_F(EditorViewTest, ClearResetsContent)
{
  ASSERT_TRUE(editor_.LoadFromFile((FixturesDir() / "sample.txt").wstring(), nullptr));
  editor_.Clear();
  EXPECT_EQ(SendMessage(editor_.Hwnd(), SCI_GETTEXTLENGTH, 0, 0), 0);
}

TEST_F(EditorViewTest, FindNextRespectsWholeWord)
{
  const auto path = FixturesDir() / "find_wholeword.txt";
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));
  EXPECT_TRUE(editor_.FindNext(L"hello", false, true, true));

  editor_.Clear();
  SendMessage(editor_.Hwnd(), SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("helloworld"));
  EXPECT_FALSE(editor_.FindNext(L"hello", false, true, true));
}

TEST_F(EditorViewTest, IsModifiedAfterEditAndClearsOnSetSavePoint)
{
  ASSERT_TRUE(editor_.LoadFromFile((FixturesDir() / "sample.txt").wstring(), nullptr));
  EXPECT_FALSE(editor_.IsModified());

  SendMessage(editor_.Hwnd(), SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("x"));
  EXPECT_TRUE(editor_.IsModified());

  editor_.SetSavePoint();
  EXPECT_FALSE(editor_.IsModified());
}

TEST_F(EditorViewTest, GetCurrentLineAndColumnAfterGoToLine)
{
  ASSERT_TRUE(editor_.LoadFromFile((FixturesDir() / "sample.txt").wstring(), nullptr));
  editor_.GoToLine(2);
  EXPECT_EQ(editor_.GetCurrentLine(), 2);
  EXPECT_GE(editor_.GetCurrentColumn(), 1);
}

TEST_F(EditorViewTest, LoadFromFileMissingReturnsFalse)
{
  std::string error;
  const auto missing = FixturesDir() / "does_not_exist_12345.txt";
  EXPECT_FALSE(editor_.LoadFromFile(missing.wstring(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(EditorViewTest, SaveToFileFailsOnInvalidPath)
{
  std::string error;
  const std::wstring badPath = L"Z:\\no_such_drive\\path\\file.txt";
  EXPECT_FALSE(editor_.SaveToFile(badPath, &error));
}

TEST_F(EditorViewTest, FindNextEmptySearchReturnsFalse)
{
  ASSERT_TRUE(editor_.LoadFromFile((FixturesDir() / "sample.txt").wstring(), nullptr));
  EXPECT_FALSE(editor_.FindNext(L"", false, false, true));
}

TEST_F(EditorViewTest, ReplaceAllExactCount)
{
  const auto path = FixturesDir() / "sample.txt";
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));
  const int count = editor_.ReplaceAll(L"line", L"row", false, false);
  EXPECT_EQ(count, 1);
}

TEST_F(EditorViewTest, ApplySettingsSetsTabWidth)
{
  EditorSettings settings;
  settings.tabSize = 2;
  settings.tabToSpaces = true;
  editor_.ApplySettings(settings);
  EXPECT_EQ(SendMessage(editor_.Hwnd(), SCI_GETTABWIDTH, 0, 0), 2);
  EXPECT_EQ(SendMessage(editor_.Hwnd(), SCI_GETUSETABS, 0, 0), 0);
}

TEST_F(EditorViewTest, DuplicateLineIncreasesLineCount)
{
  ASSERT_TRUE(editor_.LoadFromFile((FixturesDir() / "sample.txt").wstring(), nullptr));
  const LRESULT before = SendMessage(editor_.Hwnd(), SCI_GETLINECOUNT, 0, 0);
  editor_.DuplicateLine();
  EXPECT_EQ(SendMessage(editor_.Hwnd(), SCI_GETLINECOUNT, 0, 0), before + 1);
}

TEST_F(EditorViewTest, TrimTrailingWhitespace)
{
  ASSERT_TRUE(editor_.LoadFromFile((FixturesDir() / "trailing_ws.txt").wstring(), nullptr));
  EXPECT_GE(editor_.TrimTrailingWhitespace(), 1);
}

TEST_F(EditorViewTest, ZoomDeltaChangesZoom)
{
  const int before = editor_.GetZoom();
  editor_.ZoomDelta(1);
  EXPECT_EQ(editor_.GetZoom(), before + 1);
}

TEST_F(EditorViewTest, FindNextWithRegex)
{
  ASSERT_TRUE(editor_.LoadFromFile((FixturesDir() / "sample.txt").wstring(), nullptr));
  SearchOptions options;
  options.regex = true;
  EXPECT_TRUE(editor_.FindNext(L"line", options));
}

TEST_F(EditorViewTest, GotoMatchingBraceOnParentheses)
{
  SendMessage(editor_.Hwnd(), SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("(x)"));
  SendMessage(editor_.Hwnd(), SCI_GOTOPOS, 0, 0);
  editor_.SetBraceMatching(true);
  EXPECT_TRUE(editor_.GotoMatchingBrace());
}
