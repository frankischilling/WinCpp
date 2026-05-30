#include "RecentFilesStore.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <shlobj.h>

namespace
{
std::wstring Utf8ToWide(const std::string& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0);
  if (length <= 0)
  {
    return {};
  }

  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      result.data(), length);
  return result;
}

std::string WideToUtf8(const std::wstring& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (length <= 0)
  {
    return {};
  }

  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      result.data(), length, nullptr, nullptr);
  return result;
}

std::string JsonEscape(const std::wstring& text)
{
  std::string out;
  out.push_back('"');
  const std::string utf8 = WideToUtf8(text);
  for (unsigned char ch : utf8)
  {
    switch (ch)
    {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (ch < 0x20)
        {
          char hex[8] = {};
          sprintf_s(hex, "\\u%04x", ch);
          out += hex;
        }
        else
        {
          out.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  out.push_back('"');
  return out;
}

bool ParseJsonString(const std::string& json, size_t& pos, std::wstring& out)
{
  if (pos >= json.size() || json[pos] != '"')
  {
    return false;
  }

  ++pos;
  std::string value;
  while (pos < json.size())
  {
    const char ch = json[pos++];
    if (ch == '"')
    {
      out = Utf8ToWide(value);
      return true;
    }

    if (ch == '\\' && pos < json.size())
    {
      const char esc = json[pos++];
      switch (esc)
      {
        case '"':
          value.push_back('"');
          break;
        case '\\':
          value.push_back('\\');
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        case 'u':
          if (pos + 3 < json.size())
          {
            const std::string hex = json.substr(pos, 4);
            pos += 4;
            wchar_t wchar = static_cast<wchar_t>(std::stoi(hex, nullptr, 16));
            value.append(WideToUtf8(std::wstring(1, wchar)));
          }
          break;
        default:
          value.push_back(esc);
          break;
      }
      continue;
    }

    value.push_back(ch);
  }

  return false;
}

void ParseRecentFilesArray(const std::string& json, std::vector<std::wstring>& items)
{
  const std::string key = "\"recentFiles\"";
  const size_t keyPos = json.find(key);
  if (keyPos == std::string::npos)
  {
    return;
  }

  size_t pos = json.find('[', keyPos);
  if (pos == std::string::npos)
  {
    return;
  }

  ++pos;
  while (pos < json.size())
  {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                 json[pos] == '\r' || json[pos] == '\n' ||
                                 json[pos] == ','))
    {
      ++pos;
    }

    if (pos >= json.size() || json[pos] == ']')
    {
      break;
    }

    std::wstring path;
    if (!ParseJsonString(json, pos, path))
    {
      break;
    }

    if (!path.empty())
    {
      items.push_back(path);
    }
  }
}
}

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

void RecentFilesStore::Load()
{
  items_.clear();

  const std::wstring configPath = ConfigPath();
  std::ifstream file(configPath, std::ios::binary);
  if (!file)
  {
    return;
  }

  const std::string json((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  ParseRecentFilesArray(json, items_);

  if (items_.size() > kMaxItems)
  {
    items_.resize(kMaxItems);
  }
}

void RecentFilesStore::Save() const
{
  const std::wstring configPath = ConfigPath();
  std::ofstream file(configPath, std::ios::binary | std::ios::trunc);
  if (!file)
  {
    return;
  }

  file << "{\n  \"recentFiles\": [\n";
  for (size_t i = 0; i < items_.size(); ++i)
  {
    file << "    " << JsonEscape(items_[i]);
    if (i + 1 < items_.size())
    {
      file << ",";
    }
    file << "\n";
  }
  file << "  ]\n}\n";
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
