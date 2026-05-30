#include <gtest/gtest.h>

#include "StatusBarModel.h"

TEST(StatusBarModelTest, FormatsLineColumnAndLanguage)
{
  StatusBarModelInput input;
  input.line = 3;
  input.column = 10;
  input.languageName = L"C++";
  input.modified = true;
  const std::wstring text = StatusBarModel::Format(input);
  EXPECT_NE(text.find(L"Ln 3"), std::wstring::npos);
  EXPECT_NE(text.find(L"C++"), std::wstring::npos);
  EXPECT_NE(text.find(L"Modified"), std::wstring::npos);
}

TEST(StatusBarModelTest, ShowsLfByDefault)
{
  StatusBarModelInput input;
  const std::wstring text = StatusBarModel::Format(input);
  EXPECT_NE(text.find(L"LF"), std::wstring::npos);
}
