#include "MainWindow.h"

#include "CommandRegistry.h"
#include "DocumentCollectionLogic.h"
#include "DocumentNaming.h"
#include "PathLineParser.h"
#include "ProcessOutput.h"
#include "SessionState.h"
#include "StatusBarModel.h"
#include "TreeFilterLogic.h"

#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <shlobj.h>
#include <windowsx.h>

#include <Scintilla.h>

#include "UiHelpers.h"

namespace
{
constexpr wchar_t kMainWindowClassName[] = L"WinCppMainWindow";
constexpr int kDefaultTabStripHeight = 28;
constexpr int kStatusCaretWidth = 180;
#ifndef SB_SETBORDERS
#define SB_SETBORDERS (WM_USER + 6)
#endif
constexpr int kMinWindowWidth = 900;
constexpr int kMinWindowHeight = 500;
constexpr int kFindDlgFindEdit = 3001;
constexpr int kFindDlgReplaceEdit = 3002;
constexpr int kFindDlgMatchCase = 3003;
constexpr int kFindDlgWholeWord = 3004;
constexpr int kFindDlgFindNext = 3005;
constexpr int kFindDlgReplace = 3006;
constexpr int kFindDlgReplaceAll = 3007;
constexpr int kFindDlgClose = 3008;
constexpr int kFindDlgRegex = 3009;
constexpr int kFindDlgFindPrevious = 3010;

constexpr int kGoToDlgLineEdit = 3101;
constexpr int kGoToDlgOk = 3102;
constexpr int kGoToDlgCancel = 3103;

constexpr int kRunDlgCommandEdit = 3301;
constexpr int kRunDlgOk = 3302;
constexpr int kRunDlgCancel = 3303;

std::wstring g_lastRunCommand = L"echo hello";

constexpr int kCreditsDlgText = 3201;
constexpr int kCreditsDlgClose = 3202;

constexpr UINT_PTR kEditorSubclassId = 2;
constexpr UINT_PTR kProjectHeaderSubclassId = 3;
constexpr int kTabBottomBorder = 1;

std::filesystem::path GetExecutableDir()
{
  wchar_t buffer[MAX_PATH] = {};
  const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH)
  {
    return std::filesystem::current_path();
  }

  return std::filesystem::path(buffer).parent_path();
}

std::wstring Utf8ToWide(const std::string& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0);
  if (length <= 0)
  {
    return {};
  }

  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      result.data(), length);
  return result;
}

std::string WideToUtf8(const std::wstring& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (length <= 0)
  {
    return {};
  }

  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      result.data(), length, nullptr, nullptr);
  return result;
}

std::wstring BuildDateTimeWide()
{
  const std::string stamp = std::string(__DATE__) + " " + __TIME__;
  return Utf8ToWide(stamp);
}

std::wstring BuildAboutText()
{
  std::wstring text = L"WinCpp\r\n\r\nA simple text editor for Windows.\r\n\r\n";
  text += L"Built: ";
  text += BuildDateTimeWide();
  text += L"\r\n\r\nAuthor: github.com/frankischilling\r\n";
  text += L"Project: https://github.com/frankischilling/WinCpp\r\n";
  return text;
}

std::wstring BuildCreditsText()
{
  std::wstring text = L"WinCpp\r\nA simple text editor for Windows.\r\n\r\n";
  text += L"Built: ";
  text += BuildDateTimeWide();
  text += L"\r\nAuthor: github.com/frankischilling\r\n";
  text += L"Project: https://github.com/frankischilling/WinCpp\r\n";
  text +=
      L"\r\n"
      L"Syntax highlighting\r\n"
      L"The syntax files in the syntax/ folder come from the micro editor:\r\n"
      L"  https://github.com/zyedidia/micro\r\n"
      L"Many of those files were converted from Nano syntax (nanorc):\r\n"
      L"  https://github.com/scopatz/nanorc\r\n"
      L"\r\n"
      L"Third-party libraries\r\n"
      L"  Scintilla  https://www.scintilla.org/\r\n"
      L"  yaml-cpp   https://github.com/jbeder/yaml-cpp\r\n"
      L"  PCRE2      https://github.com/PCRE2Project/pcre2\r\n"
      L"\r\n"
      L"Thank you to the authors and contributors of these projects.";
  return text;
}

const wchar_t* GetDialogFilter()
{
  static const wchar_t kFilter[] =
      L"All Files (*.*)\0*.*\0"
      L"C/C++ Files (*.c;*.cpp;*.h;*.hpp;*.cxx;*.hxx)\0*.c;*.cpp;*.h;*.hpp;*.cxx;*.hxx\0"
      L"Text Files (*.txt)\0*.txt\0\0";
  return kFilter;
}

std::wstring ShowOpenFileDialog(HWND owner)
{
  wchar_t filePath[MAX_PATH] = {};
  OPENFILENAMEW ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFilter = GetDialogFilter();
  ofn.lpstrFile = filePath;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

  if (!GetOpenFileNameW(&ofn))
  {
    return {};
  }

  return filePath;
}

std::wstring ShowSaveFileDialog(HWND owner, const std::wstring& initialPath)
{
  wchar_t filePath[MAX_PATH] = {};
  if (!initialPath.empty())
  {
    wcsncpy_s(filePath, initialPath.c_str(), _TRUNCATE);
  }

  OPENFILENAMEW ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFilter = GetDialogFilter();
  ofn.lpstrFile = filePath;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
  ofn.lpstrDefExt = L"txt";

  if (!GetSaveFileNameW(&ofn))
  {
    return {};
  }

  return filePath;
}

std::wstring ShowFolderDialog(HWND owner)
{
  wchar_t displayName[MAX_PATH] = {};
  BROWSEINFOW browseInfo = {};
  browseInfo.hwndOwner = owner;
  browseInfo.pszDisplayName = displayName;
  browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&browseInfo);
  if (!pidl)
  {
    return {};
  }

  wchar_t folderPath[MAX_PATH] = {};
  const BOOL ok = SHGetPathFromIDListW(pidl, folderPath);
  CoTaskMemFree(pidl);
  return ok ? std::wstring(folderPath) : std::wstring();
}

HWND CreateLabeledEdit(HWND parent, int x, int y, int width, int id)
{
  return CreateWindowExW(
      WS_EX_CLIENTEDGE,
      L"EDIT",
      L"",
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      x,
      y,
      width,
      kUiButtonHeight,
      parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr),
      nullptr);
}

HWND CreateDialogButton(HWND parent, const wchar_t* label, int x, int y, int width, int id)
{
  return CreateWindowExW(
      0,
      L"BUTTON",
      label,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x,
      y,
      width,
      kUiButtonHeight,
      parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr),
      nullptr);
}

HWND CreateDialogCheck(HWND parent, const wchar_t* label, int x, int y, int width, int id)
{
  return CreateWindowExW(
      0,
      L"BUTTON",
      label,
      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      x,
      y,
      width,
      22,
      parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr),
      nullptr);
}

std::wstring GetWindowTextString(HWND hwnd)
{
  const int length = GetWindowTextLengthW(hwnd);
  if (length <= 0)
  {
    return {};
  }

  std::wstring text(length, L'\0');
  GetWindowTextW(hwnd, text.data(), length + 1);
  return text;
}

struct FindDialogState
{
  MainWindow* window = nullptr;
  bool replaceMode = false;
};

struct GoToDialogState
{
  MainWindow* window = nullptr;
  int maxLine = 1;
};

struct RunDialogState
{
  MainWindow* window = nullptr;
};

LRESULT CALLBACK FindDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  FindDialogState* state = reinterpret_cast<FindDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg)
  {
    case WM_CREATE:
    {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
      state = reinterpret_cast<FindDialogState*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

      RECT client = {};
      GetClientRect(hwnd, &client);
      const int labelWidth = 64;
      const int editX = kUiMargin + labelWidth;
      const int editWidth = client.right - editX - kUiMargin;
      const int rowHeight = 28;

      CreateWindowExW(0, L"STATIC", L"Find:", WS_CHILD | WS_VISIBLE, kUiMargin, kUiMargin + 2,
                      labelWidth, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
      CreateLabeledEdit(hwnd, editX, kUiMargin, editWidth, kFindDlgFindEdit);

      int optionY = kUiMargin + rowHeight + kUiGap;
      if (state->replaceMode)
      {
        CreateWindowExW(0, L"STATIC", L"Replace:", WS_CHILD | WS_VISIBLE, kUiMargin, optionY + 2,
                        labelWidth, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        CreateLabeledEdit(hwnd, editX, optionY, editWidth, kFindDlgReplaceEdit);
        optionY += rowHeight + kUiGap;
      }

      CreateDialogCheck(hwnd, L"Match case", kUiMargin, optionY, 120, kFindDlgMatchCase);
      CreateDialogCheck(hwnd, L"Whole word", kUiMargin + 128, optionY, 120, kFindDlgWholeWord);
      CreateDialogCheck(hwnd, L"Regex", kUiMargin + 256, optionY, 80, kFindDlgRegex);
      optionY += rowHeight;

      const int buttonY = client.bottom - kUiMargin - kUiButtonHeight;
      CreateDialogButton(hwnd, L"Find Next", kUiMargin, buttonY, 92, kFindDlgFindNext);
      CreateDialogButton(hwnd, L"Find Previous", kUiMargin + 100, buttonY, 104, kFindDlgFindPrevious);
      if (state->replaceMode)
      {
        CreateDialogButton(hwnd, L"Replace", kUiMargin + 210, buttonY, 92, kFindDlgReplace);
        CreateDialogButton(hwnd, L"Replace All", kUiMargin + 308, buttonY, 92, kFindDlgReplaceAll);
        CreateDialogButton(hwnd, L"Close", client.right - kUiMargin - 84, buttonY, 84,
                           kFindDlgClose);
      }
      else
      {
        CreateDialogButton(hwnd, L"Close", client.right - kUiMargin - 84, buttonY, 84,
                           kFindDlgClose);
      }

      ApplyUiFontToDescendants(hwnd);
      return 0;
    }
    case WM_COMMAND:
    {
      if (!state || !state->window)
      {
        return 0;
      }

      switch (LOWORD(wParam))
      {
        case kFindDlgFindNext:
          PostMessageW(state->window->Hwnd(), WM_APP + 1, 1, reinterpret_cast<LPARAM>(hwnd));
          return 0;
        case kFindDlgFindPrevious:
          PostMessageW(state->window->Hwnd(), WM_APP + 1, 0, reinterpret_cast<LPARAM>(hwnd));
          return 0;
        case kFindDlgReplace:
          PostMessageW(state->window->Hwnd(), WM_APP + 2, 0, reinterpret_cast<LPARAM>(hwnd));
          return 0;
        case kFindDlgReplaceAll:
          PostMessageW(state->window->Hwnd(), WM_APP + 3, 0, reinterpret_cast<LPARAM>(hwnd));
          return 0;
        case kFindDlgClose:
          DestroyWindow(hwnd);
          return 0;
        default:
          break;
      }
      break;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK GoToDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  GoToDialogState* state = reinterpret_cast<GoToDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg)
  {
    case WM_CREATE:
    {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
      state = reinterpret_cast<GoToDialogState*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

      RECT client = {};
      GetClientRect(hwnd, &client);

      wchar_t label[64] = {};
      swprintf_s(label, L"Line number (1-%d):", state ? state->maxLine : 1);
      CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE, kUiMargin, kUiMargin + 2,
                      client.right - 2 * kUiMargin, 20, hwnd, nullptr, GetModuleHandleW(nullptr),
                      nullptr);
      CreateLabeledEdit(hwnd, kUiMargin, kUiMargin + 24, client.right - 2 * kUiMargin,
                        kGoToDlgLineEdit);

      const int buttonY = client.bottom - kUiMargin - kUiButtonHeight;
      CreateDialogButton(hwnd, L"Go", client.right - kUiMargin - 176, buttonY, 80, kGoToDlgOk);
      CreateDialogButton(hwnd, L"Cancel", client.right - kUiMargin - 84, buttonY, 84,
                         kGoToDlgCancel);

      ApplyUiFontToDescendants(hwnd);
      return 0;
    }
    case WM_COMMAND:
    {
      if (!state || !state->window)
      {
        return 0;
      }

      switch (LOWORD(wParam))
      {
        case kGoToDlgOk:
          PostMessageW(state->window->Hwnd(), WM_APP + 4, 0, reinterpret_cast<LPARAM>(hwnd));
          return 0;
        case kGoToDlgCancel:
          DestroyWindow(hwnd);
          return 0;
        default:
          break;
      }
      break;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK RunDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  RunDialogState* state = reinterpret_cast<RunDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg)
  {
    case WM_CREATE:
    {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
      state = reinterpret_cast<RunDialogState*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

      RECT client = {};
      GetClientRect(hwnd, &client);

      CreateWindowExW(0, L"STATIC", L"Command (cmd /c):", WS_CHILD | WS_VISIBLE, kUiMargin,
                      kUiMargin + 2, client.right - 2 * kUiMargin, 20, hwnd, nullptr,
                      GetModuleHandleW(nullptr), nullptr);
      CreateLabeledEdit(hwnd, kUiMargin, kUiMargin + 24, client.right - 2 * kUiMargin,
                        kRunDlgCommandEdit);

      const int buttonY = client.bottom - kUiMargin - kUiButtonHeight;
      CreateDialogButton(hwnd, L"Run", client.right - kUiMargin - 176, buttonY, 80, kRunDlgOk);
      CreateDialogButton(hwnd, L"Cancel", client.right - kUiMargin - 84, buttonY, 84,
                         kRunDlgCancel);

      ApplyUiFontToDescendants(hwnd);
      return 0;
    }
    case WM_COMMAND:
    {
      if (!state || !state->window)
      {
        return 0;
      }

      switch (LOWORD(wParam))
      {
        case kRunDlgOk:
          PostMessageW(state->window->Hwnd(), WM_APP + 5, 0, reinterpret_cast<LPARAM>(hwnd));
          return 0;
        case kRunDlgCancel:
          DestroyWindow(hwnd);
          return 0;
        default:
          break;
      }
      break;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK CreditsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_CREATE:
    {
      RECT client = {};
      GetClientRect(hwnd, &client);

      const int buttonWidth = 84;
      const int editHeight =
          client.bottom - kUiMargin - kUiGap - kUiButtonHeight - kUiMargin;
      const int editWidth = client.right - 2 * kUiMargin;

      const std::wstring creditsBody = BuildCreditsText();
      CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"EDIT",
          creditsBody.c_str(),
          WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
          kUiMargin,
          kUiMargin,
          editWidth,
          editHeight > 0 ? editHeight : 0,
          hwnd,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCreditsDlgText)),
          GetModuleHandleW(nullptr),
          nullptr);

      CreateDialogButton(hwnd, L"Close", client.right - kUiMargin - buttonWidth,
                         client.bottom - kUiMargin - kUiButtonHeight, buttonWidth,
                         kCreditsDlgClose);

      ApplyUiFontToDescendants(hwnd);
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wParam) == kCreditsDlgClose)
      {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterDialogClass(const wchar_t* className, WNDPROC procedure)
{
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = procedure;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = className;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void CenterWindowOnParent(HWND dialog, HWND parent, int width, int height)
{
  RECT parentRect = {};
  GetWindowRect(parent, &parentRect);
  const int x = parentRect.left + (parentRect.right - parentRect.left - width) / 2;
  const int y = parentRect.top + (parentRect.bottom - parentRect.top - height) / 2;
  SetWindowPos(dialog, nullptr, x, y, width, height, SWP_NOZORDER | SWP_SHOWWINDOW);
}
}

MainWindow::MainWindow()
  : hwnd_(nullptr),
    projectPaneHeader_(nullptr),
    projectPaneDivider_(nullptr),
    projectTree_(nullptr),
    outputPane_(nullptr),
    statusBar_(nullptr),
    accelerators_(nullptr),
    activeDocumentIndex_(-1),
    untitledCounter_(0),
    recentFilesMenu_(nullptr),
    tabContextMenu_(nullptr),
    contextMenuTabIndex_(-1),
    projectPaneWidth_(240),
    projectPaneHeaderHeight_(0),
    tabStripHeight_(kDefaultTabStripHeight),
    outputPaneHeight_(180),
    showProjectPane_(true),
    showOutputPane_(true),
    wordWrapEnabled_(false),
    codeFoldingEnabled_(false)
{
}

bool MainWindow::Create(HINSTANCE instance, int nCmdShow)
{
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MainWindow::WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = kMainWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

  if (!RegisterClassExW(&wc))
  {
    return false;
  }

  hwnd_ = CreateWindowExW(
      0,
      kMainWindowClassName,
      L"WinCpp",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      1200,
      800,
      nullptr,
      nullptr,
      instance,
      this);

  if (!hwnd_)
  {
    return false;
  }

  ShowWindow(hwnd_, nCmdShow);
  UpdateWindow(hwnd_);
  return true;
}

bool MainWindow::TranslateAcceleratorMessage(MSG* msg)
{
  if (!accelerators_ || !hwnd_)
  {
    return false;
  }

  return TranslateAcceleratorW(hwnd_, accelerators_, msg) != 0;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  MainWindow* self = nullptr;

  if (msg == WM_NCCREATE)
  {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<MainWindow*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }
  else
  {
    self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self)
  {
    return self->HandleMessage(msg, wParam, lParam);
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_CREATE:
    {
      INITCOMMONCONTROLSEX icc = {};
      icc.dwSize = sizeof(icc);
      icc.dwICC = ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES;
      InitCommonControlsEx(&icc);

      recentFiles_.Load();
      editorSettings_.Load();
      wordWrapEnabled_ = editorSettings_.wordWrap;
      CreateMenus();
      CreateAccelerators();
      CreateChildPanes();
      EnsureInitialDocument();
      ApplyEditorSettingsToAllGroups();
      LoadSession();
      UpdateRecentFilesMenu();
      LayoutChildren();
      return 0;
    }
    case WM_SIZE:
    {
      if (wParam != SIZE_MINIMIZED)
      {
        LayoutChildren();
      }
      return 0;
    }
    case WM_GETMINMAXINFO:
    {
      auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
      info->ptMinTrackSize.x = kMinWindowWidth;
      info->ptMinTrackSize.y = kMinWindowHeight;
      return 0;
    }
    case WM_DPICHANGED:
    {
      LayoutChildren();
      InvalidateTabs();
      return 0;
    }
    case WM_SYSCOLORCHANGE:
      SyncChromeColors();
      break;
    case WM_CTLCOLOREDIT:
      if (outputPane_ && reinterpret_cast<HWND>(lParam) == outputPane_ && chromeBackgroundBrush_)
      {
        const TabBar::ChromeColors chrome = TabBar::GetChromeColors(hwnd_);
        SetBkColor(reinterpret_cast<HDC>(wParam), chrome.stripBackground);
        SetTextColor(reinterpret_cast<HDC>(wParam), chrome.inactiveForeground);
        return reinterpret_cast<LRESULT>(chromeBackgroundBrush_);
      }
      break;
    case WM_CTLCOLORSTATIC:
    {
      const HWND control = reinterpret_cast<HWND>(lParam);
      const TabBar::ChromeColors chrome = TabBar::GetChromeColors(hwnd_);
      HDC hdc = reinterpret_cast<HDC>(wParam);
      if (projectPaneDivider_ && control == projectPaneDivider_ && chromeSidebarBorderBrush_)
      {
        SetBkColor(hdc, chrome.sidebarBorder);
        return reinterpret_cast<LRESULT>(chromeSidebarBorderBrush_);
      }
      break;
    }
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      if (editorWorkspace_.ForwardWheelToTabBar({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, msg,
                                                wParam, lParam))
      {
        return 0;
      }
      break;
    case WM_TABBAR_SELECT:
      editorWorkspace_.OnTabSelect(static_cast<int>(lParam), static_cast<int>(wParam));
      return 0;
    case WM_TABBAR_CLOSE:
      OnWorkspaceTabClose(static_cast<int>(lParam), static_cast<int>(wParam));
      return 0;
    case WM_TABBAR_CONTEXTMENU:
      ShowTabContextMenu(DocumentIndexForGroupTab(static_cast<int>(lParam),
                                                  static_cast<int>(wParam)));
      return 0;
    case WM_TABBAR_REORDER:
      editorWorkspace_.OnTabReorder(static_cast<int>(HIWORD(lParam)), static_cast<int>(wParam),
                                   static_cast<int>(LOWORD(lParam)));
      return 0;
    case WM_TABBAR_DRAG_MOVE:
    {
      const POINT pt = UnpackScreenPoint(lParam);
      editorWorkspace_.OnTabDragMove(static_cast<int>(HIWORD(wParam)), static_cast<int>(LOWORD(wParam)),
                                     pt);
      return 0;
    }
    case WM_TABBAR_DRAG_END:
    {
      const POINT pt = UnpackScreenPoint(lParam);
      editorWorkspace_.OnTabDragEnd(static_cast<int>(HIWORD(wParam)), static_cast<int>(LOWORD(wParam)),
                                    pt);
      return 0;
    }
    case WM_TABBAR_DRAG_CANCEL:
      editorWorkspace_.EndDragTracking();
      return 0;
    case WM_WORKSPACE_TAB_SELECT:
      OnWorkspaceTabSelect(static_cast<int>(lParam), static_cast<int>(wParam));
      return 0;
    case WM_WORKSPACE_TAB_CLOSE:
      OnWorkspaceTabClose(static_cast<int>(lParam), static_cast<int>(wParam));
      return 0;
    case WM_WORKSPACE_REQUEST_SYNC:
      SyncTabBar();
      return 0;
    case WM_COMMAND:
    {
      if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == kCtrlProjectFilter)
      {
        projectFilterText_ = GetWindowTextString(projectFilterEdit_);
        if (!projectRootPath_.empty())
        {
          PopulateProjectTree(projectRootPath_);
        }
        return 0;
      }

      const UINT command = LOWORD(wParam);
      switch (command)
      {
        case kCmdFileNew:
          NewFile();
          return 0;
        case kCmdFileOpen:
          OpenFile();
          return 0;
        case kCmdFileOpenFolder:
          OpenFolder();
          return 0;
        case kCmdFileSave:
          SaveFile();
          return 0;
        case kCmdFileSaveAs:
          SaveFileAs();
          return 0;
        case kCmdFileSaveAll:
          SaveAllFiles();
          return 0;
        case kCmdFileCloseAll:
          CloseAllDocuments();
          return 0;
        case kCmdFileOpenRecentClear:
          ClearRecentFiles();
          return 0;
        case kCmdFileExit:
          DestroyWindow(hwnd_);
          return 0;
        case kCmdEditUndo:
          SendMessage(editorWorkspace_.ActiveEditor().Hwnd(), SCI_UNDO, 0, 0);
          return 0;
        case kCmdEditRedo:
          SendMessage(editorWorkspace_.ActiveEditor().Hwnd(), SCI_REDO, 0, 0);
          return 0;
        case kCmdEditCut:
          SendMessage(editorWorkspace_.ActiveEditor().Hwnd(), SCI_CUT, 0, 0);
          return 0;
        case kCmdEditCopy:
          SendMessage(editorWorkspace_.ActiveEditor().Hwnd(), SCI_COPY, 0, 0);
          return 0;
        case kCmdEditPaste:
          SendMessage(editorWorkspace_.ActiveEditor().Hwnd(), SCI_PASTE, 0, 0);
          return 0;
        case kCmdViewSplitRight:
          SplitEditor(EditorSplitDirection::Right);
          return 0;
        case kCmdViewSplitDown:
          SplitEditor(EditorSplitDirection::Down);
          return 0;
        case kCmdViewCloseEditorGroup:
          if (editorWorkspace_.HasMultipleGroups())
          {
            editorWorkspace_.CloseActiveGroup();
            SyncTabBar();
            LayoutChildren();
          }
          return 0;
        case kCmdEditFind:
          ShowFindReplaceDialog(false);
          return 0;
        case kCmdEditReplace:
          ShowFindReplaceDialog(true);
          return 0;
        case kCmdEditGoToLine:
          ShowGoToLineDialog();
          return 0;
        case kCmdEditFindPrevious:
          if (findDialog_)
          {
            FindFromDialog(findDialog_, false);
          }
          else
          {
            ShowFindReplaceDialog(false);
          }
          return 0;
        case kCmdEditDuplicateLine:
          editorWorkspace_.ActiveEditor().DuplicateLine();
          return 0;
        case kCmdEditDeleteLine:
          editorWorkspace_.ActiveEditor().DeleteLine();
          return 0;
        case kCmdEditMoveLineUp:
          editorWorkspace_.ActiveEditor().MoveLineUp();
          return 0;
        case kCmdEditMoveLineDown:
          editorWorkspace_.ActiveEditor().MoveLineDown();
          return 0;
        case kCmdEditTrimTrailingWhitespace:
          editorWorkspace_.ActiveEditor().TrimTrailingWhitespace();
          return 0;
        case kCmdEditConvertTabsToSpaces:
          editorWorkspace_.ActiveEditor().ConvertSelectionTabsToSpaces();
          return 0;
        case kCmdEditGotoMatchingBrace:
          editorWorkspace_.ActiveEditor().GotoMatchingBrace();
          return 0;
        case kCmdEditCommandPalette:
          ShowCommandPalette();
          return 0;
        case kCmdViewWordWrap:
          ToggleWordWrap();
          return 0;
        case kCmdViewZoomIn:
          ZoomIn();
          return 0;
        case kCmdViewZoomOut:
          ZoomOut();
          return 0;
        case kCmdViewZoomReset:
          ZoomReset();
          return 0;
        case kCmdViewCodeFolding:
          ToggleCodeFolding();
          return 0;
        case kCmdViewRunCommand:
          ShowRunCommandDialog();
          return 0;
        case kCmdTabPin:
          PinActiveTab();
          return 0;
        case kCmdHelpAbout:
          MessageBoxW(hwnd_, BuildAboutText().c_str(), L"About WinCpp", MB_OK | MB_ICONINFORMATION);
          return 0;
        case kCmdHelpCredits:
          ShowCreditsDialog();
          return 0;
        case kCmdViewProjectPane:
          ToggleProjectPane();
          return 0;
        case kCmdViewOutputPane:
          ToggleOutputPane();
          return 0;
        case kCmdTabClose:
          if (activeDocumentIndex_ >= 0)
          {
            CloseDocumentAt(activeDocumentIndex_);
          }
          return 0;
        case kCmdTabCloseOthers:
          if (contextMenuTabIndex_ >= 0)
          {
            CloseOtherDocuments(contextMenuTabIndex_);
          }
          return 0;
        case kCmdTabCloseAll:
          CloseAllDocuments();
          return 0;
        default:
          if (command >= kCmdFileOpenRecentBase && command <= kCmdFileOpenRecentMax)
          {
            const size_t index = command - kCmdFileOpenRecentBase;
            const std::vector<std::wstring>& items = recentFiles_.Items();
            if (index < items.size())
            {
              OpenFileAtPath(items[index], false);
            }
            return 0;
          }
          break;
      }
      break;
    }
    case WM_NOTIFY:
    {
      const auto* header = reinterpret_cast<NMHDR*>(lParam);
      if (header->hwndFrom == projectTree_ && header->code == NM_DBLCLK)
      {
        OnProjectTreeDoubleClick();
        return 0;
      }

      if (header->hwndFrom == editorWorkspace_.ActiveEditor().Hwnd())
      {
        const auto* notification = reinterpret_cast<SCNotification*>(lParam);
        if (notification->nmhdr.code == SCN_UPDATEUI)
        {
          UpdateStatusBar();
        }
        else if (notification->nmhdr.code == SCN_CHARADDED)
        {
          editorWorkspace_.ActiveEditor().OnEditorNotify(*notification);
        }
        else if (notification->nmhdr.code == SCN_MODIFIED &&
                 (notification->modificationType & SC_MOD_INSERTTEXT ||
                  notification->modificationType & SC_MOD_DELETETEXT))
        {
          if (activeDocumentIndex_ >= 0 &&
              activeDocumentIndex_ < static_cast<int>(documents_.size()))
          {
            documents_[activeDocumentIndex_].modified =
                editorWorkspace_.ActiveEditor().IsModified();
          }
          UpdateStatusBar();
          UpdateActiveTabTitle();
        }
      }
      break;
    }
    case WM_APP + 1:
      FindFromDialog(reinterpret_cast<HWND>(lParam), wParam != 0);
      return 0;
    case WM_APP + 2:
    case WM_APP + 3:
    {
      HWND dialog = reinterpret_cast<HWND>(lParam);
      const HWND findEdit = GetDlgItem(dialog, kFindDlgFindEdit);
      const HWND replaceEdit = GetDlgItem(dialog, kFindDlgReplaceEdit);
      const bool matchCase = IsDlgButtonChecked(dialog, kFindDlgMatchCase) == BST_CHECKED;
      const bool wholeWord = IsDlgButtonChecked(dialog, kFindDlgWholeWord) == BST_CHECKED;
      const bool regex = IsDlgButtonChecked(dialog, kFindDlgRegex) == BST_CHECKED;
      const std::wstring findText = GetWindowTextString(findEdit);
      const std::wstring replaceText = replaceEdit ? GetWindowTextString(replaceEdit) : L"";

      if (findText.empty())
      {
        MessageBoxW(hwnd_, L"Enter text to find.", L"Find", MB_OK | MB_ICONINFORMATION);
        return 0;
      }

      if (msg == WM_APP + 2)
      {
        editorWorkspace_.ActiveEditor().ReplaceSelection(findText, replaceText, matchCase, wholeWord,
                                                         regex);
      }
      else if (msg == WM_APP + 3)
      {
        const int count = editorWorkspace_.ActiveEditor().ReplaceAll(findText, replaceText, matchCase,
                                                                     wholeWord, regex);
        wchar_t message[64] = {};
        swprintf_s(message, L"Replaced %d occurrence(s).", count);
        MessageBoxW(hwnd_, message, L"Replace All", MB_OK | MB_ICONINFORMATION);
      }
      return 0;
    }
    case WM_APP + 4:
    {
      HWND dialog = reinterpret_cast<HWND>(lParam);
      const std::wstring text = GetWindowTextString(GetDlgItem(dialog, kGoToDlgLineEdit));
      const int line = _wtoi(text.c_str());
      if (line <= 0)
      {
        MessageBoxW(hwnd_, L"Enter a valid line number.", L"Go To Line", MB_OK | MB_ICONWARNING);
        return 0;
      }

      editorWorkspace_.ActiveEditor().GoToLine(line);
      DestroyWindow(dialog);
      UpdateStatusBar();
      return 0;
    }
    case WM_APP + 5:
    {
      HWND dialog = reinterpret_cast<HWND>(lParam);
      const std::wstring command = GetWindowTextString(GetDlgItem(dialog, kRunDlgCommandEdit));
      DestroyWindow(dialog);
      if (command.empty())
      {
        MessageBoxW(hwnd_, L"Enter a command to run.", L"Run Command", MB_OK | MB_ICONINFORMATION);
        return 0;
      }

      g_lastRunCommand = command;
      const std::wstring commandLine = L"cmd /c " + command;
      std::string output;
      int exitCode = 0;
      if (ProcessOutput::RunCaptureStdout(commandLine, &output, &exitCode))
      {
        if (!output.empty() && output.back() != '\n')
        {
          output.push_back('\n');
        }
        wchar_t footer[64] = {};
        swprintf_s(footer, L"\r\n[exit code %d]", exitCode);
        output += WideToUtf8(footer);
        SetWindowTextA(outputPane_, output.c_str());
        showOutputPane_ = true;
        CheckMenuItem(GetMenu(hwnd_), kCmdViewOutputPane, MF_BYCOMMAND | MF_CHECKED);
        LayoutChildren();
      }
      else
      {
        MessageBoxW(hwnd_, L"Failed to run command.", L"Run Command", MB_OK | MB_ICONERROR);
      }
      return 0;
    }
    case WM_DESTROY:
      SaveSession();
      editorSettings_.wordWrap = wordWrapEnabled_;
      editorSettings_.zoom = editorWorkspace_.ActiveEditor().GetZoom();
      editorSettings_.Save();
      if (accelerators_)
      {
        DestroyAcceleratorTable(accelerators_);
        accelerators_ = nullptr;
      }
      ReleaseUiFont();
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

bool MainWindow::IsPointOverTabBar(POINT screenPoint) const
{
  return editorWorkspace_.IsPointOverAnyTabBar(screenPoint);
}

LRESULT CALLBACK MainWindow::EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                UINT_PTR, DWORD_PTR refData)
{
  auto* self = reinterpret_cast<MainWindow*>(refData);
  if (!self)
  {
    return DefSubclassProc(hwnd, msg, wParam, lParam);
  }

  if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL)
  {
    if (self->editorWorkspace_.ForwardWheelToTabBar({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                                                    msg, wParam, lParam))
    {
      return 0;
    }
  }

  if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONUP)
  {
    HWND capture = GetCapture();
    if (capture && capture != hwnd && self->editorWorkspace_.IsTabBarHwnd(capture))
    {
      POINT clientPt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      MapWindowPoints(hwnd, capture, &clientPt, 1);
      SendMessageW(capture, msg, wParam, MAKELPARAM(clientPt.x, clientPt.y));
      return 0;
    }
  }

  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::ProjectPaneHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                           LPARAM lParam, UINT_PTR,
                                                           DWORD_PTR refData)
{
  auto* self = reinterpret_cast<MainWindow*>(refData);
  if (!self || !self->hwnd_)
  {
    return DefSubclassProc(hwnd, msg, wParam, lParam);
  }

  if (msg == WM_PAINT)
  {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client = {};
    GetClientRect(hwnd, &client);

    const TabBar::ChromeColors chrome = TabBar::GetChromeColors(self->hwnd_);
    HBRUSH backBrush = CreateSolidBrush(chrome.sidebarHeaderBackground);
    FillRect(hdc, &client, backBrush);
    DeleteObject(backBrush);

    const int viewBottom =
        client.bottom > kTabBottomBorder ? client.bottom - kTabBottomBorder : client.bottom;

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
    if (!font)
    {
      font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, font));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, chrome.sidebarHeaderForeground);

    const UINT dpi = GetDpiForWindow(hwnd);
    RECT textRect = client;
    textRect.left += MulDiv(10, static_cast<int>(dpi), 96);
    textRect.bottom = viewBottom;
    DrawTextW(hdc, L"EXPLORER", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (viewBottom < client.bottom)
    {
      HPEN borderPen = CreatePen(PS_SOLID, 1, chrome.bottomBorder);
      HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, borderPen));
      MoveToEx(hdc, 0, client.bottom - 1, nullptr);
      LineTo(hdc, client.right, client.bottom - 1);
      SelectObject(hdc, oldPen);
      DeleteObject(borderPen);
    }

    SelectObject(hdc, oldFont);
    EndPaint(hwnd, &ps);
    return 0;
  }

  if (msg == WM_ERASEBKGND)
  {
    return 1;
  }

  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void MainWindow::CreateMenus()
{
  HMENU menu = CreateMenu();
  HMENU fileMenu = CreateMenu();
  recentFilesMenu_ = CreateMenu();
  HMENU editMenu = CreateMenu();
  HMENU viewMenu = CreateMenu();
  HMENU helpMenu = CreateMenu();

  AppendMenuW(fileMenu, MF_STRING, kCmdFileNew, L"&New\tCtrl+N");
  AppendMenuW(fileMenu, MF_STRING, kCmdFileOpen, L"&Open...\tCtrl+O");
  AppendMenuW(fileMenu, MF_STRING, kCmdFileOpenFolder, L"Open Fo&lder...");
  AppendMenuW(fileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(recentFilesMenu_), L"Open &Recent");
  AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(fileMenu, MF_STRING, kCmdFileSave, L"&Save\tCtrl+S");
  AppendMenuW(fileMenu, MF_STRING, kCmdFileSaveAs, L"Save &As...");
  AppendMenuW(fileMenu, MF_STRING, kCmdFileSaveAll, L"Save A&ll");
  AppendMenuW(fileMenu, MF_STRING, kCmdFileCloseAll, L"Close A&ll");
  AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(fileMenu, MF_STRING, kCmdFileExit, L"E&xit");

  AppendMenuW(editMenu, MF_STRING, kCmdEditUndo, L"&Undo\tCtrl+Z");
  AppendMenuW(editMenu, MF_STRING, kCmdEditRedo, L"&Redo\tCtrl+Y");
  AppendMenuW(editMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(editMenu, MF_STRING, kCmdEditCut, L"Cu&t\tCtrl+X");
  AppendMenuW(editMenu, MF_STRING, kCmdEditCopy, L"&Copy\tCtrl+C");
  AppendMenuW(editMenu, MF_STRING, kCmdEditPaste, L"&Paste\tCtrl+V");
  AppendMenuW(editMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(editMenu, MF_STRING, kCmdEditFind, L"&Find...\tCtrl+F");
  AppendMenuW(editMenu, MF_STRING, kCmdEditReplace, L"&Replace...\tCtrl+H");
  AppendMenuW(editMenu, MF_STRING, kCmdEditFindPrevious, L"Find &Previous\tShift+F3");
  AppendMenuW(editMenu, MF_STRING, kCmdEditGoToLine, L"&Go To Line...\tCtrl+G");
  AppendMenuW(editMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(editMenu, MF_STRING, kCmdEditDuplicateLine, L"&Duplicate Line\tCtrl+D");
  AppendMenuW(editMenu, MF_STRING, kCmdEditDeleteLine, L"De&lete Line");
  AppendMenuW(editMenu, MF_STRING, kCmdEditMoveLineUp, L"Move Line &Up");
  AppendMenuW(editMenu, MF_STRING, kCmdEditMoveLineDown, L"Move Line &Down");
  AppendMenuW(editMenu, MF_STRING, kCmdEditTrimTrailingWhitespace, L"Trim Trailing &Whitespace");
  AppendMenuW(editMenu, MF_STRING, kCmdEditConvertTabsToSpaces, L"Convert Tabs to Spaces");
  AppendMenuW(editMenu, MF_STRING, kCmdEditGotoMatchingBrace, L"Go to Matching &Brace");
  AppendMenuW(editMenu, MF_STRING, kCmdEditCommandPalette, L"&Command Palette\tCtrl+Shift+P");

  AppendMenuW(viewMenu, MF_STRING | MF_CHECKED, kCmdViewProjectPane, L"&Project Pane");
  AppendMenuW(viewMenu, MF_STRING | MF_CHECKED, kCmdViewOutputPane, L"&Output Pane");
  AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(viewMenu, MF_STRING, kCmdViewWordWrap, L"&Word Wrap");
  AppendMenuW(viewMenu, MF_STRING, kCmdViewCodeFolding, L"Code &Folding");
  AppendMenuW(viewMenu, MF_STRING, kCmdViewZoomIn, L"Zoom &In");
  AppendMenuW(viewMenu, MF_STRING, kCmdViewZoomOut, L"Zoom &Out");
  AppendMenuW(viewMenu, MF_STRING, kCmdViewZoomReset, L"Zoom &Reset");
  AppendMenuW(viewMenu, MF_STRING, kCmdViewRunCommand, L"&Run Command...");
  AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(viewMenu, MF_STRING, kCmdViewSplitRight, L"Split Editor &Right");
  AppendMenuW(viewMenu, MF_STRING, kCmdViewSplitDown, L"Split Editor &Down");
  AppendMenuW(viewMenu, MF_STRING, kCmdViewCloseEditorGroup, L"Close Editor &Group");

  AppendMenuW(helpMenu, MF_STRING, kCmdHelpAbout, L"&About");
  AppendMenuW(helpMenu, MF_STRING, kCmdHelpCredits, L"C&redits");

  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(editMenu), L"&Edit");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"&View");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), L"&Help");

  SetMenu(hwnd_, menu);
}

void MainWindow::CreateAccelerators()
{
  ACCEL table[] = {
      {FVIRTKEY | FCONTROL, 'N', kCmdFileNew},
      {FVIRTKEY | FCONTROL, 'O', kCmdFileOpen},
      {FVIRTKEY | FCONTROL, 'S', kCmdFileSave},
      {FVIRTKEY | FCONTROL, 'F', kCmdEditFind},
      {FVIRTKEY | FCONTROL, 'H', kCmdEditReplace},
      {FVIRTKEY | FCONTROL, 'G', kCmdEditGoToLine},
      {FVIRTKEY | FCONTROL, 'D', kCmdEditDuplicateLine},
      {FVIRTKEY, VK_F3, kCmdEditFind},
      {FVIRTKEY | FSHIFT, VK_F3, kCmdEditFindPrevious},
      {FVIRTKEY | FCONTROL | FSHIFT, 'P', kCmdEditCommandPalette},
      {FVIRTKEY | FCONTROL, 'W', kCmdTabClose},
      {FVIRTKEY | FCONTROL, VK_OEM_5, kCmdViewSplitRight},
  };

  accelerators_ = CreateAcceleratorTableW(table, static_cast<int>(std::size(table)));
}

void MainWindow::CreateChildPanes()
{
  projectPaneHeader_ = CreateWindowExW(
      0,
      L"STATIC",
      L"",
      WS_CHILD | WS_VISIBLE,
      0,
      0,
      0,
      0,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      nullptr);
  SetWindowSubclass(projectPaneHeader_, ProjectPaneHeaderSubclassProc, kProjectHeaderSubclassId,
                    reinterpret_cast<DWORD_PTR>(this));

  projectPaneDivider_ = CreateWindowExW(
      0,
      L"STATIC",
      L"",
      WS_CHILD | WS_VISIBLE,
      0,
      0,
      0,
      0,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      nullptr);

  projectTree_ = CreateWindowExW(
      0,
      WC_TREEVIEWW,
      L"",
      WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
      0,
      0,
      0,
      0,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      nullptr);

  tabContextMenu_ = CreatePopupMenu();
  projectFilterEdit_ = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtrlProjectFilter)), GetModuleHandleW(nullptr),
      nullptr);

  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabClose, L"Close");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabCloseOthers, L"Close Others");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabCloseAll, L"Close All");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabPin, L"Pin Tab");
  AppendMenuW(tabContextMenu_, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdViewSplitRight, L"Split Right");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdViewSplitDown, L"Split Down");

  outputPane_ = CreateWindowExW(
      0,
      L"EDIT",
      L"Output pane (log goes here)\r\n",
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
      0,
      0,
      0,
      0,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      nullptr);

  editorWorkspace_.Create(hwnd_, GetModuleHandleW(nullptr), hwnd_,
                          reinterpret_cast<DWORD_PTR>(this));
  const std::filesystem::path syntaxDir = GetExecutableDir() / L"syntax";
  editorWorkspace_.LoadSyntaxDirectory(syntaxDir.wstring());

  statusBar_ = CreateWindowExW(
      0,
      STATUSCLASSNAMEW,
      L"Ready",
      WS_CHILD | WS_VISIBLE,
      0,
      0,
      0,
      0,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      nullptr);

  int parts[] = {-kStatusCaretWidth, -1};
  SendMessageW(statusBar_, SB_SETPARTS, 2, reinterpret_cast<LPARAM>(parts));
  int statusBorders[] = {0, 0, 0};
  SendMessageW(statusBar_, SB_SETBORDERS, 0, reinterpret_cast<LPARAM>(statusBorders));

  ApplyUiFont(projectPaneHeader_);
  ApplyUiFont(projectFilterEdit_);
  ApplyUiFont(projectTree_);
  ApplyUiFont(outputPane_);
  ApplyUiFont(statusBar_);

  SyncChromeColors();
  UpdateStatusBar();
}

void MainWindow::SyncChromeColors()
{
  const TabBar::ChromeColors chrome = TabBar::GetChromeColors(hwnd_);

  if (chromeBackgroundBrush_)
  {
    DeleteObject(chromeBackgroundBrush_);
    chromeBackgroundBrush_ = nullptr;
  }
  chromeBackgroundBrush_ = CreateSolidBrush(chrome.stripBackground);

  if (chromeSidebarHeaderBrush_)
  {
    DeleteObject(chromeSidebarHeaderBrush_);
    chromeSidebarHeaderBrush_ = nullptr;
  }
  chromeSidebarHeaderBrush_ = CreateSolidBrush(chrome.sidebarHeaderBackground);

  if (chromeSidebarBorderBrush_)
  {
    DeleteObject(chromeSidebarBorderBrush_);
    chromeSidebarBorderBrush_ = nullptr;
  }
  chromeSidebarBorderBrush_ = CreateSolidBrush(chrome.sidebarBorder);

  if (projectPaneHeader_)
  {
    InvalidateRect(projectPaneHeader_, nullptr, FALSE);
  }
  if (projectPaneDivider_)
  {
    InvalidateRect(projectPaneDivider_, nullptr, TRUE);
  }

  if (projectTree_)
  {
    TreeView_SetBkColor(projectTree_, chrome.stripBackground);
    TreeView_SetTextColor(projectTree_, chrome.inactiveForeground);
  }

  if (outputPane_)
  {
    InvalidateRect(outputPane_, nullptr, TRUE);
  }
}

void MainWindow::EnsureInitialDocument()
{
  if (!documents_.empty())
  {
    return;
  }

  EditorView& editor = editorWorkspace_.PrimaryEditor();
  void* document = editor.CreateDocument();
  editor.SetDocument(document);
  editor.Clear();

  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.tabTitle = MakeUntitledName();
  entry.displayTitle = entry.tabTitle;
  documents_.push_back(entry);
  activeDocumentIndex_ = 0;
  editorWorkspace_.AttachDocumentToActiveGroup(0, documents_);
  SyncTabBar();
}

std::wstring MainWindow::MakeUntitledName()
{
  ++untitledCounter_;
  return L"Untitled-" + std::to_wstring(untitledCounter_);
}

std::wstring MainWindow::TabLabelForPath(const std::wstring& path) const
{
  return DocumentNaming::TabLabelForPath(path);
}

void MainWindow::SaveCurrentDocumentState()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument& doc = documents_[activeDocumentIndex_];
  EditorView& editor = editorWorkspace_.ActiveEditor();
  if (doc.scintillaDoc)
  {
    editor.SetDocumentIfNeeded(doc.scintillaDoc);
    doc.modified = editor.IsModified();
  }
  else
  {
    void* const shown = editor.GetDocumentPointer();
    if (shown)
    {
      doc.scintillaDoc = shown;
      doc.modified = editor.IsModified();
    }
  }
}

void MainWindow::AttachEditorToDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  editorWorkspace_.AttachDocumentToActiveGroup(index, documents_);
  activeDocumentIndex_ = index;
}

void MainWindow::SwitchToDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  SaveCurrentDocumentState();

  if (index == activeDocumentIndex_ &&
      editorWorkspace_.ActiveEditor().GetDocumentPointer() == documents_[index].scintillaDoc)
  {
    if (!documents_[index].path.empty())
    {
      editorWorkspace_.ActiveEditor().ApplySyntaxForPath(documents_[index].path);
    }
    return;
  }

  editorWorkspace_.OpenDocumentInActiveGroup(index, documents_);
  activeDocumentIndex_ = index;
  UpdateStatusBar();
  SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
}

void MainWindow::AddDocumentTab(const std::wstring& path, void* document)
{
  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.path = path;
  entry.tabTitle = path.empty() ? MakeUntitledName() : TabLabelForPath(path);
  entry.displayTitle = entry.tabTitle;

  documents_.push_back(entry);
  const int index = static_cast<int>(documents_.size()) - 1;
  SwitchToDocument(index);
  SyncTabBar();
}

int MainWindow::FindDocumentByPath(const std::wstring& path) const
{
  for (size_t i = 0; i < documents_.size(); ++i)
  {
    if (!documents_[i].path.empty() && _wcsicmp(documents_[i].path.c_str(), path.c_str()) == 0)
    {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void MainWindow::UpdateActiveTabTitle()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  SyncDocumentDisplayTitle(activeDocumentIndex_);
  const EditorDocument& doc = documents_[activeDocumentIndex_];
  SyncTabBar();
}

void MainWindow::NewFile()
{
  SaveCurrentDocumentState();

  EditorView& editor = editorWorkspace_.PrimaryEditor();
  void* const document = editor.CreateDocument();
  AddDocumentTab(L"", document);
  editor.Clear();
  UpdateStatusBar();
}

void MainWindow::OpenFile()
{
  const std::wstring path = ShowOpenFileDialog(hwnd_);
  if (!path.empty())
  {
    OpenFileAtPath(path, true);
  }
}

void MainWindow::OpenFileAtPath(const std::wstring& path, bool addToRecents)
{
  const int existing = FindDocumentByPath(path);
  if (existing >= 0)
  {
    SwitchToDocument(existing);
    return;
  }

  SaveCurrentDocumentState();

  EditorView& editor = editorWorkspace_.PrimaryEditor();
  void* const document = editor.CreateDocument();
  if (!document)
  {
    MessageBoxW(hwnd_, L"Failed to open file.\n\nEditor is not ready.", L"Open File",
                MB_OK | MB_ICONERROR);
    return;
  }

  std::string error;
  if (!editor.LoadFromFileIntoDocument(document, path, &error))
  {
    editor.ReleaseDocument(document);
    std::wstring message = L"Failed to open file.";
    const std::wstring detail = Utf8ToWide(error);
    if (!detail.empty())
    {
      message += L"\n\n" + detail;
    }
    MessageBoxW(hwnd_, message.c_str(), L"Open File", MB_OK | MB_ICONERROR);
    return;
  }

  AddDocumentTab(path, document);
  UpdateActiveTabTitle();

  if (addToRecents)
  {
    recentFiles_.Add(path);
    UpdateRecentFilesMenu();
  }

  UpdateStatusBar();
}

void MainWindow::OpenFolder()
{
  const std::wstring folder = ShowFolderDialog(hwnd_);
  if (!folder.empty())
  {
    PopulateProjectTree(folder);
  }
}

void MainWindow::SaveFile()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument& doc = documents_[activeDocumentIndex_];
  if (doc.path.empty())
  {
    SaveFileAs();
    return;
  }

  if (editorSettings_.trimTrailingWhitespaceOnSave)
  {
    editorWorkspace_.ActiveEditor().TrimTrailingWhitespace();
  }

  std::string error;
  if (!editorWorkspace_.ActiveEditor().SaveToFile(doc.path, &error))
  {
    std::wstring message = L"Failed to save file.";
    const std::wstring detail = Utf8ToWide(error);
    if (!detail.empty())
    {
      message += L"\n\n" + detail;
    }
    MessageBoxW(hwnd_, message.c_str(), L"Save File", MB_OK | MB_ICONERROR);
    return;
  }

  recentFiles_.Add(doc.path);
  UpdateRecentFilesMenu();
  doc.modified = false;
  UpdateStatusBar();
  UpdateActiveTabTitle();
}

void MainWindow::SaveFileAs()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument& doc = documents_[activeDocumentIndex_];
  const std::wstring path = ShowSaveFileDialog(hwnd_, doc.path);
  if (path.empty())
  {
    return;
  }

  std::string error;
  if (!editorWorkspace_.ActiveEditor().SaveToFile(path, &error))
  {
    std::wstring message = L"Failed to save file.";
    const std::wstring detail = Utf8ToWide(error);
    if (!detail.empty())
    {
      message += L"\n\n" + detail;
    }
    MessageBoxW(hwnd_, message.c_str(), L"Save File", MB_OK | MB_ICONERROR);
    return;
  }

  doc.path = path;
  recentFiles_.Add(path);
  UpdateRecentFilesMenu();
  doc.modified = false;
  UpdateStatusBar();
  UpdateActiveTabTitle();
}

void MainWindow::UpdateStatusBar()
{
  if (!statusBar_)
  {
    return;
  }

  StatusBarModelInput input;
  input.line = editorWorkspace_.ActiveEditor().GetCurrentLine();
  input.column = editorWorkspace_.ActiveEditor().GetCurrentColumn();
  input.selectionLength = editorWorkspace_.ActiveEditor().GetSelectionLength();
  input.modified = editorWorkspace_.ActiveEditor().IsModified();
  input.tabSize = editorSettings_.tabSize;
  input.insertSpaces = editorSettings_.tabToSpaces;
  input.languageName = editorWorkspace_.ActiveEditor().CurrentLanguageName();
  if (activeDocumentIndex_ >= 0 && activeDocumentIndex_ < static_cast<int>(documents_.size()))
  {
    input.path = documents_[activeDocumentIndex_].path;
  }

  const std::wstring status = StatusBarModel::Format(input);
  SendMessageW(statusBar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(status.c_str()));
}

void MainWindow::UpdateRecentFilesMenu()
{
  if (!recentFilesMenu_)
  {
    return;
  }

  while (GetMenuItemCount(recentFilesMenu_) > 0)
  {
    DeleteMenu(recentFilesMenu_, 0, MF_BYPOSITION);
  }

  const std::vector<std::wstring>& items = recentFiles_.Items();
  if (items.empty())
  {
    AppendMenuW(recentFilesMenu_, MF_STRING | MF_GRAYED, 0, L"(empty)");
    return;
  }

  for (size_t i = 0; i < items.size() && i < RecentFilesStore::kMaxItems; ++i)
  {
    const UINT command = kCmdFileOpenRecentBase + static_cast<UINT>(i);
    std::wstring label = std::to_wstring(i + 1) + L"  " + items[i];
    AppendMenuW(recentFilesMenu_, MF_STRING, command, label.c_str());
  }

  AppendMenuW(recentFilesMenu_, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(recentFilesMenu_, MF_STRING, kCmdFileOpenRecentClear, L"Clear &History");
}

void MainWindow::ClearRecentFiles()
{
  recentFiles_.Clear();
  UpdateRecentFilesMenu();
}

void MainWindow::ToggleWordWrap()
{
  wordWrapEnabled_ = !wordWrapEnabled_;
  editorSettings_.wordWrap = wordWrapEnabled_;
  editorWorkspace_.ApplySettingsToAllEditors(editorSettings_);
  CheckMenuItem(GetMenu(hwnd_), kCmdViewWordWrap,
                MF_BYCOMMAND | (wordWrapEnabled_ ? MF_CHECKED : MF_UNCHECKED));
  editorSettings_.Save();
}

void MainWindow::ShowFindReplaceDialog(bool replaceMode)
{
  static bool findClassRegistered = false;
  if (!findClassRegistered)
  {
    RegisterDialogClass(L"WinCppFindDialog", FindDialogProc);
    findClassRegistered = true;
  }

  static FindDialogState state;
  state.window = this;
  state.replaceMode = replaceMode;

  const DWORD style = WS_CAPTION | WS_SYSMENU | WS_POPUP;
  const int height = replaceMode ? 250 : 210;
  const int width = 420;
  HWND dialog = CreateWindowExW(
      WS_EX_DLGMODALFRAME,
      L"WinCppFindDialog",
      replaceMode ? L"Replace" : L"Find",
      style,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      width,
      height,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      &state);

  if (dialog)
  {
    findDialog_ = dialog;
    CenterDialogOnWindow(dialog, width, height);
    SetForegroundWindow(dialog);
  }
}

void MainWindow::ShowCreditsDialog()
{
  static bool creditsClassRegistered = false;
  if (!creditsClassRegistered)
  {
    RegisterDialogClass(L"WinCppCreditsDialog", CreditsDialogProc);
    creditsClassRegistered = true;
  }

  constexpr int dialogWidth = 520;
  constexpr int dialogHeight = 460;

  HWND dialog = CreateWindowExW(
      WS_EX_DLGMODALFRAME,
      L"WinCppCreditsDialog",
      L"Credits",
      WS_CAPTION | WS_SYSMENU | WS_POPUP,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      dialogWidth,
      dialogHeight,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      nullptr);

  if (dialog)
  {
    CenterDialogOnWindow(dialog, dialogWidth, dialogHeight);
    SetForegroundWindow(dialog);
  }
}

void MainWindow::ShowGoToLineDialog()
{
  static bool goToClassRegistered = false;
  if (!goToClassRegistered)
  {
    RegisterDialogClass(L"WinCppGoToDialog", GoToDialogProc);
    goToClassRegistered = true;
  }

  static GoToDialogState state;
  state.window = this;
  state.maxLine = static_cast<int>(SendMessage(editorWorkspace_.ActiveEditor().Hwnd(), SCI_GETLINECOUNT, 0, 0));

  HWND dialog = CreateWindowExW(
      WS_EX_DLGMODALFRAME,
      L"WinCppGoToDialog",
      L"Go To Line",
      WS_CAPTION | WS_SYSMENU | WS_POPUP,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      440,
      140,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      &state);

  if (dialog)
  {
    wchar_t currentLine[16] = {};
    swprintf_s(currentLine, L"%d", editorWorkspace_.ActiveEditor().GetCurrentLine());
    SetWindowTextW(GetDlgItem(dialog, kGoToDlgLineEdit), currentLine);
    CenterDialogOnWindow(dialog, 440, 140);
    SetForegroundWindow(dialog);
  }
}

HTREEITEM MainWindow::InsertTreeItem(HTREEITEM parent, const std::wstring& label, bool isDirectory)
{
  TVINSERTSTRUCTW insert = {};
  insert.hParent = parent ? parent : TVI_ROOT;
  insert.hInsertAfter = TVI_LAST;
  insert.item.mask = TVIF_TEXT;
  insert.item.pszText = const_cast<LPWSTR>(label.c_str());
  return TreeView_InsertItem(projectTree_, &insert);
}

void MainWindow::AddTreeDirectory(HTREEITEM parent, const std::filesystem::path& directory)
{
  std::vector<std::filesystem::directory_entry> entries;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
  {
    if (ec)
    {
      break;
    }
    entries.push_back(entry);
  }

  std::sort(entries.begin(), entries.end(),
            [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
              if (a.is_directory() != b.is_directory())
              {
                return a.is_directory() > b.is_directory();
              }
              return a.path().filename().wstring() < b.path().filename().wstring();
            });

  for (const auto& entry : entries)
  {
    const std::wstring name = entry.path().filename().wstring();
    if (name.empty() || name == L"." || name == L"..")
    {
      continue;
    }

    if (entry.is_directory(ec))
    {
      const HTREEITEM folderItem = InsertTreeItem(parent, name, true);
      AddTreeDirectory(folderItem, entry.path());
    }
    else if (entry.is_regular_file(ec))
    {
      if (!projectRootPath_.empty() && !projectFilterText_.empty())
      {
        std::error_code relEc;
        const std::wstring relative =
            std::filesystem::relative(entry.path(), std::filesystem::path(projectRootPath_), relEc)
                .wstring();
        if (!relEc && !TreeFilterLogic::PathMatchesFilter(relative, projectFilterText_))
        {
          continue;
        }
      }

      const HTREEITEM fileItem = InsertTreeItem(parent, name, false);
      treeItemPaths_[fileItem] = entry.path().wstring();
    }
  }
}

void MainWindow::ClearProjectTree()
{
  TreeView_DeleteAllItems(projectTree_);
  treeItemPaths_.clear();
}

void MainWindow::PopulateProjectTree(const std::wstring& rootPath)
{
  projectRootPath_ = rootPath;
  ClearProjectTree();

  const std::filesystem::path root(rootPath);
  std::wstring rootLabel = root.filename().wstring();
  if (rootLabel.empty())
  {
    rootLabel = root.wstring();
  }

  const HTREEITEM rootItem = InsertTreeItem(nullptr, rootLabel, true);
  AddTreeDirectory(rootItem, root);
  TreeView_Expand(projectTree_, rootItem, TVE_EXPAND);
}

void MainWindow::OnProjectTreeDoubleClick()
{
  const HTREEITEM selected = TreeView_GetSelection(projectTree_);
  if (!selected)
  {
    return;
  }

  const auto it = treeItemPaths_.find(selected);
  if (it != treeItemPaths_.end())
  {
    if (const auto target = PathLineParser::Parse(it->second))
    {
      OpenFileAtPath(target->path, true);
      editorWorkspace_.ActiveEditor().GoToLine(target->line);
    }
    else
    {
      OpenFileAtPath(it->second, true);
    }
  }
}

void MainWindow::LayoutChildren()
{
  if (!statusBar_ || !hwnd_)
  {
    return;
  }

  SendMessageW(statusBar_, WM_SIZE, 0, 0);

  RECT client = {};
  GetClientRect(hwnd_, &client);

  RECT statusRect = {};
  GetWindowRect(statusBar_, &statusRect);
  MapWindowPoints(HWND_DESKTOP, hwnd_, reinterpret_cast<POINT*>(&statusRect), 2);
  client.bottom = statusRect.top;

  const int clientWidth = client.right - client.left;
  int contentHeight = client.bottom - client.top;
  if (contentHeight < 0)
  {
    contentHeight = 0;
  }

  int bottomHeight = showOutputPane_ ? outputPaneHeight_ : 0;
  int leftWidth = showProjectPane_ ? projectPaneWidth_ : 0;

  constexpr int minEditorWidth = 320;
  constexpr int minEditorHeight = 160;

  if (clientWidth - leftWidth < minEditorWidth)
  {
    leftWidth = clientWidth > minEditorWidth ? clientWidth - minEditorWidth : 0;
  }

  if (contentHeight - bottomHeight < minEditorHeight)
  {
    bottomHeight = contentHeight > minEditorHeight ? contentHeight - minEditorHeight : 0;
  }

  const int mainHeight = contentHeight > bottomHeight ? contentHeight - bottomHeight : 0;
  const int editorWidth = clientWidth > leftWidth ? clientWidth - leftWidth : 0;

  constexpr int kSidebarDividerWidth = 1;
  const UINT layoutDpi = GetDpiForWindow(hwnd_);
  MeasureTabStripHeight();
  if (tabStripHeight_ <= 0)
  {
    projectPaneHeaderHeight_ = MulDiv(kDefaultTabStripHeight, static_cast<int>(layoutDpi), 96);
  }
  else
  {
    projectPaneHeaderHeight_ = tabStripHeight_;
  }

  // Sidebar spans the full height above the status bar; tab strip is editor-column only.
  if (showProjectPane_ && leftWidth > kSidebarDividerWidth)
  {
    const int sidebarContentWidth = leftWidth - kSidebarDividerWidth;
    const int treeHeight =
        contentHeight > projectPaneHeaderHeight_ ? contentHeight - projectPaneHeaderHeight_ : 0;
    const int dividerHeight =
        contentHeight > projectPaneHeaderHeight_ ? contentHeight - projectPaneHeaderHeight_ : 0;

    if (projectPaneHeader_)
    {
      // Span the divider column so the bottom rule lines up with the tab strip border.
      MoveWindow(projectPaneHeader_, 0, 0, leftWidth, projectPaneHeaderHeight_, TRUE);
      ShowWindow(projectPaneHeader_, SW_SHOW);
    }
    constexpr int kFilterHeight = 24;
    if (projectFilterEdit_)
    {
      MoveWindow(projectFilterEdit_, 0, projectPaneHeaderHeight_, sidebarContentWidth, kFilterHeight,
                 TRUE);
      ShowWindow(projectFilterEdit_, SW_SHOW);
    }
    if (projectTree_)
    {
      const int treeTop = projectPaneHeaderHeight_ + kFilterHeight;
      const int adjustedTreeHeight = treeHeight > kFilterHeight ? treeHeight - kFilterHeight : 0;
      MoveWindow(projectTree_, 0, treeTop, sidebarContentWidth, adjustedTreeHeight, TRUE);
      ShowWindow(projectTree_, SW_SHOW);
    }
    if (projectPaneDivider_)
    {
      MoveWindow(projectPaneDivider_, leftWidth - kSidebarDividerWidth, projectPaneHeaderHeight_,
                 kSidebarDividerWidth, dividerHeight, TRUE);
      ShowWindow(projectPaneDivider_, SW_SHOW);
    }
  }
  else
  {
    if (projectPaneHeader_)
    {
      ShowWindow(projectPaneHeader_, SW_HIDE);
    }
    if (projectTree_)
    {
      ShowWindow(projectTree_, SW_HIDE);
    }
    if (projectPaneDivider_)
    {
      ShowWindow(projectPaneDivider_, SW_HIDE);
    }
  }

  const int editorLeft = leftWidth;
  const int editorPaneWidth = editorWidth > 0 ? editorWidth : clientWidth;

  RECT workspaceBounds = {editorLeft, 0, editorLeft + editorPaneWidth, mainHeight};
  editorWorkspace_.SetBounds(workspaceBounds);

  if (outputPane_)
  {
    if (showOutputPane_ && bottomHeight > 0 && editorPaneWidth > 0)
    {
      MoveWindow(outputPane_, editorLeft, mainHeight, editorPaneWidth, bottomHeight, TRUE);
      ShowWindow(outputPane_, SW_SHOW);
    }
    else
    {
      ShowWindow(outputPane_, SW_HIDE);
    }
  }

  SyncChromeColors();
}

void MainWindow::MeasureTabStripHeight()
{
  const UINT dpi = GetDpiForWindow(hwnd_);
  tabStripHeight_ = editorWorkspace_.GetTabStripHeight();
  if (tabStripHeight_ <= 0)
  {
    tabStripHeight_ = MulDiv(kDefaultTabStripHeight, static_cast<int>(dpi), 96);
  }
}

void MainWindow::SyncTabBar()
{
  for (size_t i = 0; i < documents_.size(); ++i)
  {
    SyncDocumentDisplayTitle(static_cast<int>(i));
  }

  editorWorkspace_.SyncGroupTabs(documents_);
}

void MainWindow::CenterDialogOnWindow(HWND dialog, int dialogWidth, int dialogHeight) const
{
  CenterWindowOnParent(dialog, hwnd_, dialogWidth, dialogHeight);
}

void MainWindow::InvalidateTabs()
{
  SyncTabBar();
}

void MainWindow::ToggleProjectPane()
{
  showProjectPane_ = !showProjectPane_;
  CheckMenuItem(GetMenu(hwnd_), kCmdViewProjectPane, MF_BYCOMMAND | (showProjectPane_ ? MF_CHECKED : MF_UNCHECKED));
  LayoutChildren();
}

void MainWindow::ToggleOutputPane()
{
  showOutputPane_ = !showOutputPane_;
  CheckMenuItem(GetMenu(hwnd_), kCmdViewOutputPane, MF_BYCOMMAND | (showOutputPane_ ? MF_CHECKED : MF_UNCHECKED));
  LayoutChildren();
}

std::wstring MainWindow::BuildDisplayTitle(int index) const
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return {};
  }

  const EditorDocument& doc = documents_[index];
  return DocumentNaming::DisplayTitle(doc.path, doc.tabTitle, false);
}

void MainWindow::SyncDocumentDisplayTitle(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument& doc = documents_[index];
  doc.displayTitle = DocumentNaming::DisplayTitle(doc.path, doc.tabTitle, doc.modified);
  if (doc.path.empty())
  {
    std::wstring base = doc.displayTitle;
    if (base.size() >= 2 && base.compare(base.size() - 2, 2, L" *") == 0)
    {
      base.resize(base.size() - 2);
    }
    doc.tabTitle = base;
  }
}

bool MainWindow::SaveDocumentAt(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return false;
  }

  const int previous = activeDocumentIndex_;
  SwitchToDocument(index);

  EditorDocument& doc = documents_[index];
  if (doc.path.empty())
  {
    SaveFileAs();
  }
  else
  {
    SaveFile();
  }

  const bool saved = !editorWorkspace_.ActiveEditor().IsModified();
  documents_[index].modified = !saved;
  if (previous >= 0 && previous < static_cast<int>(documents_.size()) && previous != index)
  {
    SwitchToDocument(previous);
  }
  return saved;
}

bool MainWindow::PromptSaveDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return true;
  }

  if (index == activeDocumentIndex_)
  {
    documents_[index].modified = editorWorkspace_.ActiveEditor().IsModified();
  }

  if (!documents_[index].modified)
  {
    return true;
  }

  std::wstring prompt = L"Save changes to ";
  prompt += documents_[index].displayTitle.empty() ? BuildDisplayTitle(index)
                                                     : documents_[index].displayTitle;
  prompt += L"?";

  const int result = MessageBoxW(hwnd_, prompt.c_str(), L"WinCpp", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (result == IDCANCEL)
  {
    return false;
  }

  if (result == IDYES)
  {
    return SaveDocumentAt(index);
  }

  return true;
}

void MainWindow::CreateUntitledDocument()
{
  EditorView& editor = editorWorkspace_.PrimaryEditor();
  void* const document = editor.CreateDocument();

  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.tabTitle = MakeUntitledName();
  entry.displayTitle = entry.tabTitle;
  documents_.push_back(entry);
  AttachEditorToDocument(0);
  editorWorkspace_.ActiveEditor().Clear();
  UpdateStatusBar();
  SyncTabBar();
  SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
}

void MainWindow::ReorderDocument(int fromIndex, int insertBefore)
{
  const int count = static_cast<int>(documents_.size());
  if (fromIndex < 0 || fromIndex >= count || insertBefore < 0 || insertBefore > count)
  {
    return;
  }

  if (insertBefore == fromIndex || insertBefore == fromIndex + 1)
  {
    return;
  }

  SaveCurrentDocumentState();

  void* const activeDoc =
      (activeDocumentIndex_ >= 0 && activeDocumentIndex_ < count)
          ? documents_[activeDocumentIndex_].scintillaDoc
          : nullptr;

  EditorDocument moved = std::move(documents_[fromIndex]);
  documents_.erase(documents_.begin() + fromIndex);

  int target = insertBefore;
  if (fromIndex < insertBefore)
  {
    --target;
  }

  documents_.insert(documents_.begin() + target, std::move(moved));

  editorWorkspace_.ReindexDocumentsAfterReorder(fromIndex, target);

  if (activeDoc)
  {
    for (int i = 0; i < static_cast<int>(documents_.size()); ++i)
    {
      if (documents_[i].scintillaDoc == activeDoc)
      {
        activeDocumentIndex_ = i;
        break;
      }
    }
  }

  SyncTabBar();
}

void MainWindow::CloseDocumentAt(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  if (!PromptSaveDocument(index))
  {
    return;
  }

  void* const documentToClose = documents_[index].scintillaDoc;
  const bool closingOnlyTab = documents_.size() == 1;

  void* const editorDocument = editorWorkspace_.ActiveEditor().GetDocumentPointer();

  if (editorDocument == documentToClose)
  {
    if (closingOnlyTab)
    {
      void* const replacement = editorWorkspace_.PrimaryEditor().CreateDocument();
      editorWorkspace_.ActiveEditor().SetDocumentIfNeeded(replacement);
      editorWorkspace_.ActiveEditor().ReleaseDocument(documentToClose);

      documents_.clear();

      EditorDocument entry;
      entry.scintillaDoc = replacement;
      entry.tabTitle = MakeUntitledName();
      entry.displayTitle = entry.tabTitle;
      documents_.push_back(entry);
      activeDocumentIndex_ = 0;
      editorWorkspace_.EnsureDefaultGroup();
      editorWorkspace_.AttachDocumentToActiveGroup(0, documents_);
      UpdateStatusBar();
      SyncTabBar();
      LayoutChildren();
      SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
      return;
    }

    const int switchIndex = index > 0 ? index - 1 : 1;
    editorWorkspace_.ActiveEditor().SetDocumentIfNeeded(documents_[switchIndex].scintillaDoc);
    activeDocumentIndex_ = switchIndex;
  }
  else
  {
    SaveCurrentDocumentState();
  }

  if (editorWorkspace_.ActiveEditor().GetDocumentPointer() == documentToClose)
  {
    for (size_t i = 0; i < documents_.size(); ++i)
    {
      if (static_cast<int>(i) != index)
      {
        editorWorkspace_.ActiveEditor().SetDocumentIfNeeded(documents_[i].scintillaDoc);
        break;
      }
    }
  }

  editorWorkspace_.ActiveEditor().ReleaseDocument(documentToClose);
  editorWorkspace_.RemoveDocumentFromAllGroups(index);
  editorWorkspace_.PruneEmptyGroups();

  documents_.erase(documents_.begin() + index);
  editorWorkspace_.ReindexDocumentsAfterClose(index);

  if (documents_.empty())
  {
    activeDocumentIndex_ = -1;
    CreateUntitledDocument();
    LayoutChildren();
    return;
  }

  if (index < activeDocumentIndex_)
  {
    activeDocumentIndex_--;
  }

  if (activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    activeDocumentIndex_ = static_cast<int>(documents_.size()) - 1;
  }

  if (activeDocumentIndex_ < 0)
  {
    activeDocumentIndex_ = 0;
  }

  AttachEditorToDocument(activeDocumentIndex_);
  editorWorkspace_.PruneEmptyGroups();
  UpdateStatusBar();
  UpdateActiveTabTitle();
  SyncTabBar();
  LayoutChildren();
  SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
}

void MainWindow::CloseOtherDocuments(int keepIndex)
{
  if (keepIndex < 0 || keepIndex >= static_cast<int>(documents_.size()))
  {
    return;
  }

  for (int i = static_cast<int>(documents_.size()) - 1; i >= 0; --i)
  {
    if (i != keepIndex)
    {
      CloseDocumentAt(i);
      if (keepIndex >= static_cast<int>(documents_.size()))
      {
        keepIndex = static_cast<int>(documents_.size()) - 1;
      }
    }
  }
}

void MainWindow::CloseAllDocuments()
{
  while (documents_.size() > 1)
  {
    const size_t countBefore = documents_.size();
    CloseDocumentAt(static_cast<int>(documents_.size()) - 1);
    if (documents_.size() >= countBefore)
    {
      return;
    }
  }

  if (!documents_.empty())
  {
    CloseDocumentAt(0);
  }
}

void MainWindow::SplitEditor(EditorSplitDirection direction)
{
  SaveCurrentDocumentState();
  editorWorkspace_.SplitActive(direction);
  SyncTabBar();
  LayoutChildren();
  SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
}

int MainWindow::DocumentIndexForGroupTab(int groupId, int localTabIndex) const
{
  return editorWorkspace_.GetDocumentIndex(groupId, localTabIndex);
}

void MainWindow::OnWorkspaceTabSelect(int groupId, int localTabIndex)
{
  const int docIndex = DocumentIndexForGroupTab(groupId, localTabIndex);
  if (docIndex < 0)
  {
    return;
  }

  SaveCurrentDocumentState();
  editorWorkspace_.FocusGroupTab(groupId, localTabIndex, documents_);
  activeDocumentIndex_ = docIndex;
  UpdateStatusBar();
}

void MainWindow::OnWorkspaceTabClose(int groupId, int localTabIndex)
{
  const int docIndex = DocumentIndexForGroupTab(groupId, localTabIndex);
  if (docIndex >= 0)
  {
    CloseDocumentAt(docIndex);
  }
}

void MainWindow::ShowTabContextMenu(int tabIndex)
{
  contextMenuTabIndex_ = tabIndex;
  if (tabIndex != activeDocumentIndex_)
  {
    SwitchToDocument(tabIndex);
  }

  POINT point = {};
  GetCursorPos(&point);
  const UINT flags = TPM_RIGHTBUTTON | TPM_RETURNCMD;
  const UINT command = TrackPopupMenu(tabContextMenu_, flags, point.x, point.y, 0, hwnd_, nullptr);
  if (command != 0)
  {
    SendMessageW(hwnd_, WM_COMMAND, command, 0);
  }
}

void MainWindow::ApplyEditorSettingsToAllGroups()
{
  editorWorkspace_.ApplySettingsToAllEditors(editorSettings_);
  wordWrapEnabled_ = editorSettings_.wordWrap;
  CheckMenuItem(GetMenu(hwnd_), kCmdViewWordWrap,
                MF_BYCOMMAND | (wordWrapEnabled_ ? MF_CHECKED : MF_UNCHECKED));
}

void MainWindow::FindFromDialog(HWND dialog, bool forward)
{
  if (!dialog)
  {
    return;
  }

  const HWND findEdit = GetDlgItem(dialog, kFindDlgFindEdit);
  const bool matchCase = IsDlgButtonChecked(dialog, kFindDlgMatchCase) == BST_CHECKED;
  const bool wholeWord = IsDlgButtonChecked(dialog, kFindDlgWholeWord) == BST_CHECKED;
  const bool regex = IsDlgButtonChecked(dialog, kFindDlgRegex) == BST_CHECKED;
  const std::wstring findText = GetWindowTextString(findEdit);
  if (findText.empty())
  {
    MessageBoxW(hwnd_, L"Enter text to find.", L"Find", MB_OK | MB_ICONINFORMATION);
    return;
  }

  SearchOptions options;
  options.matchCase = matchCase;
  options.wholeWord = wholeWord;
  options.regex = regex;
  options.forward = forward;

  EditorView& editor = editorWorkspace_.ActiveEditor();
  if (!editor.FindNext(findText, options))
  {
    MessageBoxW(hwnd_, L"No matches found.", L"Find", MB_OK | MB_ICONINFORMATION);
    return;
  }

  editor.UpdateFindHighlights(findText, options);
}

void MainWindow::SaveAllFiles()
{
  std::vector<DocumentState> states;
  states.reserve(documents_.size());
  for (const EditorDocument& doc : documents_)
  {
    states.push_back({doc.modified, doc.path});
  }

  for (size_t index : DocumentCollectionLogic::IndicesNeedingSave(states))
  {
    if (!SaveDocumentAt(static_cast<int>(index)))
    {
      break;
    }
  }
}

void MainWindow::ToggleCodeFolding()
{
  codeFoldingEnabled_ = !codeFoldingEnabled_;
  editorWorkspace_.ForEachEditor(
      [&](EditorView& editor) { editor.EnableCodeFolding(codeFoldingEnabled_); });
  CheckMenuItem(GetMenu(hwnd_), kCmdViewCodeFolding,
                MF_BYCOMMAND | (codeFoldingEnabled_ ? MF_CHECKED : MF_UNCHECKED));
}

void MainWindow::ZoomIn()
{
  editorWorkspace_.ActiveEditor().ZoomDelta(1);
  editorSettings_.zoom = editorWorkspace_.ActiveEditor().GetZoom();
  editorSettings_.Save();
}

void MainWindow::ZoomOut()
{
  editorWorkspace_.ActiveEditor().ZoomDelta(-1);
  editorSettings_.zoom = editorWorkspace_.ActiveEditor().GetZoom();
  editorSettings_.Save();
}

void MainWindow::ZoomReset()
{
  while (editorWorkspace_.ActiveEditor().GetZoom() > 0)
  {
    editorWorkspace_.ActiveEditor().ZoomDelta(-1);
  }
  while (editorWorkspace_.ActiveEditor().GetZoom() < 0)
  {
    editorWorkspace_.ActiveEditor().ZoomDelta(1);
  }
  editorSettings_.zoom = 0;
  editorSettings_.Save();
}

void MainWindow::PinActiveTab()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  documents_[activeDocumentIndex_].pinned = !documents_[activeDocumentIndex_].pinned;
  SyncTabBar();
}

void MainWindow::ShowCommandPalette()
{
  const std::wstring query = L"save";
  const auto commands = CommandRegistry::Filter(CommandRegistry::DefaultCommands(), query);
  if (commands.empty())
  {
    return;
  }

  SendMessageW(hwnd_, WM_COMMAND, commands.front().id, 0);
}

void MainWindow::ShowRunCommandDialog()
{
  static bool runClassRegistered = false;
  if (!runClassRegistered)
  {
    RegisterDialogClass(L"WinCppRunDialog", RunDialogProc);
    runClassRegistered = true;
  }

  static RunDialogState state;
  state.window = this;

  HWND dialog = CreateWindowExW(
      WS_EX_DLGMODALFRAME,
      L"WinCppRunDialog",
      L"Run Command",
      WS_CAPTION | WS_SYSMENU | WS_POPUP,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      520,
      140,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      &state);

  if (dialog)
  {
    SetWindowTextW(GetDlgItem(dialog, kRunDlgCommandEdit), g_lastRunCommand.c_str());
    CenterDialogOnWindow(dialog, 520, 140);
    SetForegroundWindow(dialog);
  }
}

void MainWindow::LoadSession()
{
  std::filesystem::path sessionPath(editorSettings_.ConfigPath());
  sessionPath += L".session";
  std::ifstream file(sessionPath, std::ios::binary);
  if (!file)
  {
    return;
  }

  const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  SessionState state;
  if (!SessionStateCodec::Deserialize(json, &state))
  {
    return;
  }

  if (!state.projectRoot.empty() && std::filesystem::is_directory(state.projectRoot))
  {
    showProjectPane_ = state.showProjectPane;
    CheckMenuItem(GetMenu(hwnd_), kCmdViewProjectPane,
                    MF_BYCOMMAND | (showProjectPane_ ? MF_CHECKED : MF_UNCHECKED));
    PopulateProjectTree(state.projectRoot);
  }

  for (const SessionTab& tab : state.tabs)
  {
    if (!tab.path.empty())
    {
      OpenFileAtPath(tab.path, false);
    }
  }

  LayoutChildren();
}

void MainWindow::SaveSession()
{
  SessionState state;
  state.activeGroupId = editorWorkspace_.ActiveGroupId();
  state.projectRoot = projectRootPath_;
  state.showProjectPane = showProjectPane_;
  for (const EditorDocument& doc : documents_)
  {
    if (doc.path.empty())
    {
      continue;
    }
    SessionTab tab;
    tab.path = doc.path;
    tab.pinned = doc.pinned;
    state.tabs.push_back(tab);
  }

  std::filesystem::path sessionPath(editorSettings_.ConfigPath());
  sessionPath += L".session";
  std::ofstream file(sessionPath, std::ios::binary | std::ios::trunc);
  if (file)
  {
    file << SessionStateCodec::Serialize(state);
  }
}
