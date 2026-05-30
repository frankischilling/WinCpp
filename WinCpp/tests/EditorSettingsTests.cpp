#include <gtest/gtest.h>

#include "EditorSettings.h"
#include "RecentFilesStore.h"

#include <filesystem>
#include <fstream>

namespace
{
class EditorSettingsTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    tempDir_ = std::filesystem::temp_directory_path() / L"WinCppEditorSettingsTests";
    std::filesystem::create_directories(tempDir_);
    configPath_ = tempDir_ / L"config.json";
    std::filesystem::remove(configPath_);
    settings_.SetConfigPathOverride(configPath_.wstring());
    recents_.SetConfigPathOverride(configPath_.wstring());
  }

  void TearDown() override
  {
    settings_.ClearConfigPathOverride();
    recents_.ClearConfigPathOverride();
    std::error_code ec;
    std::filesystem::remove(configPath_, ec);
  }

  std::filesystem::path tempDir_;
  std::filesystem::path configPath_;
  EditorSettings settings_;
  RecentFilesStore recents_;
};
}

TEST_F(EditorSettingsTest, DefaultsMatchMicroLikeValues)
{
  const EditorSettings defaults = EditorSettings::Defaults();
  EXPECT_EQ(defaults.tabSize, 4);
  EXPECT_FALSE(defaults.tabToSpaces);
  EXPECT_TRUE(defaults.autoIndent);
  EXPECT_TRUE(defaults.matchBrace);
  EXPECT_FALSE(defaults.wordWrap);
  EXPECT_EQ(defaults.fontSize, 11);
}

TEST_F(EditorSettingsTest, SaveAndLoadRoundTrip)
{
  settings_.tabSize = 2;
  settings_.tabToSpaces = true;
  settings_.wordWrap = true;
  settings_.zoom = 2;
  settings_.Save();

  EditorSettings loaded;
  loaded.SetConfigPathOverride(configPath_.wstring());
  loaded.Load();

  EXPECT_EQ(loaded.tabSize, 2);
  EXPECT_TRUE(loaded.tabToSpaces);
  EXPECT_TRUE(loaded.wordWrap);
  EXPECT_EQ(loaded.zoom, 2);
}

TEST_F(EditorSettingsTest, LoadMissingFileUsesDefaults)
{
  settings_.Load();
  EXPECT_EQ(settings_.tabSize, 4);
  EXPECT_TRUE(settings_.matchBrace);
}

TEST_F(EditorSettingsTest, SavePreservesRecentFiles)
{
  recents_.Add(L"C:\\a.txt");
  settings_.tabSize = 8;
  settings_.Save();

  RecentFilesStore loadedRecents;
  loadedRecents.SetConfigPathOverride(configPath_.wstring());
  loadedRecents.Load();
  ASSERT_EQ(loadedRecents.Items().size(), 1u);
  EXPECT_EQ(loadedRecents.Items()[0], L"C:\\a.txt");

  EditorSettings loadedSettings;
  loadedSettings.SetConfigPathOverride(configPath_.wstring());
  loadedSettings.Load();
  EXPECT_EQ(loadedSettings.tabSize, 8);
}

TEST_F(EditorSettingsTest, RecentSavePreservesEditorBlock)
{
  settings_.autoIndent = false;
  settings_.Save();
  recents_.Add(L"b.txt");

  EditorSettings loaded;
  loaded.SetConfigPathOverride(configPath_.wstring());
  loaded.Load();
  EXPECT_FALSE(loaded.autoIndent);
}

TEST_F(EditorSettingsTest, LoadIgnoresInvalidEditorKeys)
{
  {
    std::ofstream out(configPath_, std::ios::binary);
    out << "{\n  \"editor\": { \"tabsize\": -5, \"fontsize\": 3 }\n}\n";
  }

  settings_.Load();
  EXPECT_EQ(settings_.tabSize, 4);
  EXPECT_EQ(settings_.fontSize, 11);
}
