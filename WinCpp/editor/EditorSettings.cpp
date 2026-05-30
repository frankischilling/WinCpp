#include "EditorSettings.h"

#include "ConfigJson.h"

#include <filesystem>
#include <fstream>
#include <shlobj.h>

EditorSettings EditorSettings::Defaults()
{
  return EditorSettings{};
}

void EditorSettings::SetConfigPathOverride(const std::wstring& path)
{
  configPathOverride_ = path;
}

void EditorSettings::ClearConfigPathOverride()
{
  configPathOverride_.clear();
}

std::wstring EditorSettings::ConfigPath() const
{
  if (!configPathOverride_.empty())
  {
    return configPathOverride_.wstring();
  }

  wchar_t appData[MAX_PATH] = {};
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData)))
  {
    return L"config.json";
  }

  std::filesystem::path path(appData);
  path /= L"WinCpp";
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  path /= L"config.json";
  return path.wstring();
}

void EditorSettings::Load()
{
  const std::filesystem::path configPath =
    configPathOverride_.empty() ? std::filesystem::path(ConfigPath()) : configPathOverride_;

  *this = Defaults();
  configPathOverride_ = configPath;

  std::ifstream file(configPath, std::ios::binary);
  if (!file)
  {
    return;
  }

  const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  ConfigJson::ParseEditorSettings(json, *this);

  if (tabSize < 1)
  {
    tabSize = 4;
  }
  if (fontSize < 8)
  {
    fontSize = 11;
  }
}

void EditorSettings::Save() const
{
  const std::filesystem::path path =
    configPathOverride_.empty() ? std::filesystem::path(ConfigPath()) : configPathOverride_;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::vector<std::wstring> recentFiles;
  std::ifstream in(path, std::ios::binary);
  if (in)
  {
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ConfigJson::ParseRecentFilesArray(json, recentFiles);
  }

  ConfigJson::WriteConfigFile(path.wstring(), recentFiles, *this);
}
