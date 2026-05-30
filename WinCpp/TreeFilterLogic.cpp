#include "TreeFilterLogic.h"

#include <cwctype>

namespace
{
std::wstring ToLower(std::wstring text)
{
  for (wchar_t& ch : text)
  {
    ch = static_cast<wchar_t>(towlower(ch));
  }
  return text;
}
}

namespace TreeFilterLogic
{
bool PathMatchesFilter(const std::wstring& relativePath, const std::wstring& filter)
{
  if (filter.empty())
  {
    return true;
  }

  return ToLower(relativePath).find(ToLower(filter)) != std::wstring::npos;
}

std::vector<std::wstring> FilterPaths(const std::vector<std::wstring>& paths,
                                      const std::wstring& filter)
{
  std::vector<std::wstring> result;
  for (const std::wstring& path : paths)
  {
    if (PathMatchesFilter(path, filter))
    {
      result.push_back(path);
    }
  }
  return result;
}
}
