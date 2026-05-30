#include "MainWindow.h"

#include "AppIds.h"
#include "DialogLayout.h"
#include "MainWindowConstants.h"
#include "MainWindowInternal.h"
#include "PathLineParser.h"
#include "ProcessOutput.h"
#include "UiHelpers.h"
#include "UiMetrics.h"

#include <windowsx.h>

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)

{

  switch (msg)

  {

  case WM_CREATE:

  {

    INITCOMMONCONTROLSEX icc = {};

    icc.dwSize = sizeof(icc);

    icc.dwICC = ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_COOL_CLASSES;

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

    auto *info = reinterpret_cast<MINMAXINFO *>(lParam);

    info->ptMinTrackSize.x = kMinWindowWidth;

    info->ptMinTrackSize.y = kMinWindowHeight;

    return 0;
  }

  case WM_DPICHANGED:

  {

    LayoutChildren();

    ApplyUiTheme();

    return 0;
  }

  case WM_SYSCOLORCHANGE:

    ApplyUiTheme();

    break;

  case WM_LBUTTONDOWN:

  {

    const int x = GET_X_LPARAM(lParam);

    const int y = GET_Y_LPARAM(lParam);

    if (showProjectPane_ && projectPaneDivider_)

    {

      RECT dividerRect = {};

      GetWindowRect(projectPaneDivider_, &dividerRect);

      MapWindowPoints(HWND_DESKTOP, hwnd_, reinterpret_cast<POINT *>(&dividerRect), 2);

      dividerRect.left -= UiMetrics::ScalePx(hwnd_, kSplitterHitWidth);

      dividerRect.right += UiMetrics::ScalePx(hwnd_, kSplitterHitWidth);

      POINT pt = {x, y};

      if (PtInRect(&dividerRect, pt))

      {

        BeginProjectSplitterDrag(x);

        return 0;
      }
    }

    if (showOutputPane_ && outputSplitter_)

    {

      RECT splitterRect = {};

      GetWindowRect(outputSplitter_, &splitterRect);

      MapWindowPoints(HWND_DESKTOP, hwnd_, reinterpret_cast<POINT *>(&splitterRect), 2);

      splitterRect.top -= UiMetrics::ScalePx(hwnd_, kSplitterHitWidth);

      splitterRect.bottom += UiMetrics::ScalePx(hwnd_, kSplitterHitWidth);

      POINT pt = {x, y};

      if (PtInRect(&splitterRect, pt))

      {

        BeginOutputSplitterDrag(y);

        return 0;
      }
    }

    break;
  }

  case WM_MOUSEMOVE:

    if (draggingProjectSplitter_ || draggingOutputSplitter_)

    {

      ContinueSplitterDrag({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});

      return 0;
    }

    break;

  case WM_LBUTTONUP:

    if (draggingProjectSplitter_ || draggingOutputSplitter_)

    {

      EndSplitterDrag();

      return 0;
    }

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

    editorWorkspace_.OnTabDragMove(static_cast<int>(HIWORD(wParam)),
                                   static_cast<int>(LOWORD(wParam)),

                                   pt);

    return 0;
  }

  case WM_TABBAR_DRAG_END:

  {

    const POINT pt = UnpackScreenPoint(lParam);

    editorWorkspace_.OnTabDragEnd(static_cast<int>(HIWORD(wParam)),
                                  static_cast<int>(LOWORD(wParam)),

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

      if (findDialog_ && !IsWindow(findDialog_))

      {

        findDialog_ = nullptr;
      }

      if (!findDialog_)

      {

        ShowFindReplaceDialog(false);
      }

      else

      {

        FindFromDialog(findDialog_, false);
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

        const std::vector<std::wstring> &items = recentFiles_.Items();

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

    const auto *header = reinterpret_cast<NMHDR *>(lParam);

    if (header->hwndFrom == statusBar_ && header->code == NM_CLICK)

    {

      OnStatusBarClick(header);

      return 0;
    }

    if (header->hwndFrom == projectTree_ && header->code == NM_DBLCLK)

    {

      OnProjectTreeDoubleClick();

      return 0;
    }

    if (header->hwndFrom == editorWorkspace_.ActiveEditor().Hwnd())

    {

      const auto *notification = reinterpret_cast<SCNotification *>(lParam);

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

    const HWND findEdit = GetDlgItem(dialog, kDlgFindEdit);

    const HWND replaceEdit = GetDlgItem(dialog, kDlgReplaceEdit);

    const bool matchCase = IsDlgButtonChecked(dialog, kDlgMatchCase) == BST_CHECKED;

    const bool wholeWord = IsDlgButtonChecked(dialog, kDlgWholeWord) == BST_CHECKED;

    const bool regex = IsDlgButtonChecked(dialog, kDlgRegex) == BST_CHECKED;

    const std::wstring findText = GetWindowTextString(findEdit);

    const std::wstring replaceText = replaceEdit ? GetWindowTextString(replaceEdit) : L"";

    if (findText.empty())

    {

      if (IsWindow(dialog))

      {

        SetForegroundWindow(dialog);

        FocusFindEdit(dialog, true);
      }

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

    const std::wstring text = GetWindowTextString(GetDlgItem(dialog, kDlgGoToLineEdit));

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

    const std::wstring command = GetWindowTextString(GetDlgItem(dialog, kDlgRunCommandEdit));

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

  case WM_APP + 6:

    OnFindDialogDestroyed(reinterpret_cast<HWND>(lParam));

    return 0;

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

    if (outputFont_)

    {

      DeleteObject(outputFont_);

      outputFont_ = nullptr;
    }

    if (treeImageList_)

    {

      ImageList_Destroy(treeImageList_);

      treeImageList_ = nullptr;
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

  return editorWorkspace_.IsPointOverAnyTabBar(screenPoint);
}

LRESULT CALLBACK MainWindow::EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,

                                                UINT_PTR, DWORD_PTR refData)

{

  auto *self = reinterpret_cast<MainWindow *>(refData);

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
