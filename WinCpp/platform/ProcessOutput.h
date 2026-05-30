#pragma once

#include <string>

namespace ProcessOutput
{
bool RunCaptureStdout(const std::wstring& commandLine, std::string* output, int* exitCode);
}
