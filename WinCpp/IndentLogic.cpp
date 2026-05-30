#include "IndentLogic.h"

namespace
{
bool IsWhitespace(unsigned char ch)
{
  return ch == ' ' || ch == '\t';
}

std::string ExtraIndentForBrace(const std::string& trimmed, int tabSize, bool useSpaces)
{
  if (trimmed.empty())
  {
    return {};
  }

  const char last = trimmed.back();
  if (last != '{' && last != '(' && last != '[')
  {
    return {};
  }

  if (useSpaces)
  {
    return std::string(static_cast<size_t>(tabSize), ' ');
  }

  return "\t";
}
}

namespace IndentLogic
{
std::string ComputeNewLineIndent(const std::string& previousLine, int tabSize, bool useSpaces)
{
  std::string indent;
  for (unsigned char ch : previousLine)
  {
    if (ch == ' ' || ch == '\t')
    {
      indent.push_back(static_cast<char>(ch));
    }
    else
    {
      break;
    }
  }

  std::string trimmed = previousLine;
  while (!trimmed.empty() && IsWhitespace(static_cast<unsigned char>(trimmed.front())))
  {
    trimmed.erase(trimmed.begin());
  }
  while (!trimmed.empty() && IsWhitespace(static_cast<unsigned char>(trimmed.back())))
  {
    trimmed.pop_back();
  }

  indent += ExtraIndentForBrace(trimmed, tabSize, useSpaces);
  return indent;
}
}
