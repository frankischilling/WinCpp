#include "MainWindow.h"

#include <commdlg.h>
#include <filesystem>
#include <algorithm>
#include <shlobj.h>
#include <windowsx.h>

#include <Scintilla.h>

namespace
{
constexpr wchar_t kMainWindowClassName[] = L"WinCppMainWindow";
constexpr int kDefaultTabStripHeight = 28;
constexpr int kStatusCaretWidth = 180;
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

constexpr int kGoToDlgLineEdit = 3101;
constexpr int kGoToDlgOk = 3102;
constexpr int kGoToDlgCancel = 3103;
constexpr UINT_PTR kEditorSubclassId = 2;

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
      24,
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
      26,
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

      CreateWindowExW(0, L"STATIC", L"Find:", WS_CHILD | WS_VISIBLE, 12, 12, 60, 18, hwnd,
                      nullptr, GetModuleHandleW(nullptr), nullptr);
      CreateLabeledEdit(hwnd, 72, 8, 280, kFindDlgFindEdit);

      if (state->replaceMode)
      {
        CreateWindowExW(0, L"STATIC", L"Replace:", WS_CHILD | WS_VISIBLE, 12, 44, 60, 18, hwnd,
                        nullptr, GetModuleHandleW(nullptr), nullptr);
        CreateLabeledEdit(hwnd, 72, 40, 280, kFindDlgReplaceEdit);
      }

      const int optionY = state->replaceMode ? 76 : 44;
      CreateDialogCheck(hwnd, L"Match case", 12, optionY, 120, kFindDlgMatchCase);
      CreateDialogCheck(hwnd, L"Whole word", 140, optionY, 120, kFindDlgWholeWord);

      const int buttonY = state->replaceMode ? 108 : 76;
      CreateDialogButton(hwnd, L"Find Next", 12, buttonY, 90, kFindDlgFindNext);
      if (state->replaceMode)
      {
        CreateDialogButton(hwnd, L"Replace", 108, buttonY, 90, kFindDlgReplace);
        CreateDialogButton(hwnd, L"Replace All", 204, buttonY, 90, kFindDlgReplaceAll);
      }
      CreateDialogButton(hwnd, L"Close", 300, buttonY, 70, kFindDlgClose);
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

      wchar_t label[64] = {};
      swprintf_s(label, L"Line number (1-%d):", state ? state->maxLine : 1);
      CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE, 12, 14, 220, 18, hwnd,
                      nullptr, GetModuleHandleW(nullptr), nullptr);
      CreateLabeledEdit(hwnd, 12, 36, 220, kGoToDlgLineEdit);
      CreateDialogButton(hwnd, L"Go", 250, 34, 70, kGoToDlgOk);
      CreateDialogButton(hwnd, L"Cancel", 330, 34, 70, kGoToDlgCancel);
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
    tabStripHeight_(kDefaultTabStripHeight),
    outputPaneHeight_(180),
    showProjectPane_(true),
    showOutputPane_(true),
    wordWrapEnabled_(false)
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
      CreateMenus();
      CreateAccelerators();
      CreateChildPanes();
      EnsureInitialDocument();
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
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      if (tabBar_.Hwnd() &&
          IsPointOverTabBar({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}))
      {
        return SendMessageW(tabBar_.Hwnd(), msg, wParam, lParam);
      }
      break;
    case WM_TABBAR_SELECT:
      if (static_cast<int>(wParam) >= 0 && static_cast<int>(wParam) != activeDocumentIndex_)
      {
        SwitchToDocument(static_cast<int>(wParam));
      }
      return 0;
    case WM_TABBAR_CLOSE:
      CloseDocumentAt(static_cast<int>(wParam));
      return 0;
    case WM_TABBAR_CONTEXTMENU:
      ShowTabContextMenu(static_cast<int>(wParam));
      return 0;
    case WM_COMMAND:
    {
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
        case kCmdFileOpenRecentClear:
          ClearRecentFiles();
          return 0;
        case kCmdFileExit:
          DestroyWindow(hwnd_);
          return 0;
        case kCmdEditUndo:
          SendMessage(editor_.Hwnd(), SCI_UNDO, 0, 0);
          return 0;
        case kCmdEditRedo:
          SendMessage(editor_.Hwnd(), SCI_REDO, 0, 0);
          return 0;
        case kCmdEditCut:
          SendMessage(editor_.Hwnd(), SCI_CUT, 0, 0);
          return 0;
        case kCmdEditCopy:
          SendMessage(editor_.Hwnd(), SCI_COPY, 0, 0);
          return 0;
        case kCmdEditPaste:
          SendMessage(editor_.Hwnd(), SCI_PASTE, 0, 0);
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
        case kCmdViewWordWrap:
          ToggleWordWrap();
          return 0;
        case kCmdHelpAbout:
          MessageBoxW(hwnd_, L"WinCpp text editor.", L"About WinCpp", MB_OK | MB_ICONINFORMATION);
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

      if (header->hwndFrom == editor_.Hwnd())
      {
        const auto* notification = reinterpret_cast<SCNotification*>(lParam);
        if (notification->nmhdr.code == SCN_UPDATEUI)
        {
          UpdateStatusBar();
        }
        else if (notification->nmhdr.code == SCN_MODIFIED &&
                 (notification->modificationType & SC_MOD_INSERTTEXT ||
                  notification->modificationType & SC_MOD_DELETETEXT))
        {
          if (activeDocumentIndex_ >= 0 &&
              activeDocumentIndex_ < static_cast<int>(documents_.size()))
          {
            documents_[activeDocumentIndex_].modified = editor_.IsModified();
          }
          UpdateStatusBar();
          UpdateActiveTabTitle();
        }
      }
      break;
    }
    case WM_APP + 1:
    case WM_APP + 2:
    case WM_APP + 3:
    {
      HWND dialog = reinterpret_cast<HWND>(lParam);
      const HWND findEdit = GetDlgItem(dialog, kFindDlgFindEdit);
      const HWND replaceEdit = GetDlgItem(dialog, kFindDlgReplaceEdit);
      const bool matchCase = IsDlgButtonChecked(dialog, kFindDlgMatchCase) == BST_CHECKED;
      const bool wholeWord = IsDlgButtonChecked(dialog, kFindDlgWholeWord) == BST_CHECKED;
      const std::wstring findText = GetWindowTextString(findEdit);
      const std::wstring replaceText = replaceEdit ? GetWindowTextString(replaceEdit) : L"";

      if (findText.empty())
      {
        MessageBoxW(hwnd_, L"Enter text to find.", L"Find", MB_OK | MB_ICONINFORMATION);
        return 0;
      }

      if (msg == WM_APP + 1)
      {
        if (!editor_.FindNext(findText, matchCase, wholeWord, true))
        {
          MessageBoxW(hwnd_, L"No matches found.", L"Find", MB_OK | MB_ICONINFORMATION);
        }
      }
      else if (msg == WM_APP + 2)
      {
        editor_.ReplaceSelection(findText, replaceText, matchCase, wholeWord);
      }
      else if (msg == WM_APP + 3)
      {
        const int count = editor_.ReplaceAll(findText, replaceText, matchCase, wholeWord);
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

      editor_.GoToLine(line);
      DestroyWindow(dialog);
      UpdateStatusBar();
      return 0;
    }
    case WM_DESTROY:
      if (accelerators_)
      {
        DestroyAcceleratorTable(accelerators_);
        accelerators_ = nullptr;
      }
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

bool MainWindow::IsPointOverTabBar(POINT screenPoint) const
{
  if (!tabBar_.Hwnd())
  {
    return false;
  }

  RECT tabRect = {};
  GetWindowRect(tabBar_.Hwnd(), &tabRect);
  return PtInRect(&tabRect, screenPoint) != 0;
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
    if (self->tabBar_.Hwnd() &&
        self->IsPointOverTabBar({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}))
    {
      return SendMessageW(self->tabBar_.Hwnd(), msg, wParam, lParam);
    }
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
  AppendMenuW(editMenu, MF_STRING, kCmdEditGoToLine, L"&Go To Line...\tCtrl+G");

  AppendMenuW(viewMenu, MF_STRING | MF_CHECKED, kCmdViewProjectPane, L"&Project Pane");
  AppendMenuW(viewMenu, MF_STRING | MF_CHECKED, kCmdViewOutputPane, L"&Output Pane");
  AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(viewMenu, MF_STRING, kCmdViewWordWrap, L"&Word Wrap");

  AppendMenuW(helpMenu, MF_STRING, kCmdHelpAbout, L"&About");

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
      {FVIRTKEY | FCONTROL, 'W', kCmdTabClose},
  };

  accelerators_ = CreateAcceleratorTableW(table, static_cast<int>(std::size(table)));
}

void MainWindow::CreateChildPanes()
{
  projectTree_ = CreateWindowExW(
      WS_EX_CLIENTEDGE,
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

  tabBar_.Create(hwnd_, GetModuleHandleW(nullptr), kCtrlTab);

  tabContextMenu_ = CreatePopupMenu();
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabClose, L"Close");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabCloseOthers, L"Close Others");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabCloseAll, L"Close All");

  outputPane_ = CreateWindowExW(
      WS_EX_CLIENTEDGE,
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

  editor_.Create(hwnd_, GetModuleHandleW(nullptr));
  SetWindowSubclass(editor_.Hwnd(), EditorSubclassProc, kEditorSubclassId,
                    reinterpret_cast<DWORD_PTR>(this));
  const std::filesystem::path syntaxDir = GetExecutableDir() / L"syntax";
  editor_.LoadSyntaxDirectory(syntaxDir.wstring());

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
  UpdateStatusBar();
}

void MainWindow::EnsureInitialDocument()
{
  if (!documents_.empty())
  {
    return;
  }

  void* document = editor_.CreateDocument();
  editor_.SetDocument(document);
  editor_.Clear();

  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.tabTitle = MakeUntitledName();
  entry.displayTitle = entry.tabTitle;
  documents_.push_back(entry);
  activeDocumentIndex_ = 0;
  SyncTabBar();
}

std::wstring MainWindow::MakeUntitledName()
{
  ++untitledCounter_;
  return L"Untitled-" + std::to_wstring(untitledCounter_);
}

std::wstring MainWindow::TabLabelForPath(const std::wstring& path) const
{
  if (path.empty())
  {
    return L"Untitled";
  }

  return std::filesystem::path(path).filename().wstring();
}

void MainWindow::SaveCurrentDocumentState()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  documents_[activeDocumentIndex_].scintillaDoc = editor_.GetDocumentPointer();
}

void MainWindow::AttachEditorToDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  if (activeDocumentIndex_ >= 0 && activeDocumentIndex_ < static_cast<int>(documents_.size()))
  {
    documents_[activeDocumentIndex_].modified = editor_.IsModified();
  }

  editor_.SetDocumentIfNeeded(documents_[index].scintillaDoc);
  activeDocumentIndex_ = index;
  documents_[index].modified = editor_.IsModified();
  tabBar_.SetSelectedIndex(index);
}

void MainWindow::SwitchToDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  if (index == activeDocumentIndex_ &&
      editor_.GetDocumentPointer() == documents_[index].scintillaDoc)
  {
    return;
  }

  if (activeDocumentIndex_ >= 0 && activeDocumentIndex_ < static_cast<int>(documents_.size()))
  {
    SaveCurrentDocumentState();
  }

  AttachEditorToDocument(index);
  tabBar_.RevealTab(index);
  UpdateStatusBar();
  SetFocus(editor_.Hwnd());
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
  tabBar_.UpdateTitle(activeDocumentIndex_,
                      doc.displayTitle.empty() ? doc.tabTitle : doc.displayTitle);
}

void MainWindow::NewFile()
{
  SaveCurrentDocumentState();

  void* const document = editor_.CreateDocument();
  AddDocumentTab(L"", document);
  editor_.Clear();
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

  void* const document = editor_.CreateDocument();

  std::string error;
  if (!editor_.LoadFromFileIntoDocument(document, path, &error))
  {
    editor_.ReleaseDocument(document);
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
  editor_.ApplySyntaxForPath(path);
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

  std::string error;
  if (!editor_.SaveToFile(doc.path, &error))
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
  if (!editor_.SaveToFile(path, &error))
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

  std::wstring pathLabel = L"Untitled";
  if (activeDocumentIndex_ >= 0 && activeDocumentIndex_ < static_cast<int>(documents_.size()) &&
      !documents_[activeDocumentIndex_].path.empty())
  {
    pathLabel = documents_[activeDocumentIndex_].path;
  }

  if (editor_.IsModified())
  {
    pathLabel += L" *";
  }

  wchar_t caretLabel[64] = {};
  swprintf_s(caretLabel, L"Ln %d, Col %d", editor_.GetCurrentLine(), editor_.GetCurrentColumn());

  SendMessageW(statusBar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(pathLabel.c_str()));
  SendMessageW(statusBar_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(caretLabel));
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
  editor_.SetWordWrap(wordWrapEnabled_);
  CheckMenuItem(GetMenu(hwnd_), kCmdViewWordWrap,
                MF_BYCOMMAND | (wordWrapEnabled_ ? MF_CHECKED : MF_UNCHECKED));
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
  const int height = replaceMode ? 170 : 130;
  HWND dialog = CreateWindowExW(
      WS_EX_DLGMODALFRAME,
      L"WinCppFindDialog",
      replaceMode ? L"Replace" : L"Find",
      style,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      390,
      height,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      &state);

  if (dialog)
  {
    CenterDialogOnWindow(dialog, 390, height);
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
  state.maxLine = static_cast<int>(SendMessage(editor_.Hwnd(), SCI_GETLINECOUNT, 0, 0));

  HWND dialog = CreateWindowExW(
      WS_EX_DLGMODALFRAME,
      L"WinCppGoToDialog",
      L"Go To Line",
      WS_CAPTION | WS_SYSMENU | WS_POPUP,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      420,
      120,
      hwnd_,
      nullptr,
      GetModuleHandleW(nullptr),
      &state);

  if (dialog)
  {
    wchar_t currentLine[16] = {};
    swprintf_s(currentLine, L"%d", editor_.GetCurrentLine());
    SetWindowTextW(GetDlgItem(dialog, kGoToDlgLineEdit), currentLine);
    CenterDialogOnWindow(dialog, 420, 120);
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
    OpenFileAtPath(it->second, true);
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

  if (tabBar_.Hwnd() && editorWidth > 0)
  {
    MoveWindow(tabBar_.Hwnd(), leftWidth, 0, editorWidth, kDefaultTabStripHeight, TRUE);
    MeasureTabStripHeight();
    MoveWindow(tabBar_.Hwnd(), leftWidth, 0, editorWidth, tabStripHeight_, TRUE);
    SyncTabBar();
  }

  const int paneTop = tabStripHeight_;
  const int paneHeight = mainHeight > paneTop ? mainHeight - paneTop : 0;

  if (projectTree_)
  {
    if (showProjectPane_ && leftWidth > 0)
    {
      MoveWindow(projectTree_, 0, paneTop, leftWidth, paneHeight, TRUE);
      ShowWindow(projectTree_, SW_SHOW);
    }
    else
    {
      ShowWindow(projectTree_, SW_HIDE);
    }
  }

  const int editorLeft = leftWidth;
  const int editorPaneWidth = editorWidth > 0 ? editorWidth : clientWidth;

  if (editor_.Hwnd())
  {
    MoveWindow(editor_.Hwnd(), editorLeft, paneTop, editorPaneWidth, paneHeight, TRUE);
  }

  if (outputPane_)
  {
    if (showOutputPane_ && bottomHeight > 0)
    {
      MoveWindow(outputPane_, 0, mainHeight, clientWidth, bottomHeight, TRUE);
      ShowWindow(outputPane_, SW_SHOW);
    }
    else
    {
      ShowWindow(outputPane_, SW_HIDE);
    }
  }
}

void MainWindow::MeasureTabStripHeight()
{
  if (!tabBar_.Hwnd())
  {
    tabStripHeight_ = kDefaultTabStripHeight;
    return;
  }

  const UINT dpi = GetDpiForWindow(hwnd_);
  const int height = tabBar_.GetPreferredHeight();
  tabStripHeight_ = height > 0 ? height : MulDiv(kDefaultTabStripHeight, static_cast<int>(dpi), 96);
}

void MainWindow::SyncTabBar()
{
  if (!tabBar_.Hwnd())
  {
    return;
  }

  for (size_t i = 0; i < documents_.size(); ++i)
  {
    SyncDocumentDisplayTitle(static_cast<int>(i));
  }

  std::vector<std::wstring> titles;
  titles.reserve(documents_.size());
  for (const EditorDocument& doc : documents_)
  {
    titles.push_back(doc.displayTitle.empty() ? doc.tabTitle : doc.displayTitle);
  }

  tabBar_.SetTabs(titles, activeDocumentIndex_);
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
  std::wstring baseTitle = doc.path.empty() ? doc.tabTitle : TabLabelForPath(doc.path);
  return baseTitle;
}

void MainWindow::SyncDocumentDisplayTitle(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument& doc = documents_[index];
  std::wstring title = BuildDisplayTitle(index);

  if (doc.modified)
  {
    title += L" *";
  }

  doc.displayTitle = title;
  if (doc.path.empty())
  {
    std::wstring base = title;
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

  const bool saved = !editor_.IsModified();
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
    documents_[index].modified = editor_.IsModified();
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
  void* const document = editor_.CreateDocument();

  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.tabTitle = MakeUntitledName();
  entry.displayTitle = entry.tabTitle;
  documents_.push_back(entry);
  AttachEditorToDocument(0);
  editor_.Clear();
  UpdateStatusBar();
  SyncTabBar();
  SetFocus(editor_.Hwnd());
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

  void* const editorDocument = editor_.GetDocumentPointer();

  if (editorDocument == documentToClose)
  {
    if (closingOnlyTab)
    {
      void* const replacement = editor_.CreateDocument();
      editor_.SetDocumentIfNeeded(replacement);
      editor_.ReleaseDocument(documentToClose);

      documents_.clear();

      EditorDocument entry;
      entry.scintillaDoc = replacement;
      entry.tabTitle = MakeUntitledName();
      entry.displayTitle = entry.tabTitle;
      documents_.push_back(entry);
      activeDocumentIndex_ = 0;
      tabBar_.RevealTab(0);
      UpdateStatusBar();
      SyncTabBar();
      LayoutChildren();
      SetFocus(editor_.Hwnd());
      return;
    }

    const int switchIndex = index > 0 ? index - 1 : 1;
    editor_.SetDocumentIfNeeded(documents_[switchIndex].scintillaDoc);
    activeDocumentIndex_ = switchIndex;
  }
  else
  {
    SaveCurrentDocumentState();
  }

  if (editor_.GetDocumentPointer() == documentToClose)
  {
    for (size_t i = 0; i < documents_.size(); ++i)
    {
      if (static_cast<int>(i) != index)
      {
        editor_.SetDocumentIfNeeded(documents_[i].scintillaDoc);
        break;
      }
    }
  }

  editor_.ReleaseDocument(documentToClose);

  documents_.erase(documents_.begin() + index);

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
  tabBar_.RevealTab(activeDocumentIndex_);
  UpdateStatusBar();
  UpdateActiveTabTitle();
  SyncTabBar();
  LayoutChildren();
  SetFocus(editor_.Hwnd());
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
