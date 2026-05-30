#pragma once

#include <string>
#include <vector>

namespace TreeFilterLogic
{
bool PathMatchesFilter(const std::wstring& relativePath, const std::wstring& filter);
std::vector<std::wstring> FilterPaths(const std::vector<std::wstring>& paths,
                                      const std::wstring& filter);
}
