#pragma once

#include <string>
#include <vector>

namespace FoldingLogic
{
// Returns 0-based line indices that start a foldable block (lines ending with '{').
std::vector<int> FoldHeaderLinesFromText(const std::string& text);
}
