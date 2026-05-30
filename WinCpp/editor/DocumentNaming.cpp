#include "DocumentNaming.h"

#include <filesystem>

namespace DocumentNaming
{
std::wstring TabLabelForPath(const std::wstring& path)
{
  if (path.empty())
  {
    return L"Untitled";
  }

  return std::filesystem::path(path).filename().wstring();
}

std::wstring DisplayTitle(const std::wstring& path, const std::wstring& tabTitle, bool modified)
{
  const std::wstring base = path.empty() ? tabTitle : TabLabelForPath(path);
  if (!modified)
  {
    return base;
  }

  return base + L" *";
}
}
