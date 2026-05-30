#pragma once

#include <string>

struct StatusBarModelInput
{
  int line = 1;
  int column = 1;
  int selectionLength = 0;
  std::wstring path;
  std::wstring languageName;
  bool modified = false;
  bool useCrlf = false;
  int tabSize = 4;
  bool insertSpaces = false;
};

namespace StatusBarModel
{
std::wstring Format(const StatusBarModelInput& input);
}
