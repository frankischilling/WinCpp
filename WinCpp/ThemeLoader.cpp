#include "ThemeLoader.h"

#include "ConfigJson.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
std::string Trim(const std::string& text)
{
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
  {
    ++start;
  }

  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
  {
    --end;
  }

  return text.substr(start, end - start);
}

bool ParseColorPair(const std::string& value, ThemeColor* color)
{
  if (!color)
  {
    return false;
  }

  const size_t comma = value.find(',');
  if (comma == std::string::npos)
  {
    bool ok = false;
    color->foreground = ThemeLoader::ParseHexColor(value, &ok);
    color->hasForeground = ok;
    return ok;
  }

  bool fgOk = false;
  bool bgOk = false;
  color->foreground = ThemeLoader::ParseHexColor(Trim(value.substr(0, comma)), &fgOk);
  color->background = ThemeLoader::ParseHexColor(Trim(value.substr(comma + 1)), &bgOk);
  color->hasForeground = fgOk;
  color->hasBackground = bgOk;
  return fgOk || bgOk;
}
}

namespace ThemeLoader
{
uint32_t ParseHexColor(const std::string& token, bool* ok)
{
  std::string hex = Trim(token);
  if (hex.empty())
  {
    if (ok)
    {
      *ok = false;
    }
    return 0;
  }

  if (hex[0] == '#')
  {
    hex.erase(hex.begin());
  }

  if (hex.size() != 6)
  {
    if (ok)
    {
      *ok = false;
    }
    return 0;
  }

  try
  {
    const uint32_t value = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
    if (ok)
    {
      *ok = true;
    }
    return value;
  }
  catch (...)
  {
    if (ok)
    {
      *ok = false;
    }
    return 0;
  }
}

std::vector<std::wstring> ListSchemeFiles(const std::wstring& directory)
{
  std::vector<std::wstring> names;
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec))
  {
    return names;
  }

  for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }

    const std::wstring ext = entry.path().extension().wstring();
    if (ext == L".micro" || ext == L".Micro")
    {
      names.push_back(entry.path().filename().wstring());
    }
  }

  std::sort(names.begin(), names.end());
  return names;
}

bool LoadFromFile(const std::wstring& path, EditorTheme* theme, std::string* error)
{
  if (!theme)
  {
    if (error)
    {
      *error = "theme output is null";
    }
    return false;
  }

  theme->links.clear();
  theme->name = std::filesystem::path(path).stem().string();

  std::ifstream file(std::filesystem::path(path), std::ios::binary);
  if (!file)
  {
    if (error)
    {
      *error = "unable to open theme file";
    }
    return false;
  }

  std::string line;
  while (std::getline(file, line))
  {
    line = Trim(line);
    if (line.empty() || line[0] == '#')
    {
      continue;
    }

    if (line.rfind("include ", 0) == 0)
    {
      continue;
    }

    const std::string prefix = "color-link ";
    if (line.rfind(prefix, 0) != 0)
    {
      continue;
    }

    size_t linkStart = prefix.size();
    while (linkStart < line.size() && std::isspace(static_cast<unsigned char>(line[linkStart])))
    {
      ++linkStart;
    }

    const size_t valueQuote = line.find('"', linkStart);
    if (valueQuote == std::string::npos)
    {
      continue;
    }

    const std::string link = Trim(line.substr(linkStart, valueQuote - linkStart));
    const size_t closingQuote = line.find('"', valueQuote + 1);
    if (closingQuote == std::string::npos)
    {
      continue;
    }

    const std::string value = line.substr(valueQuote + 1, closingQuote - valueQuote - 1);
    ThemeColor color{};
    if (ParseColorPair(value, &color))
    {
      theme->links[link] = color;
    }
  }

  return !theme->links.empty();
}
}
