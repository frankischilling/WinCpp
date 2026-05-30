#include <gtest/gtest.h>

#include "TreeFilterLogic.h"

TEST(TreeFilterLogicTest, MatchesCaseInsensitiveSubstring)
{
  EXPECT_TRUE(TreeFilterLogic::PathMatchesFilter(L"src\\Foo.cpp", L"foo"));
  EXPECT_FALSE(TreeFilterLogic::PathMatchesFilter(L"src\\Bar.cpp", L"foo"));
}

TEST(TreeFilterLogicTest, EmptyFilterMatchesEverything)
{
  EXPECT_TRUE(TreeFilterLogic::PathMatchesFilter(L"any.txt", L""));
}
