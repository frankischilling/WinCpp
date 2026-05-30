#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct ThemeColor
{
  uint32_t foreground = 0;
  uint32_t background = 0;
  bool hasForeground = false;
  bool hasBackground = false;
};

struct EditorTheme
{
  std::string name;
  std::unordered_map<std::string, ThemeColor> links;
};

namespace ThemeLoader
{
std::vector<std::wstring> ListSchemeFiles(const std::wstring& directory);
bool LoadFromFile(const std::wstring& path, EditorTheme* theme, std::string* error = nullptr);
uint32_t ParseHexColor(const std::string& token, bool* ok = nullptr);
}
