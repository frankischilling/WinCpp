#include "StatusBarModel.h"

namespace StatusBarModel
{
StatusBarParts FormatParts(const StatusBarModelInput& input)
{
  StatusBarParts parts;
  parts.left = L"Ln " + std::to_wstring(input.line) + L", Col " + std::to_wstring(input.column);
  if (input.selectionLength > 0)
  {
    parts.left += L" (" + std::to_wstring(input.selectionLength) + L" selected)";
  }
  if (input.modified)
  {
    parts.left += L"  |  Modified";
  }
  if (!input.path.empty())
  {
    parts.left += L"  |  " + input.path;
  }

  if (!input.languageName.empty())
  {
    parts.right = input.languageName;
  }
  else
  {
    parts.right = L"Plain Text";
  }

  parts.right += input.useCrlf ? L"  |  CRLF" : L"  |  LF";
  parts.right += input.insertSpaces ? L"  |  Spaces: " + std::to_wstring(input.tabSize)
                                  : L"  |  Tab Size: " + std::to_wstring(input.tabSize);

  return parts;
}

std::wstring Format(const StatusBarModelInput& input)
{
  const StatusBarParts parts = FormatParts(input);
  if (parts.right.empty())
  {
    return parts.left;
  }
  return parts.left + L"  |  " + parts.right;
}
}
