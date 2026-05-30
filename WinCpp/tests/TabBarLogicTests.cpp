#include <gtest/gtest.h>

#include "TabBarLogic.h"

#include <vector>

namespace
{
std::vector<RECT> ThreeTabs()
{
  return {
      {0, 0, 100, 28},
      {100, 0, 200, 28},
      {200, 0, 300, 28},
  };
}
}

TEST(TabBarLogic, InsertBeforeFirstWhenLeftOfStrip)
{
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(-5, 0, ThreeTabs()), 0);
}

TEST(TabBarLogic, InsertAfterLastWhenRightOfStrip)
{
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(310, 0, ThreeTabs()), 3);
}

TEST(TabBarLogic, LeftHalfOfTabInsertsBefore)
{
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(40, 0, ThreeTabs()), 0);
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(140, 0, ThreeTabs()), 1);
}

TEST(TabBarLogic, RightHalfOfTabInsertsAfter)
{
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(60, 0, ThreeTabs()), 1);
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(260, 0, ThreeTabs()), 3);
}

TEST(TabBarLogic, ScrollOffsetShiftsHitTest)
{
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(10, 100, ThreeTabs()), 1);
}

TEST(TabBarLogic, EmptyTabsReturnsZero)
{
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(5, 0, {}), 0);
}

TEST(TabBarLogic, SingleTabLeftHalfInsertsBefore)
{
  const std::vector<RECT> oneTab{{0, 0, 100, 28}};
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(40, 0, oneTab), 0);
}

TEST(TabBarLogic, SingleTabRightHalfInsertsAfter)
{
  const std::vector<RECT> oneTab{{0, 0, 100, 28}};
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(60, 0, oneTab), 1);
}

TEST(TabBarLogic, BoundaryAtTabEdge)
{
  const std::vector<RECT> tabs{{0, 0, 100, 28}, {100, 0, 200, 28}};
  EXPECT_EQ(TabBarLogic::HitTestInsertIndex(100, 0, tabs), 1);
}
