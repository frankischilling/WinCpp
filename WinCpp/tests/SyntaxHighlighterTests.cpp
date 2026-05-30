#include <gtest/gtest.h>

#include "EditorView.h"
#include "SyntaxHighlighter.h"
#include "SyntaxRegistry.h"

#include <filesystem>
#include <fstream>
#include <Scintilla.h>

namespace
{
std::filesystem::path FixturesDir()
{
  return std::filesystem::path(WINCPP_TEST_FIXTURES_DIR);
}

class SyntaxHighlighterTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    parent_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 640, 480, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(parent_, nullptr);
    ASSERT_TRUE(editor_.Create(parent_, GetModuleHandleW(nullptr)));

    const auto syntaxFull = FixturesDir() / "syntax_full";
    const auto bundledSyntax = FixturesDir() / ".." / ".." / "WinCpp" / "syntax";
    const std::filesystem::path syntaxDir =
        std::filesystem::exists(syntaxFull) ? syntaxFull : bundledSyntax;
    SyntaxRegistry::LoadSharedDirectory(syntaxDir, nullptr);
  }

  void TearDown() override
  {
    if (parent_)
    {
      DestroyWindow(parent_);
    }
  }

  unsigned char StyleAt(HWND hwnd, int position)
  {
    return static_cast<unsigned char>(SendMessage(hwnd, SCI_GETSTYLEAT, position, 0));
  }

  HWND parent_ = nullptr;
  EditorView editor_;
};
}

TEST_F(SyntaxHighlighterTest, ApplySyntaxColorsCppKeywords)
{
  const auto cppPath = FixturesDir() / "sample.cpp";
  std::string error;
  ASSERT_TRUE(editor_.LoadFromFile(cppPath.wstring(), &error)) << error;

  editor_.ApplySyntaxForPath(cppPath.wstring());

  const HWND hwnd = editor_.Hwnd();
  ASSERT_GT(SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0), 0);

  bool foundStyledByte = false;
  const int length = static_cast<int>(SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0));
  for (int i = 0; i < length; ++i)
  {
    if (StyleAt(hwnd, i) != 0)
    {
      foundStyledByte = true;
      break;
    }
  }
  EXPECT_TRUE(foundStyledByte);
}

TEST_F(SyntaxHighlighterTest, ApplySyntaxNoOpForUnknownExtension)
{
  const auto path = FixturesDir() / "sample.unknownext";
  {
    std::ofstream out(path);
    out << "plain text\n";
  }

  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));
  editor_.ApplySyntaxForPath(path.wstring());

  const int length = static_cast<int>(SendMessage(editor_.Hwnd(), SCI_GETTEXTLENGTH, 0, 0));
  int nonDefault = 0;
  for (int i = 0; i < length; ++i)
  {
    if (StyleAt(editor_.Hwnd(), i) != 0)
    {
      ++nonDefault;
    }
  }
  EXPECT_EQ(nonDefault, 0);

  std::filesystem::remove(path);
}

TEST_F(SyntaxHighlighterTest, ApplySyntaxColorsCommentAndStatement)
{
  SyntaxRegistry registry;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", nullptr));

  const auto path = FixturesDir() / "sample.testlang";
  {
    std::ofstream out(path);
    out << "# comment line\nhello world\n";
  }

  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));
  const SyntaxDefinition* def = registry.DetectForPath(path.wstring(), "");
  ASSERT_NE(def, nullptr);

  SyntaxHighlighter highlighter;
  highlighter.Attach(editor_.Hwnd());
  highlighter.Apply(*def);

  const HWND hwnd = editor_.Hwnd();
  const unsigned char hashStyle = StyleAt(hwnd, 0);
  const unsigned char helloStyle = StyleAt(hwnd, 15);
  EXPECT_NE(hashStyle, 0u);
  EXPECT_NE(helloStyle, 0u);
  EXPECT_NE(hashStyle, helloStyle);

  std::filesystem::remove(path);
}

TEST_F(SyntaxHighlighterTest, ApplySyntaxForPathThroughEditorView)
{
  const auto cppPath = FixturesDir() / "sample.cpp";
  ASSERT_TRUE(editor_.LoadFromFile(cppPath.wstring(), nullptr));
  editor_.ApplySyntaxForPath(cppPath.wstring());

  bool foundStyled = false;
  const int length = static_cast<int>(SendMessage(editor_.Hwnd(), SCI_GETTEXTLENGTH, 0, 0));
  for (int i = 0; i < length; ++i)
  {
    if (StyleAt(editor_.Hwnd(), i) != 0)
    {
      foundStyled = true;
      break;
    }
  }
  EXPECT_TRUE(foundStyled);
}

TEST_F(SyntaxHighlighterTest, ApplySyntaxIdempotent)
{
  SyntaxRegistry registry;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", nullptr));
  const auto path = FixturesDir() / "sample.testlang";
  {
    std::ofstream out(path);
    out << "hello\n";
  }
  ASSERT_TRUE(editor_.LoadFromFile(path.wstring(), nullptr));
  const SyntaxDefinition* def = registry.DetectForPath(path.wstring(), "");
  ASSERT_NE(def, nullptr);

  SyntaxHighlighter highlighter;
  highlighter.Attach(editor_.Hwnd());
  highlighter.Apply(*def);
  const unsigned char afterFirst = StyleAt(editor_.Hwnd(), 0);
  highlighter.Apply(*def);
  const unsigned char afterSecond = StyleAt(editor_.Hwnd(), 0);
  EXPECT_EQ(afterFirst, afterSecond);

  std::filesystem::remove(path);
}
