#include "StatusBarModel.h"

namespace StatusBarModel
{
std::wstring Format(const StatusBarModelInput& input)
{
  std::wstring text = L"Ln " + std::to_wstring(input.line) + L", Col " + std::to_wstring(input.column);
  if (input.selectionLength > 0)
  {
    text += L" (" + std::to_wstring(input.selectionLength) + L" selected)";
  }

  if (!input.languageName.empty())
  {
    text += L"  |  " + input.languageName;
  }

  text += input.useCrlf ? L"  |  CRLF" : L"  |  LF";
  text += input.insertSpaces ? L"  |  Spaces: " + std::to_wstring(input.tabSize)
                            : L"  |  Tab Size: " + std::to_wstring(input.tabSize);

  if (input.modified)
  {
    text += L"  |  Modified";
  }

  if (!input.path.empty())
  {
    text += L"  |  " + input.path;
  }

  return text;
}
}
