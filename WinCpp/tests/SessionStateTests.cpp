#include <gtest/gtest.h>

#include "SessionState.h"

TEST(SessionStateTest, RoundTripSerialization)
{
  SessionState original;
  original.activeGroupId = 2;
  original.projectRoot = L"C:\\Projects\\WinCpp";
  original.showProjectPane = true;
  original.tabs.push_back({L"C:\\a.txt", 0, true});
  original.tabs.push_back({L"C:\\b.txt", 1, false});

  SessionState loaded;
  ASSERT_TRUE(SessionStateCodec::Deserialize(SessionStateCodec::Serialize(original), &loaded));
  ASSERT_EQ(loaded.tabs.size(), 2u);
  EXPECT_EQ(loaded.tabs[0].path, L"C:\\a.txt");
  EXPECT_TRUE(loaded.tabs[0].pinned);
  EXPECT_EQ(loaded.projectRoot, L"C:\\Projects\\WinCpp");
  EXPECT_TRUE(loaded.showProjectPane);
}

TEST(SessionStateTest, RoundTripProjectRootWithoutTabs)
{
  SessionState original;
  original.projectRoot = L"D:\\repo";
  original.showProjectPane = true;

  SessionState loaded;
  ASSERT_TRUE(SessionStateCodec::Deserialize(SessionStateCodec::Serialize(original), &loaded));
  EXPECT_TRUE(loaded.tabs.empty());
  EXPECT_EQ(loaded.projectRoot, L"D:\\repo");
  EXPECT_TRUE(loaded.showProjectPane);
}
