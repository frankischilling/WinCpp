#include "MainWindow.h"

#include "AppIds.h"
#include "DialogLayout.h"
#include "MainWindowInternal.h"
#include "UiHelpers.h"
#include "UiMetrics.h"

struct FindDialogState
{
  MainWindow *window = nullptr;
  bool replaceMode = false;
};

struct GoToDialogState
{
  MainWindow *window = nullptr;
  int maxLine = 1;
};

struct RunDialogState
{
  MainWindow *window = nullptr;
};

LRESULT CALLBACK FindDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  FindDialogState *state =
      reinterpret_cast<FindDialogState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg)
  {
  case WM_CREATE: {
    auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    state = reinterpret_cast<FindDialogState *>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

    CreateDialogFieldLabel(hwnd, L"Find:", kDlgFindLabel);
    CreateLabeledEdit(hwnd, 0, 0, 0, kDlgFindEdit);

    CreateDialogFieldLabel(hwnd, L"Replace:", kDlgReplaceLabel);
    CreateLabeledEdit(hwnd, 0, 0, 0, kDlgReplaceEdit);

    CreateDialogCheck(hwnd, L"Match case", 0, 0, 0, kDlgMatchCase);
    CreateDialogCheck(hwnd, L"Whole word", 0, 0, 0, kDlgWholeWord);
    CreateDialogCheck(hwnd, L"Regex", 0, 0, 0, kDlgRegex);

    CreateDialogButton(hwnd, L"Find Next", 0, 0, 0, kDlgFindNext, true);
    CreateDialogButton(hwnd, L"Find Previous", 0, 0, 0, kDlgFindPrevious);
    CreateDialogButton(hwnd, L"Replace", 0, 0, 0, kDlgReplace);
    CreateDialogButton(hwnd, L"Replace All", 0, 0, 0, kDlgReplaceAll);
    CreateDialogButton(hwnd, L"Close", 0, 0, 0, kDlgClose);

    ApplyUiFontToDescendants(hwnd);
    DialogLayout::LayoutFindReplaceDialog(hwnd, state->replaceMode);
    return 0;
  }
  case WM_SIZE:
    if (state)
    {
      DialogLayout::LayoutFindReplaceDialog(hwnd, state->replaceMode);
    }
    return 0;
  case WM_KEYDOWN:
    if (wParam == VK_RETURN && state && state->window)
    {
      if (GetFocus() == GetDlgItem(hwnd, kDlgFindEdit))
      {
        PostMessageW(state->window->Hwnd(), WM_APP + 1, 1, reinterpret_cast<LPARAM>(hwnd));
        return 0;
      }
    }
    else if (wParam == VK_ESCAPE)
    {
      DestroyWindow(hwnd);
      return 0;
    }
    break;
  case WM_DESTROY:
    if (state && state->window)
    {
      PostMessageW(state->window->Hwnd(), WM_APP + 6, 0, reinterpret_cast<LPARAM>(hwnd));
    }
    return 0;
  case WM_COMMAND: {
    if (!state || !state->window)
    {
      return 0;
    }

    switch (LOWORD(wParam))
    {
    case kDlgFindNext:
      PostMessageW(state->window->Hwnd(), WM_APP + 1, 1, reinterpret_cast<LPARAM>(hwnd));
      return 0;
    case kDlgFindPrevious:
      PostMessageW(state->window->Hwnd(), WM_APP + 1, 0, reinterpret_cast<LPARAM>(hwnd));
      return 0;
    case kDlgReplace:
      PostMessageW(state->window->Hwnd(), WM_APP + 2, 0, reinterpret_cast<LPARAM>(hwnd));
      return 0;
    case kDlgReplaceAll:
      PostMessageW(state->window->Hwnd(), WM_APP + 3, 0, reinterpret_cast<LPARAM>(hwnd));
      return 0;
    case kDlgClose:
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
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLOREDIT:
    return DialogLayout::OnDialogCtlColor(msg, wParam, lParam);
  default:
    break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK GoToDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  GoToDialogState *state =
      reinterpret_cast<GoToDialogState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg)
  {
  case WM_CREATE: {
    auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    state = reinterpret_cast<GoToDialogState *>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

    wchar_t label[64] = {};
    swprintf_s(label, L"Line number (1-%d):", state ? state->maxLine : 1);
    CreateDialogFieldLabel(hwnd, label, kDlgGoToLabel);
    CreateLabeledEdit(hwnd, 0, 0, 0, kDlgGoToLineEdit);
    CreateDialogButton(hwnd, L"Go", 0, 0, 0, kDlgGoToOk, true);
    CreateDialogButton(hwnd, L"Cancel", 0, 0, 0, kDlgGoToCancel);

    ApplyUiFontToDescendants(hwnd);
    DialogLayout::LayoutGoToDialog(hwnd);
    return 0;
  }
  case WM_SIZE:
    DialogLayout::LayoutGoToDialog(hwnd);
    return 0;
  case WM_KEYDOWN:
    if (wParam == VK_RETURN && state && state->window)
    {
      if (GetFocus() == GetDlgItem(hwnd, kDlgGoToLineEdit))
      {
        PostMessageW(state->window->Hwnd(), WM_APP + 4, 0, reinterpret_cast<LPARAM>(hwnd));
        return 0;
      }
    }
    else if (wParam == VK_ESCAPE)
    {
      DestroyWindow(hwnd);
      return 0;
    }
    break;
  case WM_COMMAND: {
    if (!state || !state->window)
    {
      return 0;
    }

    switch (LOWORD(wParam))
    {
    case kDlgGoToOk:
      PostMessageW(state->window->Hwnd(), WM_APP + 4, 0, reinterpret_cast<LPARAM>(hwnd));
      return 0;
    case kDlgGoToCancel:
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
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLOREDIT:
    return DialogLayout::OnDialogCtlColor(msg, wParam, lParam);
  default:
    break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK RunDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  RunDialogState *state =
      reinterpret_cast<RunDialogState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg)
  {
  case WM_CREATE: {
    auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    state = reinterpret_cast<RunDialogState *>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

    CreateDialogFieldLabel(hwnd, L"Command (cmd /c):", kDlgRunLabel);
    CreateLabeledEdit(hwnd, 0, 0, 0, kDlgRunCommandEdit);
    CreateDialogButton(hwnd, L"Run", 0, 0, 0, kDlgRunOk, true);
    CreateDialogButton(hwnd, L"Cancel", 0, 0, 0, kDlgRunCancel);

    ApplyUiFontToDescendants(hwnd);
    DialogLayout::LayoutRunDialog(hwnd);
    return 0;
  }
  case WM_SIZE:
    DialogLayout::LayoutRunDialog(hwnd);
    return 0;
  case WM_KEYDOWN:
    if (wParam == VK_RETURN && state && state->window)
    {
      if (GetFocus() == GetDlgItem(hwnd, kDlgRunCommandEdit))
      {
        PostMessageW(state->window->Hwnd(), WM_APP + 5, 0, reinterpret_cast<LPARAM>(hwnd));
        return 0;
      }
    }
    else if (wParam == VK_ESCAPE)
    {
      DestroyWindow(hwnd);
      return 0;
    }
    break;
  case WM_COMMAND: {
    if (!state || !state->window)
    {
      return 0;
    }

    switch (LOWORD(wParam))
    {
    case kDlgRunOk:
      PostMessageW(state->window->Hwnd(), WM_APP + 5, 0, reinterpret_cast<LPARAM>(hwnd));
      return 0;
    case kDlgRunCancel:
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
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLOREDIT:
    return DialogLayout::OnDialogCtlColor(msg, wParam, lParam);
  default:
    break;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK CreditsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_CREATE: {
    const std::wstring creditsBody = BuildCreditsText();
    HWND creditsEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", creditsBody.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 0,
        0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDlgCreditsText)),
        GetModuleHandleW(nullptr), nullptr);
    ApplyEditInnerMargins(creditsEdit, 6);

    CreateDialogButton(hwnd, L"Close", 0, 0, 0, kDlgCreditsClose);

    ApplyUiFontToDescendants(hwnd);
    DialogLayout::LayoutCreditsDialog(hwnd);
    return 0;
  }
  case WM_SIZE:
    DialogLayout::LayoutCreditsDialog(hwnd);
    return 0;
  case WM_COMMAND:
    if (LOWORD(wParam) == kDlgCreditsClose)
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

  if (findDialog_ && !IsWindow(findDialog_))
  {
    findDialog_ = nullptr;
  }

  constexpr DWORD kDialogStyle = WS_CAPTION | WS_SYSMENU | WS_POPUP;
  constexpr DWORD kDialogExStyle = WS_EX_DLGMODALFRAME;

  const DialogMetrics metrics = DialogLayout::GetMetrics(hwnd_);
  const DialogClientSize clientMin =
      DialogLayout::ComputeMinClientSizeFindReplace(replaceMode, metrics);
  const int preferredWidth = replaceMode ? UiMetrics::ScalePx(hwnd_, 480) : clientMin.width;
  const int clientWidth =
      replaceMode ? (std::max)(clientMin.width, preferredWidth) : clientMin.width;
  const DialogWindowSize outer = DialogLayout::WindowSizeForClient(kDialogStyle, kDialogExStyle,
                                                                   clientWidth, clientMin.height);

  if (findDialog_)
  {
    SetWindowTextW(findDialog_, replaceMode ? L"Replace" : L"Find");
    DialogLayout::LayoutFindReplaceDialog(findDialog_, replaceMode);
    SetWindowPos(findDialog_, nullptr, 0, 0, outer.width, outer.height, SWP_NOMOVE | SWP_NOZORDER);
    CenterDialogOnWindow(findDialog_, outer.width, outer.height);
    ShowWindow(findDialog_, SW_SHOW);
    SetForegroundWindow(findDialog_);

    const HWND findEdit = GetDlgItem(findDialog_, kDlgFindEdit);
    if (findEdit && GetWindowTextLengthW(findEdit) == 0)
    {
      const std::wstring selection = editorWorkspace_.ActiveEditor().GetSelectedText();
      if (!selection.empty())
      {
        SetWindowTextW(findEdit, selection.c_str());
        FocusFindEdit(findDialog_, true);
        return;
      }
    }

    FocusFindEdit(findDialog_, false);
    return;
  }

  HWND dialog =
      CreateWindowExW(kDialogExStyle, L"WinCppFindDialog", replaceMode ? L"Replace" : L"Find",
                      kDialogStyle, CW_USEDEFAULT, CW_USEDEFAULT, outer.width, outer.height, hwnd_,
                      nullptr, GetModuleHandleW(nullptr), &state);

  if (dialog)
  {
    findDialog_ = dialog;
    const std::wstring selection = editorWorkspace_.ActiveEditor().GetSelectedText();
    if (!selection.empty())
    {
      SetWindowTextW(GetDlgItem(dialog, kDlgFindEdit), selection.c_str());
    }

    CenterDialogOnWindow(dialog, outer.width, outer.height);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    FocusFindEdit(dialog, !selection.empty());
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

  constexpr DWORD kDialogStyle = WS_CAPTION | WS_SYSMENU | WS_POPUP;
  constexpr DWORD kDialogExStyle = WS_EX_DLGMODALFRAME;
  const int clientWidth = UiMetrics::ScalePx(hwnd_, 560);
  const int clientHeight = UiMetrics::ScalePx(hwnd_, 440);
  const DialogWindowSize outer =
      DialogLayout::WindowSizeForClient(kDialogStyle, kDialogExStyle, clientWidth, clientHeight);

  HWND dialog = CreateWindowExW(kDialogExStyle, L"WinCppCreditsDialog", L"Credits", kDialogStyle,
                                CW_USEDEFAULT, CW_USEDEFAULT, outer.width, outer.height, hwnd_,
                                nullptr, GetModuleHandleW(nullptr), nullptr);

  if (dialog)
  {
    CenterDialogOnWindow(dialog, outer.width, outer.height);
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
  state.maxLine =
      static_cast<int>(SendMessage(editorWorkspace_.ActiveEditor().Hwnd(), SCI_GETLINECOUNT, 0, 0));

  constexpr DWORD kDialogStyle = WS_CAPTION | WS_SYSMENU | WS_POPUP;
  constexpr DWORD kDialogExStyle = WS_EX_DLGMODALFRAME;

  const DialogMetrics metrics = DialogLayout::GetMetrics(hwnd_);
  const DialogClientSize clientMin = DialogLayout::ComputeMinClientSizeGoTo(metrics);
  const int clientWidth = (std::max)(clientMin.width, UiMetrics::ScalePx(hwnd_, 440));
  const DialogWindowSize outer = DialogLayout::WindowSizeForClient(kDialogStyle, kDialogExStyle,
                                                                   clientWidth, clientMin.height);

  HWND dialog = CreateWindowExW(kDialogExStyle, L"WinCppGoToDialog", L"Go To Line", kDialogStyle,
                                CW_USEDEFAULT, CW_USEDEFAULT, outer.width, outer.height, hwnd_,
                                nullptr, GetModuleHandleW(nullptr), &state);

  if (dialog)
  {
    wchar_t currentLine[16] = {};
    swprintf_s(currentLine, L"%d", editorWorkspace_.ActiveEditor().GetCurrentLine());
    const HWND lineEdit = GetDlgItem(dialog, kDlgGoToLineEdit);
    SetWindowTextW(lineEdit, currentLine);
    CenterDialogOnWindow(dialog, outer.width, outer.height);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    SetFocus(lineEdit);
    SendMessageW(lineEdit, EM_SETSEL, 0, -1);
  }
}

void MainWindow::FindFromDialog(HWND dialog, bool forward)
{
  if (!dialog || !IsWindow(dialog))
  {
    ShowFindReplaceDialog(false);
    return;
  }

  const HWND findEdit = GetDlgItem(dialog, kDlgFindEdit);
  const bool matchCase = IsDlgButtonChecked(dialog, kDlgMatchCase) == BST_CHECKED;
  const bool wholeWord = IsDlgButtonChecked(dialog, kDlgWholeWord) == BST_CHECKED;
  const bool regex = IsDlgButtonChecked(dialog, kDlgRegex) == BST_CHECKED;
  const std::wstring findText = GetWindowTextString(findEdit);
  if (findText.empty())
  {
    SetForegroundWindow(dialog);
    FocusFindEdit(dialog, true);
    return;
  }

  SearchOptions options;
  options.matchCase = matchCase;
  options.wholeWord = wholeWord;
  options.regex = regex;
  options.forward = forward;

  EditorView &editor = editorWorkspace_.ActiveEditor();
  if (!editor.FindNext(findText, options))
  {
    MessageBoxW(hwnd_, L"No matches found.", L"Find", MB_OK | MB_ICONINFORMATION);
    return;
  }

  editor.UpdateFindHighlights(findText, options);
}

void MainWindow::OnFindDialogDestroyed(HWND dialog)
{
  if (findDialog_ == dialog)
  {
    findDialog_ = nullptr;
    editorWorkspace_.ForEachEditor([](EditorView &editor) { editor.ClearFindHighlights(); });
  }
}

void MainWindow::FocusFindEdit(HWND dialog, bool selectAll)
{
  if (!IsWindow(dialog))
  {
    return;
  }

  const HWND findEdit = GetDlgItem(dialog, kDlgFindEdit);
  if (!findEdit)
  {
    return;
  }

  SetFocus(findEdit);
  if (selectAll)
  {
    SendMessageW(findEdit, EM_SETSEL, 0, -1);
  }
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

  constexpr DWORD kDialogStyle = WS_CAPTION | WS_SYSMENU | WS_POPUP;
  constexpr DWORD kDialogExStyle = WS_EX_DLGMODALFRAME;

  const DialogMetrics metrics = DialogLayout::GetMetrics(hwnd_);
  const DialogClientSize clientMin = DialogLayout::ComputeMinClientSizeRun(metrics);
  const int clientWidth = (std::max)(clientMin.width, UiMetrics::ScalePx(hwnd_, 520));
  const DialogWindowSize outer = DialogLayout::WindowSizeForClient(kDialogStyle, kDialogExStyle,
                                                                   clientWidth, clientMin.height);

  HWND dialog = CreateWindowExW(kDialogExStyle, L"WinCppRunDialog", L"Run Command", kDialogStyle,
                                CW_USEDEFAULT, CW_USEDEFAULT, outer.width, outer.height, hwnd_,
                                nullptr, GetModuleHandleW(nullptr), &state);

  if (dialog)
  {
    const HWND commandEdit = GetDlgItem(dialog, kDlgRunCommandEdit);
    SetWindowTextW(commandEdit, g_lastRunCommand.c_str());
    CenterDialogOnWindow(dialog, outer.width, outer.height);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    SetFocus(commandEdit);
    SendMessageW(commandEdit, EM_SETSEL, 0, -1);
  }
}
