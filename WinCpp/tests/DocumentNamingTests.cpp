#include <gtest/gtest.h>

#include "DocumentNaming.h"

TEST(DocumentNaming, TabLabelForPathEmptyReturnsUntitled)
{
  EXPECT_EQ(DocumentNaming::TabLabelForPath(L""), L"Untitled");
}

TEST(DocumentNaming, TabLabelForPathUsesFilename)
{
  EXPECT_EQ(DocumentNaming::TabLabelForPath(L"C:\\a\\b.cpp"), L"b.cpp");
}

TEST(DocumentNaming, DisplayTitleUnmodified)
{
  EXPECT_EQ(DocumentNaming::DisplayTitle(L"C:\\x\\y.txt", L"fallback", false), L"y.txt");
}

TEST(DocumentNaming, DisplayTitleModifiedAddsStar)
{
  EXPECT_EQ(DocumentNaming::DisplayTitle(L"C:\\x\\y.txt", L"fallback", true), L"y.txt *");
}

TEST(DocumentNaming, DisplayTitleUntitledUsesTabTitle)
{
  EXPECT_EQ(DocumentNaming::DisplayTitle(L"", L"Untitled-1", false), L"Untitled-1");
  EXPECT_EQ(DocumentNaming::DisplayTitle(L"", L"Untitled-1", true), L"Untitled-1 *");
}
