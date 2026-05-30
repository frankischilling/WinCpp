#include <gtest/gtest.h>

#include "EditorSearch.h"

TEST(EditorSearchTest, FindsPlainTextForward)
{
  const std::string text = "foo bar foo";
  SearchOptions options;
  options.forward = true;
  SearchMatch match{};
  ASSERT_TRUE(EditorSearch::FindNextInText(text, 0, 0, L"foo", options, &match, nullptr));
  EXPECT_EQ(match.start, 0u);
}

TEST(EditorSearchTest, FindsRegexPattern)
{
  const std::string text = "item1 item22";
  SearchOptions options;
  options.regex = true;
  SearchMatch match{};
  std::string error;
  ASSERT_TRUE(EditorSearch::FindNextInText(text, 0, 0, L"item[0-9]+", options, &match, &error));
  EXPECT_EQ(match.start, 0u);
}

TEST(EditorSearchTest, ReplaceAllPlainText)
{
  std::string text = "aaa";
  SearchOptions options;
  const int count = EditorSearch::ReplaceAllInText(text, L"a", L"b", options, nullptr);
  EXPECT_EQ(count, 3);
  EXPECT_EQ(text, "bbb");
}

TEST(EditorSearchTest, InvalidRegexReturnsError)
{
  SearchOptions options;
  options.regex = true;
  SearchMatch match{};
  std::string error;
  EXPECT_FALSE(EditorSearch::FindNextInText("abc", 0, 0, L"(", options, &match, &error));
  EXPECT_FALSE(error.empty());
}
