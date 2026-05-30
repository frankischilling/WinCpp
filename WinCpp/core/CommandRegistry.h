#pragma once

#include <string>
#include <vector>

struct CommandEntry
{
  unsigned int id = 0;
  std::wstring label;
  std::wstring keywords;
};

namespace CommandRegistry
{
std::vector<CommandEntry> DefaultCommands();
std::vector<CommandEntry> Filter(const std::vector<CommandEntry>& commands, const std::wstring& query);
}
