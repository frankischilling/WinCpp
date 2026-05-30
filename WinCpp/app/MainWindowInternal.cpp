#include "MainWindowInternal.h"

#include "DialogLayout.h"
#include "UiHelpers.h"

#include <commdlg.h>
#include <cwchar>
#include <shlobj.h>

std::wstring g_lastRunCommand = L"echo hello";

constexpr UINT_PTR kEditorSubclassId = 2;
constexpr UINT_PTR kProjectHeaderSubclassId = 3;
constexpr UINT_PTR kOutputHeaderSubclassId = 4;
constexpr int kSplitterHitWidth = 5;
constexpr int kMinProjectPaneWidth = 120;
constexpr int kMinOutputPaneHeight = 80;
constexpr int kDefaultProjectPaneWidth = 240;
constexpr int kDefaultOutputPaneHeight = 180;
constexpr int kTabBottomBorder = 1;
constexpr int kPaneInnerPadding = 6;
constexpr int kPaneControlGap = 6;
constexpr int kToolbarButtonSize = 24;
constexpr int kToolbarIndent = 6;

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

std::wstring Utf8ToWide(const std::string &text)
{
  if (text.empty())
  {
    return {};
  }

  const int length =
      MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0)
  {
    return {};
  }

  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(),
                      length);
  return result;
}

std::string WideToUtf8(const std::wstring &text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (length <= 0)
  {
    return {};
  }

  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(),
                      length, nullptr, nullptr);
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
  text += L"\r\n"
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

const wchar_t *GetDialogFilter()
{
  static const wchar_t kFilter[] =
      L"All Files (*.*)\0*.*\0"
      L"C/C++ Files "
      L"(*.c;*.cpp;*.h;*.hpp;*.cxx;*.hxx)\0*.c;*.cpp;*.h;*.hpp;*.cxx;*.hxx\0"
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

std::wstring ShowSaveFileDialog(HWND owner, const std::wstring &initialPath)
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
  HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, width, kUiButtonHeight,
                              parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              GetModuleHandleW(nullptr), nullptr);
  ApplyEditInnerMargins(edit, 6);
  return edit;
}

HWND CreateDialogPromptLabel(HWND parent, const wchar_t *text, int id)
{
  return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0,
                         0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr), nullptr);
}

HWND CreateDialogFieldLabel(HWND parent, const wchar_t *text, int id)
{
  return CreateWindowExW(0, L"STATIC", text,
                         WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE | SS_NOPREFIX, 0, 0, 0,
                         0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr), nullptr);
}

HWND CreateDialogButton(HWND parent, const wchar_t *label, int x, int y, int width, int id,
                        bool isDefault)
{
  DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
  if (isDefault)
  {
    style |= BS_DEFPUSHBUTTON;
  }

  return CreateWindowExW(0, L"BUTTON", label, style, x, y, width, kUiButtonHeight, parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr), nullptr);
}

HWND CreateDialogCheck(HWND parent, const wchar_t *label, int x, int y, int width, int id)
{
  return CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x, y, width,
                         22, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr), nullptr);
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

bool RegisterDialogClass(const wchar_t *className, WNDPROC procedure)
{
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = procedure;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = className;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
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
