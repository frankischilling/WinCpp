#include <gtest/gtest.h>

#include "TabBar.h"

#include <string>
#include <vector>

class TabBarTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    parent_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 800, 40, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(parent_, nullptr);
    ASSERT_TRUE(TabBar::RegisterClass(GetModuleHandleW(nullptr)));
    ASSERT_TRUE(tabBar_.Create(parent_, GetModuleHandleW(nullptr), 100));
  }

  void TearDown() override
  {
    if (parent_)
    {
      DestroyWindow(parent_);
      parent_ = nullptr;
    }
  }

  HWND parent_ = nullptr;
  TabBar tabBar_;
};

TEST_F(TabBarTest, CreateInitializesHwnd)
{
  EXPECT_NE(tabBar_.Hwnd(), nullptr);
}

TEST_F(TabBarTest, SetTabsUpdatesSelection)
{
  tabBar_.SetTabs({L"a.cpp", L"b.cpp"}, 1);
  EXPECT_EQ(tabBar_.SelectedIndex(), 1);
}

TEST_F(TabBarTest, GetPreferredHeightPositive)
{
  tabBar_.SetTabs({L"tab"}, 0);
  EXPECT_GT(tabBar_.GetPreferredHeight(), 0);
}

TEST_F(TabBarTest, UpdateTitlePreservesSelection)
{
  tabBar_.SetTabs({L"old.cpp", L"other.cpp"}, 0);
  tabBar_.UpdateTitle(0, L"new.cpp");
  EXPECT_EQ(tabBar_.SelectedIndex(), 0);
}

TEST_F(TabBarTest, SetNotifyTargetStoresGroupId)
{
  HWND notify = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
  tabBar_.SetNotifyTarget(notify, 42);
  EXPECT_EQ(tabBar_.GroupId(), 42);
  DestroyWindow(notify);
}

TEST_F(TabBarTest, RevealTabDoesNotCrash)
{
  std::vector<std::wstring> titles;
  for (int i = 0; i < 8; ++i)
  {
    titles.push_back(L"very_long_tab_name_" + std::to_wstring(i) + L".cpp");
  }
  tabBar_.SetTabs(titles, 7);
  tabBar_.RevealTab(7);
  SUCCEED();
}

TEST_F(TabBarTest, GetChromeColorsReturnsValidValues)
{
  const TabBar::ChromeColors colors = TabBar::GetChromeColors(parent_);
  EXPECT_NE(colors.stripBackground, CLR_INVALID);
  EXPECT_NE(colors.inactiveForeground, CLR_INVALID);
}
