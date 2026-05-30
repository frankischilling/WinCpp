#include <gtest/gtest.h>

#include "RegexPattern.h"

#include <string>
#include <vector>

TEST(RegexPattern, MatchFindsSubstring)
{
  RegexPattern pattern;
  ASSERT_TRUE(pattern.Compile("\\bint\\b", nullptr));
  EXPECT_TRUE(pattern.Match("int x = 1;"));
  EXPECT_FALSE(pattern.Match("print"));
}

TEST(RegexPattern, ForEachMatchCollectsRanges)
{
  RegexPattern pattern;
  ASSERT_TRUE(pattern.Compile("a+", nullptr));

  std::vector<std::pair<size_t, size_t>> ranges;
  pattern.ForEachMatch("baaah", [&](size_t start, size_t end) {
    ranges.emplace_back(start, end);
  });

  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].first, 1u);
  EXPECT_EQ(ranges[0].second, 4u);
}

TEST(RegexPattern, InvalidPatternFailsCompile)
{
  RegexPattern pattern;
  std::string error;
  EXPECT_FALSE(pattern.Compile("([", &error));
  EXPECT_FALSE(error.empty());
}

TEST(RegexPattern, Utf8KeywordPattern)
{
  RegexPattern pattern;
  ASSERT_TRUE(pattern.Compile("\\breturn\\b", nullptr));
  EXPECT_TRUE(pattern.Match("  return 0;\n"));
}

TEST(RegexPattern, MoveConstructorTransfersPattern)
{
  RegexPattern first;
  ASSERT_TRUE(first.Compile("abc", nullptr));
  RegexPattern second(std::move(first));
  EXPECT_TRUE(second.IsValid());
  EXPECT_TRUE(second.Match("abc"));
  EXPECT_FALSE(first.IsValid());
}

TEST(RegexPattern, MoveAssignmentTransfersPattern)
{
  RegexPattern first;
  RegexPattern second;
  ASSERT_TRUE(first.Compile("xyz", nullptr));
  second = std::move(first);
  EXPECT_TRUE(second.Match("xyz"));
}

TEST(RegexPattern, ForEachMatchOnEmptyString)
{
  RegexPattern pattern;
  ASSERT_TRUE(pattern.Compile("a", nullptr));
  int count = 0;
  pattern.ForEachMatch("", [&](size_t, size_t) { ++count; });
  EXPECT_EQ(count, 0);
}

TEST(RegexPattern, ForEachMatchNonOverlapping)
{
  RegexPattern pattern;
  ASSERT_TRUE(pattern.Compile("aa", nullptr));
  std::vector<std::pair<size_t, size_t>> ranges;
  pattern.ForEachMatch("aaaa", [&](size_t start, size_t end) {
    ranges.emplace_back(start, end);
  });
  ASSERT_GE(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].first, 0u);
  EXPECT_EQ(ranges[0].second, 2u);
}
