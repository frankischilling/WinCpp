#include "PathLineParser.h"

namespace PathLineParser
{
std::optional<PathLineTarget> Parse(const std::wstring& text)
{
  if (text.empty())
  {
    return std::nullopt;
  }

  const size_t colon = text.rfind(L':');
  if (colon == std::wstring::npos || colon + 1 >= text.size())
  {
    return std::nullopt;
  }

  const std::wstring linePart = text.substr(colon + 1);
  bool allDigits = !linePart.empty();
  for (wchar_t ch : linePart)
  {
    if (ch < L'0' || ch > L'9')
    {
      allDigits = false;
      break;
    }
  }

  if (!allDigits)
  {
    return std::nullopt;
  }

  PathLineTarget target;
  target.path = text.substr(0, colon);
  target.line = std::stoi(linePart);
  if (target.line < 1)
  {
    target.line = 1;
  }
  return target;
}
}
