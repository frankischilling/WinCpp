#include "ConfigJson.h"

#include "WinCpp.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace
{
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
      out = ConfigJson::Utf8ToWide(value);
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
            const wchar_t wchar = static_cast<wchar_t>(std::stoi(hex, nullptr, 16));
            value.append(ConfigJson::WideToUtf8(std::wstring(1, wchar)));
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

bool ParseBoolValue(const std::string& json, const std::string& key, bool& out)
{
  const size_t keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
  {
    return false;
  }

  const size_t colon = json.find(':', keyPos);
  if (colon == std::string::npos)
  {
    return false;
  }

  size_t pos = colon + 1;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
  {
    ++pos;
  }

  if (json.compare(pos, 4, "true") == 0)
  {
    out = true;
    return true;
  }

  if (json.compare(pos, 5, "false") == 0)
  {
    out = false;
    return true;
  }

  return false;
}

bool ParseIntValue(const std::string& json, const std::string& key, int& out)
{
  const size_t keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
  {
    return false;
  }

  const size_t colon = json.find(':', keyPos);
  if (colon == std::string::npos)
  {
    return false;
  }

  size_t pos = colon + 1;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
  {
    ++pos;
  }

  const size_t end = json.find_first_of(",}\r\n", pos);
  const std::string num = json.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
  try
  {
    out = std::stoi(num);
    return true;
  }
  catch (...)
  {
    return false;
  }
}
}

namespace ConfigJson
{
std::wstring Utf8ToWide(const std::string& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0);
  if (length <= 0)
  {
    return {};
  }

  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
  return result;
}

std::string WideToUtf8(const std::wstring& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (length <= 0)
  {
    return {};
  }

  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length,
                      nullptr, nullptr);
  return result;
}

std::string EscapeString(const std::wstring& text)
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
    while (pos < json.size() &&
           (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n' ||
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

bool ParseEditorSettings(const std::string& json, EditorSettings& settings)
{
  const size_t editorPos = json.find("\"editor\":");
  if (editorPos == std::string::npos)
  {
    return false;
  }

  const size_t brace = json.find('{', editorPos);
  if (brace == std::string::npos)
  {
    return false;
  }

  size_t depth = 0;
  size_t end = brace;
  for (; end < json.size(); ++end)
  {
    if (json[end] == '{')
    {
      ++depth;
    }
    else if (json[end] == '}')
    {
      --depth;
      if (depth == 0)
      {
        break;
      }
    }
  }

  if (depth != 0)
  {
    return false;
  }

  const std::string block = json.substr(brace, end - brace + 1);
  ParseIntValue(block, "tabsize", settings.tabSize);
  ParseBoolValue(block, "tabstospaces", settings.tabToSpaces);
  ParseBoolValue(block, "autoindent", settings.autoIndent);
  ParseBoolValue(block, "matchbrace", settings.matchBrace);
  ParseBoolValue(block, "wordwrap", settings.wordWrap);
  ParseIntValue(block, "fontsize", settings.fontSize);
  ParseIntValue(block, "zoom", settings.zoom);
  ParseBoolValue(block, "trimTrailingWhitespaceOnSave", settings.trimTrailingWhitespaceOnSave);
  return true;
}

void WriteConfigFile(const std::wstring& path, const std::vector<std::wstring>& recentFiles,
                     const EditorSettings& editor)
{
  std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
  if (!file)
  {
    return;
  }

  file << "{\n  \"recentFiles\": [\n";
  for (size_t i = 0; i < recentFiles.size(); ++i)
  {
    file << "    " << EscapeString(recentFiles[i]);
    if (i + 1 < recentFiles.size())
    {
      file << ",";
    }
    file << "\n";
  }
  file << "  ],\n";
  file << "  \"editor\": {\n";
  file << "    \"tabsize\": " << editor.tabSize << ",\n";
  file << "    \"tabstospaces\": " << (editor.tabToSpaces ? "true" : "false") << ",\n";
  file << "    \"autoindent\": " << (editor.autoIndent ? "true" : "false") << ",\n";
  file << "    \"matchbrace\": " << (editor.matchBrace ? "true" : "false") << ",\n";
  file << "    \"wordwrap\": " << (editor.wordWrap ? "true" : "false") << ",\n";
  file << "    \"fontsize\": " << editor.fontSize << ",\n";
  file << "    \"zoom\": " << editor.zoom << ",\n";
  file << "    \"trimTrailingWhitespaceOnSave\": "
       << (editor.trimTrailingWhitespaceOnSave ? "true" : "false") << "\n";
  file << "  }\n}\n";
}
}
