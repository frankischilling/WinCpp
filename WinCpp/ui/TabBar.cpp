#include "TabBar.h"

#include <cstdlib>

#include <windowsx.h>

#include "TabBarLogic.h"
#include "UiHelpers.h"
#include "UiMetrics.h"
#include "UiTheme.h"

namespace
{
constexpr wchar_t kTabBarClassName[] = L"WinCppTabBar";
constexpr wchar_t kDragGhostClassName[] = L"WinCppTabDragGhost";
constexpr int kTabWidthFit = 120;
constexpr int kDragGhostMaxWidth = 120;
constexpr int kDragGhostOffsetX = 10;
constexpr int kDragGhostOffsetY = 10;
constexpr int kTabWidthMin = 50;
constexpr int kTabWidthMax = 160;
constexpr int kDefaultHeight = 28;
constexpr int kBottomBorder = 1;


void DrawVerticalLine(HDC hdc, int x, int top, int bottom, COLORREF color)
{
  if (bottom <= top)
  {
    return;
  }

  HPEN pen = CreatePen(PS_SOLID, 1, color);
  HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, pen));
  MoveToEx(hdc, x, top, nullptr);
  LineTo(hdc, x, bottom);
  SelectObject(hdc, oldPen);
  DeleteObject(pen);
}

bool RegisterDragGhostClass(HINSTANCE instance)
{
  WNDCLASSEXW wc = {};
  if (GetClassInfoExW(instance, kDragGhostClassName, &wc))
  {
    return true;
  }

  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = instance;
  wc.lpszClassName = kDragGhostClassName;
  return RegisterClassExW(&wc) != 0;
}

SIZE MeasureDragGhostLabel(HDC hdc, HFONT font, const std::wstring& label, int maxWidth)
{
  RECT bounds = {0, 0, maxWidth, 0};
  HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, font));
  DrawTextW(hdc, label.c_str(), -1, &bounds, DT_LEFT | DT_SINGLELINE | DT_CALCRECT | DT_END_ELLIPSIS);
  SelectObject(hdc, oldFont);
  return {bounds.right - bounds.left, bounds.bottom - bounds.top};
}
}

TabBar::ChromeColors TabBar::GetChromeColors(HWND hwnd)
{
  const UiThemeChrome& chrome = UiTheme::Resolve(hwnd).chrome;
  return {chrome.stripBackground,
          chrome.inactiveForeground,
          chrome.separator,
          chrome.sidebarHeaderBackground,
          chrome.sidebarHeaderForeground,
          chrome.sidebarBorder,
          chrome.bottomBorder};
}

bool TabBar::RegisterClass(HINSTANCE instance)
{
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = TabBar::WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = kTabBarClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);

  return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool TabBar::Create(HWND parent, HINSTANCE instance, int controlId)
{
  if (!RegisterClass(instance))
  {
    return false;
  }

  parent_ = parent;
  hwnd_ = CreateWindowExW(
      0,
      kTabBarClassName,
      L"",
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
      0,
      0,
      0,
      0,
      parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
      instance,
      this);

  if (!hwnd_)
  {
    return false;
  }

  ApplyUiFont(hwnd_);
  return true;
}

void TabBar::SetNotifyTarget(HWND notifyHwnd, int groupId)
{
  notifyHwnd_ = notifyHwnd;
  groupId_ = groupId;
}

void TabBar::SetTabs(const std::vector<std::wstring>& titles, int selectedIndex)
{
  titles_ = titles;
  selectedIndex_ = selectedIndex;
  if (selectedIndex_ >= static_cast<int>(titles_.size()))
  {
    selectedIndex_ = titles_.empty() ? -1 : static_cast<int>(titles_.size()) - 1;
  }

  UpdateLayout();
  // Only clamp here — scroll-into-view runs in SetSelectedIndex when the user picks a tab.
  ClampScrollOffset();
  InvalidateRect(hwnd_, nullptr, TRUE);
}

void TabBar::SetSelectedIndex(int index)
{
  if (index < 0 || index >= static_cast<int>(titles_.size()) || index == selectedIndex_)
  {
    return;
  }

  selectedIndex_ = index;
  InvalidateRect(hwnd_, nullptr, TRUE);
}

void TabBar::RevealTab(int index)
{
  if (index < 0 || index >= static_cast<int>(titles_.size()))
  {
    return;
  }

  selectedIndex_ = index;
  EnsureTabVisible(index);
  InvalidateRect(hwnd_, nullptr, TRUE);
}

void TabBar::UpdateTitle(int index, const std::wstring& title)
{
  if (index < 0 || index >= static_cast<int>(titles_.size()) || titles_[index] == title)
  {
    return;
  }

  titles_[index] = title;
  const int savedScroll = scrollOffset_;
  UpdateLayout();
  scrollOffset_ = savedScroll;
  ClampScrollOffset();
  InvalidateRect(hwnd_, nullptr, TRUE);
}

int TabBar::GetPreferredHeight() const
{
  if (!hwnd_)
  {
    return MulDiv(kDefaultHeight, 96, 96);
  }

  const UINT dpi = GetDpiForWindow(hwnd_);
  HDC dc = GetDC(hwnd_);
  if (!dc)
  {
    return MulDiv(kDefaultHeight, static_cast<int>(dpi), 96);
  }

  HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd_, WM_GETFONT, 0, 0));
  if (!font)
  {
    font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  }
  HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
  TEXTMETRICW metrics = {};
  GetTextMetricsW(dc, &metrics);
  SelectObject(dc, oldFont);
  ReleaseDC(hwnd_, dc);

  return metrics.tmHeight + MulDiv(10, static_cast<int>(dpi), 96) + kBottomBorder;
}

void TabBar::EnsureTabVisible(int index)
{
  if (index < 0 || index >= static_cast<int>(tabWidths_.size()))
  {
    return;
  }

  int tabLeft = 0;
  for (int i = 0; i < index; ++i)
  {
    tabLeft += tabWidths_[i];
  }
  const int tabRight = tabLeft + tabWidths_[index];

  RECT client = {};
  GetClientRect(hwnd_, &client);
  const int viewWidth = client.right - client.left;

  if (tabLeft < scrollOffset_)
  {
    scrollOffset_ = tabLeft;
  }
  else if (tabRight > scrollOffset_ + viewWidth)
  {
    scrollOffset_ = tabRight - viewWidth;
  }

  ClampScrollOffset();
}

int TabBar::GetCloseButtonWidth() const
{
  return MulDiv(18, static_cast<int>(GetDpiForWindow(hwnd_)), 96);
}

int TabBar::GetTabPadding() const
{
  return MulDiv(10, static_cast<int>(GetDpiForWindow(hwnd_)), 96);
}

int TabBar::GetMinTabWidth() const
{
  return MulDiv(kTabWidthMin, static_cast<int>(GetDpiForWindow(hwnd_)), 96);
}

int TabBar::GetMaxTabWidth() const
{
  return MulDiv(kTabWidthMax, static_cast<int>(GetDpiForWindow(hwnd_)), 96);
}

int TabBar::GetFitTabWidth() const
{
  return MulDiv(kTabWidthFit, static_cast<int>(GetDpiForWindow(hwnd_)), 96);
}

void TabBar::ComputeTabWidths(int clientWidth)
{
  const int count = static_cast<int>(titles_.size());
  tabWidths_.assign(count, GetFitTabWidth());
  tabRects_.assign(count, RECT{});
  closeRects_.assign(count, RECT{});
  contentWidth_ = 0;

  if (count == 0 || clientWidth <= 0)
  {
    return;
  }

  const int minWidth = GetMinTabWidth();
  const int maxWidth = GetMaxTabWidth();
  const int chrome = GetCloseButtonWidth() + GetTabPadding() * 2;

  HDC dc = GetDC(hwnd_);
  if (!dc)
  {
    return;
  }

  HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd_, WM_GETFONT, 0, 0));
  if (!font)
  {
    font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  }
  HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));

  std::vector<int> naturalWidths(count, minWidth);
  int totalNatural = 0;
  for (int i = 0; i < count; ++i)
  {
    SIZE textSize = {};
    GetTextExtentPoint32W(dc, titles_[i].c_str(), static_cast<int>(titles_[i].size()), &textSize);
    naturalWidths[i] = textSize.cx + chrome;
    if (naturalWidths[i] < minWidth)
    {
      naturalWidths[i] = minWidth;
    }
    if (naturalWidths[i] > maxWidth)
    {
      naturalWidths[i] = maxWidth;
    }
    totalNatural += naturalWidths[i];
  }

  SelectObject(dc, oldFont);
  ReleaseDC(hwnd_, dc);

  if (totalNatural <= clientWidth)
  {
    tabWidths_ = naturalWidths;
    int extra = clientWidth - totalNatural;
    int tabIndex = 0;
    while (extra > 0 && count > 0)
    {
      if (tabWidths_[tabIndex] < maxWidth)
      {
        ++tabWidths_[tabIndex];
        --extra;
      }
      tabIndex = (tabIndex + 1) % count;
      if (tabIndex == 0)
      {
        bool canGrow = false;
        for (int w : tabWidths_)
        {
          if (w < maxWidth)
          {
            canGrow = true;
            break;
          }
        }
        if (!canGrow)
        {
          break;
        }
      }
    }
  }
  else
  {
    const int sumAtMin = count * minWidth;
    if (sumAtMin > clientWidth)
    {
      // Too many tabs to fit even at minimum width — keep min width and scroll.
      tabWidths_.assign(count, minWidth);
    }
    else
    {
      tabWidths_.assign(count, minWidth);
      int remaining = clientWidth - count * minWidth;
      if (remaining > 0)
      {
        int extraBudget = 0;
        for (int i = 0; i < count; ++i)
        {
          extraBudget += naturalWidths[i] - minWidth;
        }
        if (extraBudget > 0)
        {
          int distributed = 0;
          for (int i = 0; i < count; ++i)
          {
            const int extraForTab = (naturalWidths[i] - minWidth) * remaining / extraBudget;
            tabWidths_[i] = minWidth + extraForTab;
            distributed += extraForTab;
          }
          int leftover = remaining - distributed;
          for (int i = 0; leftover > 0 && i < count; ++i)
          {
            if (tabWidths_[i] < maxWidth)
            {
              ++tabWidths_[i];
              --leftover;
            }
          }
        }
      }
    }
  }

  int x = 0;
  for (int i = 0; i < count; ++i)
  {
    tabRects_[i] = {x, 0, x + tabWidths_[i], 32767};
    x += tabWidths_[i];
  }
  contentWidth_ = x;
  ClampScrollOffset();
}

void TabBar::ClampScrollOffset()
{
  if (!hwnd_)
  {
    return;
  }

  RECT client = {};
  GetClientRect(hwnd_, &client);
  const int viewWidth = client.right - client.left;
  const int maxScroll = contentWidth_ > viewWidth ? contentWidth_ - viewWidth : 0;
  if (maxScroll == 0)
  {
    scrollOffset_ = 0;
    return;
  }

  if (scrollOffset_ < 0)
  {
    scrollOffset_ = 0;
  }
  if (scrollOffset_ > maxScroll)
  {
    scrollOffset_ = maxScroll;
  }
}

void TabBar::UpdateLayout()
{
  if (!hwnd_)
  {
    return;
  }

  RECT client = {};
  GetClientRect(hwnd_, &client);
  ComputeTabWidths(client.right - client.left);
}

int TabBar::HitTestTab(int x, int y) const
{
  const int adjustedX = x + scrollOffset_;
  for (size_t i = 0; i < tabRects_.size(); ++i)
  {
    RECT rect = tabRects_[i];
    rect.top = 0;
    RECT client = {};
    GetClientRect(hwnd_, &client);
    rect.bottom = client.bottom > kBottomBorder ? client.bottom - kBottomBorder : client.bottom;
    if (adjustedX >= rect.left && adjustedX < rect.right && y >= 0 && y < rect.bottom)
    {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int TabBar::HitTestClose(int x, int y) const
{
  POINT point = {x, y};
  for (size_t i = 0; i < closeRects_.size(); ++i)
  {
    RECT rect = closeRects_[i];
    if (PtInRect(&rect, point))
    {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void TabBar::NotifyParent(UINT message, int tabIndex) const
{
  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, message, static_cast<WPARAM>(tabIndex),
                 static_cast<LPARAM>(groupId_));
  }
}

void TabBar::NotifyParentReorder(int fromIndex, int insertBefore) const
{
  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_TABBAR_REORDER, static_cast<WPARAM>(fromIndex),
                 MAKELPARAM(static_cast<WORD>(insertBefore), static_cast<WORD>(groupId_)));
  }
}

void TabBar::NotifyDragSessionEnd() const
{
  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_TABBAR_DRAG_CANCEL, static_cast<WPARAM>(groupId_), 0);
  }
}

void TabBar::ShowDragGhost()
{
  if (!hwnd_ || dragSourceIndex_ < 0 || dragSourceIndex_ >= static_cast<int>(titles_.size()))
  {
    return;
  }

  HideDragGhost();

  HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));
  if (!RegisterDragGhostClass(instance))
  {
    return;
  }

  const UiThemeChrome& theme = UiTheme::Resolve(hwnd_).chrome;
  const COLORREF ghostBack = theme.dragGhostBack;
  const COLORREF ghostText = theme.dragGhostText;
  const int dpi = static_cast<int>(GetDpiForWindow(hwnd_));
  const int padX = MulDiv(7, dpi, 96);
  const int padY = MulDiv(1, dpi, 96);
  const int maxWidth = MulDiv(kDragGhostMaxWidth, dpi, 96);
  const int corner = MulDiv(10, dpi, 96);

  HDC screenDc = GetDC(nullptr);
  HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd_, WM_GETFONT, 0, 0));
  if (!font)
  {
    font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  }

  const SIZE textSize = MeasureDragGhostLabel(screenDc, font, titles_[dragSourceIndex_], maxWidth);
  ReleaseDC(nullptr, screenDc);

  const int ghostW = textSize.cx + padX * 2;
  const int ghostH = textSize.cy + padY * 2;
  if (ghostW <= 0 || ghostH <= 0)
  {
    return;
  }

  dragGhost_ = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kDragGhostClassName,
      L"",
      WS_POPUP,
      0,
      0,
      ghostW,
      ghostH,
      nullptr,
      nullptr,
      instance,
      this);

  if (!dragGhost_)
  {
    return;
  }

  HDC hdc = GetDC(dragGhost_);
  HDC memDc = CreateCompatibleDC(hdc);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = ghostW;
  bmi.bmiHeader.biHeight = -ghostH;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HBITMAP oldBmp = reinterpret_cast<HBITMAP>(SelectObject(memDc, dib));

  RECT fill = {0, 0, ghostW, ghostH};
  HBRUSH back = CreateSolidBrush(ghostBack);
  FillRect(memDc, &fill, back);
  DeleteObject(back);

  HPEN outline = CreatePen(PS_SOLID, 1, ghostBack);
  HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(memDc, outline));
  HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(memDc, GetStockObject(NULL_BRUSH)));
  RoundRect(memDc, 0, 0, ghostW, ghostH, corner, corner);
  SelectObject(memDc, oldBrush);
  SelectObject(memDc, oldPen);
  DeleteObject(outline);

  SetBkMode(memDc, TRANSPARENT);
  UNREFERENCED_PARAMETER(theme);
  SetTextColor(memDc, ghostText);
  HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(memDc, font));
  RECT text = {padX, padY, ghostW - padX, ghostH - padY};
  DrawTextW(memDc, titles_[dragSourceIndex_].c_str(), -1, &text,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  SelectObject(memDc, oldFont);

  POINT pt = {};
  GetCursorPos(&pt);
  const int offsetX = MulDiv(kDragGhostOffsetX, dpi, 96);
  const int offsetY = MulDiv(kDragGhostOffsetY, dpi, 96);
  POINT ptDest = {pt.x - offsetX, pt.y - offsetY};
  POINT origin = {};
  SIZE size = {ghostW, ghostH};
  BLENDFUNCTION blend = {};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  UpdateLayeredWindow(dragGhost_, hdc, &ptDest, &size, memDc, &origin, 0, &blend, ULW_ALPHA);

  SelectObject(memDc, oldBmp);
  DeleteObject(dib);
  DeleteDC(memDc);
  ReleaseDC(dragGhost_, hdc);

  ShowWindow(dragGhost_, SW_SHOWNOACTIVATE);
}

void TabBar::UpdateDragGhost(POINT screenPoint)
{
  if (!dragGhost_)
  {
    return;
  }

  RECT rect = {};
  GetWindowRect(dragGhost_, &rect);
  const int w = rect.right - rect.left;
  const int h = rect.bottom - rect.top;
  const int dpi = static_cast<int>(GetDpiForWindow(hwnd_));
  const int offsetX = MulDiv(kDragGhostOffsetX, dpi, 96);
  const int offsetY = MulDiv(kDragGhostOffsetY, dpi, 96);

  SetWindowPos(dragGhost_, HWND_TOPMOST, screenPoint.x - offsetX, screenPoint.y - offsetY, w, h,
               SWP_NOACTIVATE | SWP_NOSIZE);
}

void TabBar::HideDragGhost()
{
  if (dragGhost_)
  {
    DestroyWindow(dragGhost_);
    dragGhost_ = nullptr;
  }
}

void TabBar::CancelDrag()
{
  if (isDragging_ || dragSourceIndex_ >= 0)
  {
    NotifyDragSessionEnd();
  }
  HideDragGhost();
  dragSourceIndex_ = -1;
  dragInsertBefore_ = -1;
  isDragging_ = false;
  if (hwnd_ && GetCapture() == hwnd_)
  {
    ReleaseCapture();
  }
  if (hwnd_)
  {
    InvalidateRect(hwnd_, nullptr, TRUE);
  }
}

void TabBar::EndDrag(bool commit)
{
  const bool wasDragging = isDragging_;
  if (wasDragging || dragSourceIndex_ >= 0)
  {
    NotifyDragSessionEnd();
  }
  HideDragGhost();

  if (wasDragging && commit && dragSourceIndex_ >= 0)
  {
    POINT pt = {};
    GetCursorPos(&pt);
    RECT tabRect = {};
    GetWindowRect(hwnd_, &tabRect);

    if (PtInRect(&tabRect, pt))
    {
      const int from = dragSourceIndex_;
      const int insertBefore = dragInsertBefore_;
      if (insertBefore != from && insertBefore != from + 1)
      {
        NotifyParentReorder(from, insertBefore);
      }
    }
    else if (notifyHwnd_)
    {
      SendMessageW(notifyHwnd_, WM_TABBAR_DRAG_END,
                   MAKELPARAM(static_cast<WORD>(dragSourceIndex_), static_cast<WORD>(groupId_)),
                   PackScreenPoint(pt));
    }
  }

  dragSourceIndex_ = -1;
  dragInsertBefore_ = -1;
  isDragging_ = false;
  if (hwnd_ && GetCapture() == hwnd_)
  {
    ReleaseCapture();
  }
  if (hwnd_)
  {
    InvalidateRect(hwnd_, nullptr, TRUE);
  }
}

int TabBar::HitTestInsertIndex(int x) const
{
  return TabBarLogic::HitTestInsertIndex(x, scrollOffset_, tabRects_);
}

int TabBar::GetInsertMarkerX(int insertBefore) const
{
  if (tabRects_.empty())
  {
    return 0;
  }

  if (insertBefore <= 0)
  {
    return tabRects_[0].left - scrollOffset_;
  }

  if (insertBefore >= static_cast<int>(tabRects_.size()))
  {
    return tabRects_.back().right - scrollOffset_;
  }

  return (tabRects_[insertBefore - 1].right + tabRects_[insertBefore].left) / 2 - scrollOffset_;
}

void TabBar::OnPaint(HDC hdc)
{
  RECT client = {};
  GetClientRect(hwnd_, &client);
  ClampScrollOffset();

  const UiThemeChrome& theme = UiTheme::Resolve(hwnd_).chrome;
  HBRUSH backBrush = CreateSolidBrush(theme.stripBackground);
  FillRect(hdc, &client, backBrush);
  DeleteObject(backBrush);

  const int closeWidth = GetCloseButtonWidth();
  const int padding = GetTabPadding();

  HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd_, WM_GETFONT, 0, 0));
  if (!font)
  {
    font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  }
  HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, font));
  SetBkMode(hdc, TRANSPARENT);

  const int viewBottom = client.bottom > kBottomBorder ? client.bottom - kBottomBorder : client.bottom;

  for (size_t i = 0; i < titles_.size(); ++i)
  {
    RECT rc = tabRects_[i];
    rc.left -= scrollOffset_;
    rc.right -= scrollOffset_;
    rc.top = 0;
    rc.bottom = viewBottom;

    if (rc.right <= 0 || rc.left >= client.right)
    {
      continue;
    }

    const bool selected = static_cast<int>(i) == selectedIndex_;
    const bool hovered = static_cast<int>(i) == hoverTabIndex_ && !selected;
    const bool isDragSource = isDragging_ && static_cast<int>(i) == dragSourceIndex_;

    COLORREF backColor = theme.inactiveBackground;
    if (selected)
    {
      backColor = theme.activeBackground;
    }
    else if (hovered)
    {
      backColor = theme.hoverBackground;
    }
    if (isDragSource)
    {
      backColor = theme.stripBackground;
    }

    const COLORREF textColor =
        (selected && !isDragSource) ? theme.activeForeground : theme.inactiveForeground;

    if (selected && client.bottom > viewBottom)
    {
      rc.bottom = client.bottom;
    }

    if (selected && !isDragSource)
    {
      TRIVERTEX vertices[2] = {};
      vertices[0].x = static_cast<LONG>(rc.left);
      vertices[0].y = static_cast<LONG>(rc.top);
      vertices[0].Red = static_cast<COLOR16>(GetRValue(backColor) << 8);
      vertices[0].Green = static_cast<COLOR16>(GetGValue(backColor) << 8);
      vertices[0].Blue = static_cast<COLOR16>(GetBValue(backColor) << 8);
      vertices[0].Alpha = 0xFF00;

      const COLORREF topColor = GetSysColor(COLOR_3DHILIGHT);
      vertices[1].x = static_cast<LONG>(rc.right);
      vertices[1].y = static_cast<LONG>(rc.bottom);
      vertices[1].Red = static_cast<COLOR16>(GetRValue(topColor) << 8);
      vertices[1].Green = static_cast<COLOR16>(GetGValue(topColor) << 8);
      vertices[1].Blue = static_cast<COLOR16>(GetBValue(topColor) << 8);
      vertices[1].Alpha = 0xFF00;

      GRADIENT_RECT gradientRect = {0, 1};
      GradientFill(hdc, vertices, 2, &gradientRect, 1, GRADIENT_FILL_RECT_V);
      DrawEdge(hdc, &rc, EDGE_RAISED, BF_RECT | BF_SOFT);
    }
    else
    {
      HBRUSH tabBrush = CreateSolidBrush(backColor);
      FillRect(hdc, &rc, tabBrush);
      DeleteObject(tabBrush);
    }

    if (selected && client.bottom > viewBottom)
    {
      rc.bottom = viewBottom;
    }

    DrawVerticalLine(hdc, rc.right - 1, rc.top, rc.bottom, theme.separator);

    RECT closeRect = rc;
    closeRect.left = closeRect.right - closeWidth - padding;
    closeRect.top += padding / 2;
    closeRect.bottom -= padding / 2;
    closeRect.right -= padding / 2;
    closeRects_[i] = closeRect;

    RECT textRect = rc;
    textRect.left += padding;
    textRect.right = closeRect.left - padding;
    if (textRect.right <= textRect.left)
    {
      textRect.right = rc.right - padding;
    }

    SetTextColor(hdc, textColor);
    DrawTextW(hdc, titles_[i].c_str(), -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (static_cast<int>(i) == hoverCloseIndex_)
    {
      HBRUSH hoverBrush = CreateSolidBrush(theme.hoverBackground);
      FillRect(hdc, &closeRect, hoverBrush);
      DeleteObject(hoverBrush);
    }

    if (!isDragSource)
    {
      DrawTextW(hdc, L"\x00D7", -1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
  }

  if (isDragging_ && dragInsertBefore_ >= 0)
  {
    const int markerX = GetInsertMarkerX(dragInsertBefore_);
    if (markerX >= 0 && markerX < client.right)
    {
      const int markerW = MulDiv(3, static_cast<int>(GetDpiForWindow(hwnd_)), 96);
      RECT marker = {markerX - markerW / 2, 2, markerX + markerW / 2 + 1, viewBottom - 2};
      if (marker.top < marker.bottom)
      {
        HBRUSH markerBrush = CreateSolidBrush(theme.accent);
        FillRect(hdc, &marker, markerBrush);
        DeleteObject(markerBrush);
      }
    }
  }

  if (viewBottom < client.bottom)
  {
    HPEN borderPen = CreatePen(PS_SOLID, 1, theme.bottomBorder);
    HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, borderPen));
    MoveToEx(hdc, 0, client.bottom - 1, nullptr);
    LineTo(hdc, client.right, client.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
  }

  SelectObject(hdc, oldFont);
}

LRESULT CALLBACK TabBar::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  TabBar* self = nullptr;

  if (msg == WM_NCCREATE)
  {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<TabBar*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }
  else
  {
    self = reinterpret_cast<TabBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (!self)
  {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  switch (msg)
  {
    case WM_PAINT:
    {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT client = {};
      GetClientRect(hwnd, &client);
      const int width = client.right - client.left;
      const int height = client.bottom - client.top;
      if (width > 0 && height > 0)
      {
        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
        HBITMAP oldBitmap = reinterpret_cast<HBITMAP>(SelectObject(memDc, bitmap));
        self->OnPaint(memDc);
        BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
        SelectObject(memDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memDc);
      }
      else
      {
        self->OnPaint(hdc);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      self->UpdateLayout();
      self->ClampScrollOffset();
      InvalidateRect(hwnd, nullptr, TRUE);
      return 0;
    case WM_LBUTTONDOWN:
    {
      const int x = GET_X_LPARAM(lParam);
      const int y = GET_Y_LPARAM(lParam);
      const int closeIndex = self->HitTestClose(x, y);
      if (closeIndex >= 0)
      {
        self->NotifyParent(WM_TABBAR_CLOSE, closeIndex);
        return 0;
      }

      const int tabIndex = self->HitTestTab(x, y);
      if (tabIndex >= 0)
      {
        self->dragSourceIndex_ = tabIndex;
        self->dragInsertBefore_ = tabIndex;
        self->dragStartX_ = x;
        self->dragStartY_ = y;
        self->isDragging_ = false;
        SetCapture(hwnd);

        if (tabIndex != self->selectedIndex_)
        {
          self->selectedIndex_ = tabIndex;
          self->NotifyParent(WM_TABBAR_SELECT, tabIndex);
        }
        InvalidateRect(hwnd, nullptr, TRUE);
      }
      return 0;
    }
    case WM_LBUTTONUP:
      if (GetCapture() == hwnd)
      {
        self->EndDrag(self->isDragging_);
        return 0;
      }
      break;
    case WM_CAPTURECHANGED:
      if (reinterpret_cast<HWND>(lParam) != hwnd)
      {
        self->CancelDrag();
      }
      return 0;
    case WM_MOUSEMOVE:
    {
      const int x = GET_X_LPARAM(lParam);
      const int y = GET_Y_LPARAM(lParam);

      if (GetCapture() == hwnd && self->dragSourceIndex_ >= 0)
      {
        if (!self->isDragging_)
        {
          const int dx = x - self->dragStartX_;
          const int dy = y - self->dragStartY_;
          if (abs(dx) >= GetSystemMetrics(SM_CXDRAG) || abs(dy) >= GetSystemMetrics(SM_CYDRAG))
          {
            self->isDragging_ = true;
            SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
            self->ShowDragGhost();
          }
        }

        if (self->isDragging_)
        {
          POINT screen = {};
          GetCursorPos(&screen);
          self->UpdateDragGhost(screen);

          RECT client = {};
          GetClientRect(hwnd, &client);
          const int margin = MulDiv(32, static_cast<int>(GetDpiForWindow(hwnd)), 96);
          const int scrollStep = MulDiv(14, static_cast<int>(GetDpiForWindow(hwnd)), 96);
          if (x < margin)
          {
            self->scrollOffset_ -= scrollStep;
            self->ClampScrollOffset();
          }
          else if (x > client.right - margin)
          {
            self->scrollOffset_ += scrollStep;
            self->ClampScrollOffset();
          }

          const int insertBefore = self->HitTestInsertIndex(x);
          if (insertBefore != self->dragInsertBefore_)
          {
            self->dragInsertBefore_ = insertBefore;
            InvalidateRect(hwnd, nullptr, TRUE);
          }

          if (self->notifyHwnd_)
          {
            POINT screen = {};
            GetCursorPos(&screen);
            RECT tabBarScreen = {};
            GetWindowRect(hwnd, &tabBarScreen);
            if (!PtInRect(&tabBarScreen, screen))
            {
              SendMessageW(self->notifyHwnd_, WM_TABBAR_DRAG_MOVE,
                           MAKELPARAM(static_cast<WORD>(self->dragSourceIndex_),
                                      static_cast<WORD>(self->groupId_)),
                           PackScreenPoint(screen));
            }
          }
        }
        return 0;
      }

      TRACKMOUSEEVENT track = {};
      track.cbSize = sizeof(track);
      track.dwFlags = TME_LEAVE;
      track.hwndTrack = hwnd;
      TrackMouseEvent(&track);

      const int closeIndex = self->HitTestClose(x, y);
      const int tabIndex = closeIndex >= 0 ? -1 : self->HitTestTab(x, y);
      bool needsRedraw = false;
      if (closeIndex != self->hoverCloseIndex_)
      {
        self->hoverCloseIndex_ = closeIndex;
        needsRedraw = true;
      }
      if (tabIndex != self->hoverTabIndex_)
      {
        self->hoverTabIndex_ = tabIndex;
        needsRedraw = true;
      }
      if (needsRedraw)
      {
        InvalidateRect(hwnd, nullptr, TRUE);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      if (GetCapture() != hwnd &&
          (self->hoverCloseIndex_ != -1 || self->hoverTabIndex_ != -1))
      {
        self->hoverCloseIndex_ = -1;
        self->hoverTabIndex_ = -1;
        InvalidateRect(hwnd, nullptr, TRUE);
      }
      return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    {
      const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
      const int step = MulDiv(80, static_cast<int>(GetDpiForWindow(hwnd)), 96);
      self->scrollOffset_ -= (delta / WHEEL_DELTA) * step;
      self->ClampScrollOffset();
      InvalidateRect(hwnd, nullptr, TRUE);
      return 0;
    }
    case WM_CONTEXTMENU:
    {
      POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      if (point.x == -1 && point.y == -1)
      {
        GetCursorPos(&point);
      }

      POINT clientPoint = point;
      ScreenToClient(hwnd, &clientPoint);
      const int tabIndex = self->HitTestTab(clientPoint.x, clientPoint.y);
      if (tabIndex >= 0)
      {
        self->NotifyParent(WM_TABBAR_CONTEXTMENU, tabIndex);
        return 0;
      }
      break;
    }
    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
