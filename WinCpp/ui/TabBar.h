#pragma once

#include <string>
#include <vector>

#include "WinCpp.h"

// Custom tab bar notifications (wParam = tab index).
constexpr UINT WM_TABBAR_SELECT = WM_USER + 200;
constexpr UINT WM_TABBAR_CLOSE = WM_USER + 201;
constexpr UINT WM_TABBAR_CONTEXTMENU = WM_USER + 202;
// wParam = source index, lParam = insert-before index (0..count).
constexpr UINT WM_TABBAR_REORDER = WM_USER + 203;
// wParam = MAKELPARAM(tabIndex, groupId), lParam = MAKELPARAM(screenX, screenY)
constexpr UINT WM_TABBAR_DRAG_MOVE = WM_USER + 204;
constexpr UINT WM_TABBAR_DRAG_END = WM_USER + 205;
// Stops split/merge preview and drag timer (always sent when a drag session ends).
constexpr UINT WM_TABBAR_DRAG_CANCEL = WM_USER + 206;

inline LPARAM PackScreenPoint(POINT pt)
{
  const UINT x = static_cast<UINT>(pt.x);
  const UINT y = static_cast<UINT>(pt.y);
  return static_cast<LPARAM>((static_cast<ULONG_PTR>(y) << 32) | x);
}

inline POINT UnpackScreenPoint(LPARAM packed)
{
  POINT pt = {};
  pt.x = static_cast<LONG>(static_cast<INT>(static_cast<UINT>(packed & 0xFFFFFFFFu)));
  pt.y = static_cast<LONG>(
      static_cast<INT>(static_cast<UINT>((static_cast<ULONG_PTR>(packed) >> 32) & 0xFFFFFFFFu)));
  return pt;
}

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

  void SetNotifyTarget(HWND notifyHwnd, int groupId);
  int GroupId() const { return groupId_; }

  struct ChromeColors
  {
    COLORREF stripBackground;
    COLORREF inactiveForeground;
    COLORREF separator;
    COLORREF sidebarHeaderBackground;
    COLORREF sidebarHeaderForeground;
    COLORREF sidebarBorder;
    COLORREF bottomBorder;
  };
  static ChromeColors GetChromeColors(HWND hwnd);

private:
  void EnsureTabVisible(int index);
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void OnPaint(HDC hdc);
  void ComputeTabWidths(int clientWidth);
  void ClampScrollOffset();
  void UpdateLayout();
  int HitTestTab(int x, int y) const;
  int HitTestClose(int x, int y) const;
  int HitTestInsertIndex(int x) const;
  int GetInsertMarkerX(int insertBefore) const;
  void NotifyParent(UINT message, int tabIndex) const;
  void NotifyParentReorder(int fromIndex, int insertBefore) const;
  void NotifyDragSessionEnd() const;
  void CancelDrag();
  void EndDrag(bool commit);
  void ShowDragGhost();
  void UpdateDragGhost(POINT screenPoint);
  void HideDragGhost();
  int GetCloseButtonWidth() const;
  int GetTabPadding() const;
  int GetMinTabWidth() const;
  int GetMaxTabWidth() const;
  int GetFitTabWidth() const;

  HWND hwnd_ = nullptr;
  HWND parent_ = nullptr;
  HWND notifyHwnd_ = nullptr;
  int groupId_ = 0;
  std::vector<std::wstring> titles_;
  int selectedIndex_ = -1;
  std::vector<int> tabWidths_;
  std::vector<RECT> tabRects_;
  std::vector<RECT> closeRects_;
  int scrollOffset_ = 0;
  int hoverTabIndex_ = -1;
  int hoverCloseIndex_ = -1;
  int contentWidth_ = 0;
  int dragSourceIndex_ = -1;
  int dragInsertBefore_ = -1;
  int dragStartX_ = 0;
  int dragStartY_ = 0;
  bool isDragging_ = false;
  HWND dragGhost_ = nullptr;
};
