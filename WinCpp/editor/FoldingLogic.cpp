#include "FoldingLogic.h"

namespace FoldingLogic
{
std::vector<int> FoldHeaderLinesFromText(const std::string& text)
{
  std::vector<int> lines;
  int lineIndex = 0;
  std::string current;

  auto flushLine = [&]() {
    while (!current.empty() && (current.back() == '\r' || current.back() == '\n' ||
                                current.back() == ' ' || current.back() == '\t'))
    {
      current.pop_back();
    }
    if (!current.empty() && current.back() == '{')
    {
      lines.push_back(lineIndex);
    }
    current.clear();
    ++lineIndex;
  };

  for (char ch : text)
  {
    if (ch == '\n')
    {
      flushLine();
    }
    else
    {
      current.push_back(ch);
    }
  }

  if (!current.empty())
  {
    flushLine();
  }

  return lines;
}
}
