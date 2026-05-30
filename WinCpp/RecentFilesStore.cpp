#include "RecentFilesStore.h"

#include "ConfigJson.h"
#include "EditorSettings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <shlobj.h>

void RecentFilesStore::SetConfigPathOverride(const std::wstring& path)
{
  configPathOverride_ = path;
}

void RecentFilesStore::ClearConfigPathOverride()
{
  configPathOverride_.clear();
}

std::wstring RecentFilesStore::ConfigPath() const
{
  if (!configPathOverride_.empty())
  {
    return configPathOverride_;
  }

  EditorSettings settings;
  return settings.ConfigPath();
}

void RecentFilesStore::Load()
{
  items_.clear();

  const std::wstring configPath = ConfigPath();
  std::ifstream file(std::filesystem::path(configPath), std::ios::binary);
  if (!file)
  {
    return;
  }

  const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  ConfigJson::ParseRecentFilesArray(json, items_);

  if (items_.size() > kMaxItems)
  {
    items_.resize(kMaxItems);
  }
}

void RecentFilesStore::Save() const
{
  const std::wstring configPath = ConfigPath();
  EditorSettings editor = EditorSettings::Defaults();
  std::ifstream in(std::filesystem::path(configPath), std::ios::binary);
  if (in)
  {
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ConfigJson::ParseEditorSettings(json, editor);
  }

  ConfigJson::WriteConfigFile(configPath, items_, editor);
}

void RecentFilesStore::Add(const std::wstring& path)
{
  if (path.empty())
  {
    return;
  }

  items_.erase(std::remove(items_.begin(), items_.end(), path), items_.end());
  items_.insert(items_.begin(), path);
  if (items_.size() > kMaxItems)
  {
    items_.resize(kMaxItems);
  }

  Save();
}

void RecentFilesStore::Clear()
{
  items_.clear();
  Save();
}
