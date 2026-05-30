#pragma once

#include "CommandRegistry.h"
#include "WinCpp.h"

#include <functional>
#include <vector>

class CommandPalette
{
public:
  using ExecuteFn = std::function<void(unsigned int commandId)>;

  void Show(HWND owner, const std::vector<CommandEntry>& commands, ExecuteFn execute);
  void Hide();
  bool IsOpen() const { return hwnd_ != nullptr; }

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
  void LayoutControls();
  void FilterAndRefresh();
  void ExecuteSelected();

  HWND hwnd_ = nullptr;
  HWND filterEdit_ = nullptr;
  HWND listBox_ = nullptr;
  std::vector<CommandEntry> allCommands_;
  std::vector<CommandEntry> filtered_;
  ExecuteFn execute_;
};
