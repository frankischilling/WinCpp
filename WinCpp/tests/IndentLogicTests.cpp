#include <gtest/gtest.h>

#include "IndentLogic.h"

TEST(IndentLogicTest, CopiesLeadingWhitespace)
{
  EXPECT_EQ(IndentLogic::ComputeNewLineIndent("    hello", 4, true), "    ");
}

TEST(IndentLogicTest, AddsExtraIndentAfterOpenBrace)
{
  EXPECT_EQ(IndentLogic::ComputeNewLineIndent("void f() {", 4, true), "    ");
  EXPECT_EQ(IndentLogic::ComputeNewLineIndent("if (x) {", 2, true), "  ");
}

TEST(IndentLogicTest, UsesTabForExtraIndentWhenConfigured)
{
  const std::string indent = IndentLogic::ComputeNewLineIndent("if (x) {", 4, false);
  EXPECT_EQ(indent, "\t");
}
