#include <gtest/gtest.h>

#include "TabBar.h"

TEST(TabBarMessage, PackUnpackScreenPointRoundTrip)
{
  const POINT original{1920, 1050};
  const LPARAM packed = PackScreenPoint(original);
  const POINT restored = UnpackScreenPoint(packed);
  EXPECT_EQ(restored.x, original.x);
  EXPECT_EQ(restored.y, original.y);
}

TEST(TabBarMessage, PackUnpackLargeCoordinates)
{
  const POINT original{3840, 2160};
  const POINT restored = UnpackScreenPoint(PackScreenPoint(original));
  EXPECT_EQ(restored.x, 3840);
  EXPECT_EQ(restored.y, 2160);
}
