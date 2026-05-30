#pragma once

#include <string>
#include <vector>

#include "WinCpp.h"

// Custom tab bar notifications (wParam = tab index).
constexpr UINT WM_TABBAR_SELECT = WM_USER + 200;
constexpr UINT WM_TABBAR_CLOSE = WM_USER + 201;
constexpr UINT WM_TABBAR_CONTEXTMENU = WM_USER + 202;

class TabBar
{
public:
  static bool RegisterClass(HINSTANCE instance);

  bool Create(HWND parent, HINSTANCE instance, int controlId);
  HWND Hwnd() const { return hwnd_; }

  void SetTabs(const std::vector<std::wstring>& titles, int selectedIndex);
  void SetSelectedIndex(int index);
  void RevealTab(int index);
  void UpdateTitle(int index, const std::wstring& title);
  int SelectedIndex() const { return selectedIndex_; }

  int GetPreferredHeight() const;

private:
  void EnsureTabVisible(int index);
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void OnPaint(HDC hdc);
  void ComputeTabWidths(int clientWidth);
  void ClampScrollOffset();
  void UpdateLayout();
  int HitTestTab(int x, int y) const;
  int HitTestClose(int x, int y) const;
  void NotifyParent(UINT message, int tabIndex) const;
  int GetCloseButtonWidth() const;
  int GetTabPadding() const;
  int GetMinTabWidth() const;
  int GetMaxTabWidth() const;
  int GetFitTabWidth() const;

  HWND hwnd_ = nullptr;
  HWND parent_ = nullptr;
  std::vector<std::wstring> titles_;
  int selectedIndex_ = -1;
  std::vector<int> tabWidths_;
  std::vector<RECT> tabRects_;
  std::vector<RECT> closeRects_;
  int scrollOffset_ = 0;
  int hoverCloseIndex_ = -1;
  int contentWidth_ = 0;
};
