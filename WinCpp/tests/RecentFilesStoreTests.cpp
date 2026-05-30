#include <gtest/gtest.h>

#include "RecentFilesStore.h"

#include <filesystem>
#include <fstream>

namespace
{
std::filesystem::path FixturesDir()
{
  return std::filesystem::path(WINCPP_TEST_FIXTURES_DIR);
}
}

namespace
{
class RecentFilesStoreTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    tempDir_ = std::filesystem::temp_directory_path() / L"WinCppTests";
    std::filesystem::create_directories(tempDir_);
    configPath_ = tempDir_ / L"recent.json";
    std::filesystem::remove(configPath_);
    store_.SetConfigPathOverride(configPath_.wstring());
  }

  void TearDown() override
  {
    store_.ClearConfigPathOverride();
    std::error_code ec;
    std::filesystem::remove(configPath_, ec);
  }

  std::filesystem::path tempDir_;
  std::filesystem::path configPath_;
  RecentFilesStore store_;
};
}

TEST_F(RecentFilesStoreTest, AddMovesToFrontAndCapsAtMax)
{
  for (int i = 0; i < 12; ++i)
  {
    store_.Add(L"path" + std::to_wstring(i) + L".txt");
  }

  ASSERT_EQ(store_.Items().size(), RecentFilesStore::kMaxItems);
  EXPECT_EQ(store_.Items().front(), L"path11.txt");
  EXPECT_EQ(store_.Items().back(), L"path2.txt");
}

TEST_F(RecentFilesStoreTest, AddDedupesExistingPath)
{
  store_.Add(L"a.txt");
  store_.Add(L"b.txt");
  store_.Add(L"a.txt");

  ASSERT_EQ(store_.Items().size(), 2u);
  EXPECT_EQ(store_.Items()[0], L"a.txt");
  EXPECT_EQ(store_.Items()[1], L"b.txt");
}

TEST_F(RecentFilesStoreTest, SaveAndLoadRoundTrip)
{
  store_.Add(L"C:\\docs\\one.cpp");
  store_.Add(L"C:\\docs\\two.h");

  RecentFilesStore loaded;
  loaded.SetConfigPathOverride(configPath_.wstring());
  loaded.Load();

  ASSERT_EQ(loaded.Items().size(), 2u);
  EXPECT_EQ(loaded.Items()[0], L"C:\\docs\\two.h");
  EXPECT_EQ(loaded.Items()[1], L"C:\\docs\\one.cpp");
}

TEST_F(RecentFilesStoreTest, AddIgnoresEmptyPath)
{
  store_.Add(L"");
  store_.Add(L"valid.txt");
  ASSERT_EQ(store_.Items().size(), 1u);
  EXPECT_EQ(store_.Items()[0], L"valid.txt");
}

TEST_F(RecentFilesStoreTest, ClearRemovesAll)
{
  store_.Add(L"x.txt");
  store_.Clear();
  EXPECT_TRUE(store_.Items().empty());

  RecentFilesStore loaded;
  loaded.SetConfigPathOverride(configPath_.wstring());
  loaded.Load();
  EXPECT_TRUE(loaded.Items().empty());
}

TEST_F(RecentFilesStoreTest, LoadMissingFileLeavesEmpty)
{
  RecentFilesStore fresh;
  fresh.SetConfigPathOverride((tempDir_ / L"missing_config.json").wstring());
  fresh.Load();
  EXPECT_TRUE(fresh.Items().empty());
}

TEST_F(RecentFilesStoreTest, LoadCorruptJsonLeavesEmpty)
{
  const auto corruptPath = tempDir_ / L"corrupt.json";
  {
    std::ifstream in(FixturesDir() / "corrupt_recent.json");
    std::ofstream out(corruptPath, std::ios::binary);
    out << in.rdbuf();
  }

  RecentFilesStore loaded;
  loaded.SetConfigPathOverride(corruptPath.wstring());
  loaded.Load();
  EXPECT_TRUE(loaded.Items().empty());
}

TEST_F(RecentFilesStoreTest, SaveLoadOrderAfterClearAndReadd)
{
  store_.Add(L"first.txt");
  store_.Add(L"second.txt");
  store_.Clear();
  store_.Add(L"older.txt");
  store_.Add(L"newest.txt");

  RecentFilesStore loaded;
  loaded.SetConfigPathOverride(configPath_.wstring());
  loaded.Load();
  ASSERT_EQ(loaded.Items().size(), 2u);
  EXPECT_EQ(loaded.Items()[0], L"newest.txt");
  EXPECT_EQ(loaded.Items()[1], L"older.txt");
}
