#include "CommandPalette.h"

#include "CommandRegistry.h"
#include "DialogLayout.h"
#include "UiHelpers.h"
#include "UiMetrics.h"

#include <algorithm>
#include <windowsx.h>

namespace
{
constexpr wchar_t kPaletteClass[] = L"WinCppCommandPalette";
constexpr int kPaletteWidth = 520;
constexpr int kPaletteHeight = 360;
constexpr int kFilterId = 1;
constexpr int kListId = 2;

bool RegisterPaletteClass()
{
  WNDCLASSEXW wc = {};
  if (GetClassInfoExW(GetModuleHandleW(nullptr), kPaletteClass, &wc))
  {
    return true;
  }

  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = CommandPalette::WindowProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kPaletteClass;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  return RegisterClassExW(&wc) != 0;
}
}

void CommandPalette::LayoutControls()
{
  if (!hwnd_ || !filterEdit_ || !listBox_)
  {
    return;
  }

  RECT client = {};
  GetClientRect(hwnd_, &client);
  const DialogMetrics m = DialogLayout::GetMetrics(hwnd_);

  const int filterHeight = m.editHeight;
  const int listTop = m.margin + filterHeight + m.gap;
  const int listHeight = client.bottom - listTop - m.margin;

  MoveWindow(filterEdit_, m.margin, m.margin, client.right - 2 * m.margin, filterHeight, TRUE);
  MoveWindow(listBox_, m.margin, listTop, client.right - 2 * m.margin,
             (std::max)(listHeight, m.rowHeight), TRUE);
}

void CommandPalette::Show(HWND owner, const std::vector<CommandEntry>& commands, ExecuteFn execute)
{
  Hide();
  allCommands_ = commands;
  execute_ = std::move(execute);
  filtered_ = allCommands_;

  if (!RegisterPaletteClass())
  {
    return;
  }

  constexpr DWORD kPaletteStyle = WS_CAPTION | WS_SYSMENU | WS_POPUP;
  constexpr DWORD kPaletteExStyle = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST;
  const int clientWidth = UiMetrics::ScalePx(owner, kPaletteWidth);
  const int clientHeight = UiMetrics::ScalePx(owner, kPaletteHeight);
  const DialogWindowSize outer =
      DialogLayout::WindowSizeForClient(kPaletteStyle, kPaletteExStyle, clientWidth, clientHeight);

  hwnd_ = CreateWindowExW(
      kPaletteExStyle,
      kPaletteClass,
      L"Command Palette",
      kPaletteStyle,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      outer.width,
      outer.height,
      owner,
      nullptr,
      GetModuleHandleW(nullptr),
      this);

  if (!hwnd_)
  {
    return;
  }

  RECT parentRect = {};
  GetWindowRect(owner, &parentRect);
  const int x = parentRect.left + (parentRect.right - parentRect.left - outer.width) / 2;
  const int y = parentRect.top + (parentRect.bottom - parentRect.top - outer.height) / 4;
  SetWindowPos(hwnd_, HWND_TOP, x, y, outer.width, outer.height, SWP_SHOWWINDOW);

  filterEdit_ = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFilterId)), GetModuleHandleW(nullptr), nullptr);
  listBox_ = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"LISTBOX", L"",
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 0, 0, hwnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListId)), GetModuleHandleW(nullptr), nullptr);

  ApplyUiFont(filterEdit_);
  ApplyUiFont(listBox_);
  ApplyEditInnerMargins(filterEdit_, 4);

  LayoutControls();
  FilterAndRefresh();
  SetFocus(filterEdit_);
  SendMessageW(filterEdit_, EM_SETSEL, 0, -1);
}

void CommandPalette::Hide()
{
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    filterEdit_ = nullptr;
    listBox_ = nullptr;
  }
}

void CommandPalette::FilterAndRefresh()
{
  if (!listBox_ || !filterEdit_)
  {
    return;
  }

  wchar_t buffer[512] = {};
  GetWindowTextW(filterEdit_, buffer, static_cast<int>(std::size(buffer)));
  filtered_ = CommandRegistry::Filter(allCommands_, buffer);

  SendMessageW(listBox_, LB_RESETCONTENT, 0, 0);
  for (const CommandEntry& entry : filtered_)
  {
    SendMessageW(listBox_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.label.c_str()));
  }

  if (!filtered_.empty())
  {
    SendMessageW(listBox_, LB_SETCURSEL, 0, 0);
  }
}

void CommandPalette::ExecuteSelected()
{
  if (!execute_ || !listBox_)
  {
    return;
  }

  const int index = static_cast<int>(SendMessageW(listBox_, LB_GETCURSEL, 0, 0));
  if (index < 0 || index >= static_cast<int>(filtered_.size()))
  {
    return;
  }

  const unsigned int commandId = filtered_[static_cast<size_t>(index)].id;
  Hide();
  execute_(commandId);
}

LRESULT CALLBACK CommandPalette::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  CommandPalette* self = nullptr;

  if (msg == WM_NCCREATE)
  {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<CommandPalette*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }
  else
  {
    self = reinterpret_cast<CommandPalette*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (!self)
  {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  switch (msg)
  {
    case WM_SIZE:
      self->LayoutControls();
      return 0;
    case WM_COMMAND:
      if (LOWORD(wParam) == kFilterId && HIWORD(wParam) == EN_CHANGE)
      {
        self->FilterAndRefresh();
        return 0;
      }
      if (LOWORD(wParam) == kListId && HIWORD(wParam) == LBN_DBLCLK)
      {
        self->ExecuteSelected();
        return 0;
      }
      break;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE)
      {
        self->Hide();
        return 0;
      }
      if (wParam == VK_RETURN)
      {
        self->ExecuteSelected();
        return 0;
      }
      if (wParam == VK_DOWN)
      {
        if (GetFocus() == self->filterEdit_ && self->listBox_)
        {
          SetFocus(self->listBox_);
          SendMessageW(self->listBox_, LB_SETCURSEL, 0, 0);
          return 0;
        }
        if (self->listBox_)
        {
          const int count = static_cast<int>(SendMessageW(self->listBox_, LB_GETCOUNT, 0, 0));
          int sel = static_cast<int>(SendMessageW(self->listBox_, LB_GETCURSEL, 0, 0));
          if (sel < count - 1)
          {
            SendMessageW(self->listBox_, LB_SETCURSEL, sel + 1, 0);
          }
          return 0;
        }
      }
      if (wParam == VK_UP)
      {
        if (self->listBox_ && GetFocus() == self->listBox_)
        {
          int sel = static_cast<int>(SendMessageW(self->listBox_, LB_GETCURSEL, 0, 0));
          if (sel <= 0)
          {
            SetFocus(self->filterEdit_);
            return 0;
          }
          SendMessageW(self->listBox_, LB_SETCURSEL, sel - 1, 0);
          return 0;
        }
      }
      break;
    case WM_DESTROY:
      self->hwnd_ = nullptr;
      self->filterEdit_ = nullptr;
      self->listBox_ = nullptr;
      return 0;
    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
