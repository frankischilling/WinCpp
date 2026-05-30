#pragma once

#include <optional>
#include <string>

struct PathLineTarget
{
  std::wstring path;
  int line = 1;
};

namespace PathLineParser
{
std::optional<PathLineTarget> Parse(const std::wstring& text);
}
