#include <gtest/gtest.h>

#include "CommandRegistry.h"

TEST(CommandRegistryTest, FilterMatchesLabel)
{
  const auto all = CommandRegistry::DefaultCommands();
  const auto filtered = CommandRegistry::Filter(all, L"find");
  ASSERT_FALSE(filtered.empty());
  EXPECT_NE(filtered.front().label.find(L"Find"), std::wstring::npos);
}

TEST(CommandRegistryTest, EmptyQueryReturnsAll)
{
  const auto all = CommandRegistry::DefaultCommands();
  EXPECT_EQ(CommandRegistry::Filter(all, L"").size(), all.size());
}
