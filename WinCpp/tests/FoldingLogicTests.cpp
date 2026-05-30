#include <gtest/gtest.h>

#include "FoldingLogic.h"

TEST(FoldingLogicTest, DetectsBraceHeaderLines)
{
  const std::string text = "void f() {\n  int x;\n}\nclass C {\n";
  const auto lines = FoldingLogic::FoldHeaderLinesFromText(text);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0], 0);
  EXPECT_EQ(lines[1], 3);
}
