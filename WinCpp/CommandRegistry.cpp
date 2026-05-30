#include "CommandRegistry.h"

#include "AppIds.h"

#include <algorithm>
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

bool ContainsInsensitive(const std::wstring& haystack, const std::wstring& needle)
{
  if (needle.empty())
  {
    return true;
  }

  const std::wstring h = ToLower(haystack);
  const std::wstring n = ToLower(needle);
  return h.find(n) != std::wstring::npos;
}
}

namespace CommandRegistry
{
std::vector<CommandEntry> DefaultCommands()
{
  return {
      {kCmdFileNew, L"New File", L"new file"},
      {kCmdFileOpen, L"Open File", L"open file"},
      {kCmdFileSave, L"Save", L"save"},
      {kCmdFileSaveAs, L"Save As", L"save as"},
      {kCmdEditFind, L"Find", L"find search"},
      {kCmdEditReplace, L"Replace", L"replace"},
      {kCmdEditGoToLine, L"Go To Line", L"go line"},
      {kCmdViewWordWrap, L"Toggle Word Wrap", L"wrap word"},
      {kCmdViewSplitRight, L"Split Right", L"split editor"},
  };
}

std::vector<CommandEntry> Filter(const std::vector<CommandEntry>& commands, const std::wstring& query)
{
  if (query.empty())
  {
    return commands;
  }

  std::vector<CommandEntry> filtered;
  for (const CommandEntry& entry : commands)
  {
    if (ContainsInsensitive(entry.label, query) || ContainsInsensitive(entry.keywords, query))
    {
      filtered.push_back(entry);
    }
  }
  return filtered;
}
}
