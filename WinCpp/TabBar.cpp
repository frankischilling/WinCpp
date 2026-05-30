#include "TabBar.h"

#include <windowsx.h>

namespace
{
constexpr wchar_t kTabBarClassName[] = L"WinCppTabBar";
constexpr int kTabWidthFit = 120;
constexpr int kTabWidthMin = 50;
constexpr int kTabWidthMax = 160;
constexpr int kDefaultHeight = 28;
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

  SendMessageW(hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
  return true;
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

  return metrics.tmHeight + MulDiv(10, static_cast<int>(dpi), 96);
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
  return MulDiv(8, static_cast<int>(GetDpiForWindow(hwnd_)), 96);
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
    rect.bottom = client.bottom;
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
  if (parent_)
  {
    SendMessageW(parent_, message, static_cast<WPARAM>(tabIndex), reinterpret_cast<LPARAM>(hwnd_));
  }
}

void TabBar::OnPaint(HDC hdc)
{
  RECT client = {};
  GetClientRect(hwnd_, &client);
  ClampScrollOffset();

  HBRUSH backBrush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
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

  const int viewBottom = client.bottom;

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
    const COLORREF backColor = selected ? GetSysColor(COLOR_WINDOW) : GetSysColor(COLOR_3DFACE);
    const COLORREF textColor = GetSysColor(COLOR_WINDOWTEXT);

    HBRUSH tabBrush = CreateSolidBrush(backColor);
    FillRect(hdc, &rc, tabBrush);
    DeleteObject(tabBrush);

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
      HBRUSH hoverBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
      FillRect(hdc, &closeRect, hoverBrush);
      DeleteObject(hoverBrush);
    }

    DrawTextW(hdc, L"\x00D7", -1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
      self->OnPaint(hdc);
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
        self->selectedIndex_ = tabIndex;
        self->NotifyParent(WM_TABBAR_SELECT, tabIndex);
        InvalidateRect(hwnd, nullptr, TRUE);
      }
      return 0;
    }
    case WM_MOUSEMOVE:
    {
      TRACKMOUSEEVENT track = {};
      track.cbSize = sizeof(track);
      track.dwFlags = TME_LEAVE;
      track.hwndTrack = hwnd;
      TrackMouseEvent(&track);

      const int closeIndex = self->HitTestClose(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      if (closeIndex != self->hoverCloseIndex_)
      {
        self->hoverCloseIndex_ = closeIndex;
        InvalidateRect(hwnd, nullptr, TRUE);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      if (self->hoverCloseIndex_ != -1)
      {
        self->hoverCloseIndex_ = -1;
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
