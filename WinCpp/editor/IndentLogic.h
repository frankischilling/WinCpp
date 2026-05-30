#pragma once

#include <string>

namespace IndentLogic
{
std::string ComputeNewLineIndent(const std::string& previousLine, int tabSize, bool useSpaces);
}
