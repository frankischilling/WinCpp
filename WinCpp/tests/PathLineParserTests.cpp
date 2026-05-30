#include <gtest/gtest.h>

#include "PathLineParser.h"

TEST(PathLineParserTest, ParsesWindowsStylePathAndLine)
{
  const auto target = PathLineParser::Parse(L"src\\foo.cpp:42");
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->path, L"src\\foo.cpp");
  EXPECT_EQ(target->line, 42);
}

TEST(PathLineParserTest, RejectsMissingLine)
{
  EXPECT_FALSE(PathLineParser::Parse(L"src\\foo.cpp").has_value());
}
