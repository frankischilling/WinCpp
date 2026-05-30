#pragma once

#include <string>

namespace DocumentNaming
{
std::wstring TabLabelForPath(const std::wstring& path);
std::wstring DisplayTitle(const std::wstring& path, const std::wstring& tabTitle, bool modified);
}
