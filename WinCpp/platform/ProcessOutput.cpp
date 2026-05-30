#include "ProcessOutput.h"

#include "WinCpp.h"

#include <array>
#include <vector>

namespace ProcessOutput
{
bool RunCaptureStdout(const std::wstring& commandLine, std::string* output, int* exitCode)
{
  if (output)
  {
    output->clear();
  }

  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE readPipe = nullptr;
  HANDLE writePipe = nullptr;
  if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
  {
    return false;
  }

  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = writePipe;
  si.hStdError = writePipe;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION pi = {};
  std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
  cmd.push_back(L'\0');

  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                      nullptr, &si, &pi))
  {
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    return false;
  }

  CloseHandle(writePipe);

  std::array<char, 4096> buffer{};
  DWORD bytesRead = 0;
  while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) &&
         bytesRead > 0)
  {
    if (output)
    {
      output->append(buffer.data(), bytesRead);
    }
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  if (exitCode)
  {
    *exitCode = static_cast<int>(code);
  }

  CloseHandle(readPipe);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}
}
