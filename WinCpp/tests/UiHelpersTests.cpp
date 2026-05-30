#include <gtest/gtest.h>

#include "UiHelpers.h"

TEST(UiHelpers, GetUiFontReturnsNonNull)
{
  HFONT font = GetUiFont();
  EXPECT_NE(font, nullptr);
}

TEST(UiHelpers, ApplyUiFontOnStaticControl)
{
  HWND hwnd = CreateWindowExW(0, L"STATIC", L"test", WS_POPUP, 0, 0, 100, 30, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
  ASSERT_NE(hwnd, nullptr);
  ApplyUiFont(hwnd);
  DestroyWindow(hwnd);
}

TEST(UiHelpers, ReleaseUiFontIdempotent)
{
  EXPECT_NE(GetUiFont(), nullptr);
  ReleaseUiFont();
  ReleaseUiFont();
  EXPECT_NE(GetUiFont(), nullptr);
}
