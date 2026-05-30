#include <gtest/gtest.h>

#include "ThemeLoader.h"

#include <algorithm>
#include <filesystem>

TEST(ThemeLoaderTest, ParsesHexColor)
{
  bool ok = false;
  EXPECT_EQ(ThemeLoader::ParseHexColor("#F8F8F2", &ok), 0xF8F8F2u);
  EXPECT_TRUE(ok);
}

TEST(ThemeLoaderTest, ListsMicroSchemeFiles)
{
  const std::filesystem::path dir =
    std::filesystem::path(WINCPP_TEST_FIXTURES_DIR) / L"colorschemes";
  const auto names = ThemeLoader::ListSchemeFiles(dir.wstring());
  ASSERT_FALSE(names.empty());
  EXPECT_NE(std::find(names.begin(), names.end(), L"sample.micro"), names.end());
}

TEST(ThemeLoaderTest, LoadsColorLinksFromFixture)
{
  const std::filesystem::path path =
    std::filesystem::path(WINCPP_TEST_FIXTURES_DIR) / L"colorschemes" / L"sample.micro";
  EditorTheme theme;
  ASSERT_TRUE(ThemeLoader::LoadFromFile(path.wstring(), &theme, nullptr));
  ASSERT_TRUE(theme.links.count("default"));
  EXPECT_TRUE(theme.links["default"].hasForeground);
  EXPECT_EQ(theme.links["default"].foreground, 0xF8F8F2u);
}
