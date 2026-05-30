#include "MainWindow.h"

#include "AppIds.h"
#include "CommandRegistry.h"
#include "ConfigJson.h"
#include "DialogLayout.h"
#include "MainWindowConstants.h"
#include "MainWindowInternal.h"
#include "StatusBarModel.h"
#include "SyntaxRegistry.h"
#include "UiHelpers.h"
#include "UiMetrics.h"
#include "UiTheme.h"

#include <algorithm>
#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>

LRESULT CALLBACK MainWindow::ProjectPaneHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                           LPARAM lParam, UINT_PTR,
                                                           DWORD_PTR refData)
{
  auto *self = reinterpret_cast<MainWindow *>(refData);
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
    DrawTextW(hdc, L"PROJECT", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

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

LRESULT CALLBACK MainWindow::OutputPaneHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                          LPARAM lParam, UINT_PTR,
                                                          DWORD_PTR refData)
{
  auto *self = reinterpret_cast<MainWindow *>(refData);
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

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
    if (!font)
    {
      font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, font));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, chrome.sidebarHeaderForeground);

    RECT textRect = client;
    textRect.left += UiMetrics::ScalePx(hwnd, 10);
    DrawTextW(hdc, L"OUTPUT", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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

void MainWindow::CreateToolbar()
{
  rebar_ = CreateWindowExW(0, REBARCLASSNAMEW, nullptr,
                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_BORDER | RBS_VARHEIGHT |
                               RBS_BANDBORDERS,
                           0, 0, 0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);

  toolbar_ = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                             WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS |
                                 CCS_NODIVIDER | CCS_NORESIZE,
                             0, 0, 0, 0, rebar_, nullptr, GetModuleHandleW(nullptr), nullptr);

  if (!rebar_ || !toolbar_)
  {
    return;
  }

  SendMessageW(toolbar_, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

  const int iconPx = UiMetrics::ScalePx(hwnd_, 16);
  SendMessageW(toolbar_, TB_SETBITMAPSIZE, 0, MAKELPARAM(iconPx, iconPx));
  const int buttonPx = UiMetrics::ScalePx(hwnd_, kToolbarButtonSize);
  SendMessageW(toolbar_, TB_SETBUTTONSIZE, 0, MAKELPARAM(buttonPx, buttonPx));
  SendMessageW(toolbar_, TB_SETINDENT, UiMetrics::ScalePx(hwnd_, kToolbarIndent), 0);

  TBADDBITMAP stdBitmap = {};
  stdBitmap.hInst = HINST_COMMCTRL;
  stdBitmap.nID = IDB_STD_SMALL_COLOR;
  const int bitmapOffset = static_cast<int>(
      SendMessageW(toolbar_, TB_ADDBITMAP, 15, reinterpret_cast<LPARAM>(&stdBitmap)));
  const int iconBase = bitmapOffset >= 0 ? bitmapOffset : 0;

  const TBBUTTON buttons[] = {
      {iconBase + STD_FILENEW, kCmdFileNew, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_FILEOPEN, kCmdFileOpen, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_FILESAVE, kCmdFileSave, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_CUT, kCmdEditCut, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_COPY, kCmdEditCopy, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_PASTE, kCmdEditPaste, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_FIND, kCmdEditFind, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_UNDO, kCmdEditUndo, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
      {iconBase + STD_REDOW, kCmdEditRedo, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
  };
  SendMessageW(toolbar_, TB_ADDBUTTONS, static_cast<WPARAM>(std::size(buttons)),
               reinterpret_cast<LPARAM>(buttons));

  if (!toolbarTooltip_)
  {
    toolbarTooltip_ = DialogLayout::CreateToolbarTooltipHost(hwnd_);
    if (toolbarTooltip_)
    {
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdFileNew, L"New (Ctrl+N)");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdFileOpen, L"Open (Ctrl+O)");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdFileSave, L"Save (Ctrl+S)");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdEditCut, L"Cut");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdEditCopy, L"Copy");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdEditPaste, L"Paste");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdEditFind, L"Find (Ctrl+F)");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdEditUndo, L"Undo");
      DialogLayout::AddToolbarTooltip(toolbarTooltip_, toolbar_, kCmdEditRedo, L"Redo");
      DialogLayout::AttachToolbarTooltips(toolbar_, toolbarTooltip_);
    }
  }

  REBARBANDINFOW band = {};
  band.cbSize = sizeof(band);
  band.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE;
  band.fStyle = RBBS_GRIPPERALWAYS;
  band.hwndChild = toolbar_;
  const DWORD toolbarButtonSize =
      static_cast<DWORD>(SendMessageW(toolbar_, TB_GETBUTTONSIZE, 0, 0));
  const UINT toolbarButtonWidth = LOWORD(toolbarButtonSize);
  const UINT toolbarButtonHeight = HIWORD(toolbarButtonSize);
  band.cxMinChild =
      toolbarButtonWidth * static_cast<UINT>(std::size(buttons)) + UiMetrics::ScalePx(hwnd_, 12);
  band.cyMinChild = toolbarButtonHeight;
  band.cx = band.cxMinChild + UiMetrics::ScalePx(hwnd_, 12);
  SendMessageW(rebar_, RB_INSERTBAND, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(&band));
  SendMessageW(toolbar_, TB_AUTOSIZE, 0, 0);
  SendMessageW(rebar_, WM_SIZE, 0, 0);
}

int MainWindow::ToolbarHeight() const
{
  if (!rebar_)
  {
    return 0;
  }

  const LRESULT barHeight = SendMessageW(rebar_, RB_GETBARHEIGHT, 0, 0);
  if (barHeight > 0)
  {
    return static_cast<int>(barHeight);
  }

  RECT rect = {};
  GetWindowRect(rebar_, &rect);
  return rect.bottom - rect.top;
}

void MainWindow::CreateChildPanes()
{
  CreateToolbar();

  projectPaneHeader_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
                                       nullptr, GetModuleHandleW(nullptr), nullptr);
  SetWindowSubclass(projectPaneHeader_, ProjectPaneHeaderSubclassProc, kProjectHeaderSubclassId,
                    reinterpret_cast<DWORD_PTR>(this));

  projectPaneDivider_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
                                        nullptr, GetModuleHandleW(nullptr), nullptr);

  projectTree_ =
      CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                      WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS, 0, 0,
                      0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);

  tabContextMenu_ = CreatePopupMenu();
  projectFilterEdit_ =
      CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtrlProjectFilter)),
                      GetModuleHandleW(nullptr), nullptr);
  ApplyEditInnerMargins(projectFilterEdit_, 4);

  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabClose, L"Close");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabCloseOthers, L"Close Others");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabCloseAll, L"Close All");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdTabPin, L"Pin Tab");
  AppendMenuW(tabContextMenu_, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdViewSplitRight, L"Split Right");
  AppendMenuW(tabContextMenu_, MF_STRING, kCmdViewSplitDown, L"Split Down");

  outputPaneHeader_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
                                      nullptr, GetModuleHandleW(nullptr), nullptr);
  SetWindowSubclass(outputPaneHeader_, OutputPaneHeaderSubclassProc, kOutputHeaderSubclassId,
                    reinterpret_cast<DWORD_PTR>(this));

  outputSplitter_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
                                    nullptr, GetModuleHandleW(nullptr), nullptr);

  outputPane_ = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"Run a command from the View menu to see output here.\r\n",
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 0, 0,
      hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
  ApplyEditInnerMargins(outputPane_, 6);

  editorWorkspace_.Create(hwnd_, GetModuleHandleW(nullptr), hwnd_,
                          reinterpret_cast<DWORD_PTR>(this));
  const std::filesystem::path syntaxDir = GetExecutableDir() / L"syntax";
  editorWorkspace_.LoadSyntaxDirectory(syntaxDir.wstring());

  statusBar_ = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                               hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);

  int parts[] = {-UiMetrics::ScalePx(hwnd_, kStatusCaretWidth), -1};
  SendMessageW(statusBar_, SB_SETPARTS, 2, reinterpret_cast<LPARAM>(parts));
  int statusBorders[] = {0, 0, 0};
  SendMessageW(statusBar_, SB_SETBORDERS, 0, reinterpret_cast<LPARAM>(statusBorders));

  ApplyUiFont(projectPaneHeader_);
  ApplyUiFont(projectFilterEdit_);
  ApplyUiFont(projectTree_);
  ApplyUiFont(outputPaneHeader_);
  ApplyUiFont(outputPane_);
  ApplyUiFont(statusBar_);

  if (projectTree_)
  {
    SetWindowTheme(projectTree_, L"Explorer", nullptr);
  }
  if (projectFilterEdit_)
  {
    SetWindowTheme(projectFilterEdit_, L"Explorer", nullptr);
  }
  if (statusBar_)
  {
    SetWindowTheme(statusBar_, L"Explorer", nullptr);
  }

  LOGFONTW logFont = {};
  logFont.lfHeight = -UiMetrics::ScalePx(hwnd_, 11);
  wcscpy_s(logFont.lfFaceName, L"Consolas");
  outputFont_ = CreateFontIndirectW(&logFont);
  if (outputFont_)
  {
    SendMessageW(outputPane_, WM_SETFONT, reinterpret_cast<WPARAM>(outputFont_), TRUE);
  }

  projectPaneWidth_ = UiMetrics::ScalePx(hwnd_, kDefaultProjectPaneWidth);
  outputPaneHeight_ = UiMetrics::ScalePx(hwnd_, kDefaultOutputPaneHeight);
  toolbarHeight_ = ToolbarHeight();

  ApplyUiTheme();
  UpdateStatusBar();
}

void MainWindow::ApplyUiTheme()
{
  const AppUiTheme theme = UiTheme::Resolve(hwnd_);
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
  if (outputPaneHeader_)
  {
    InvalidateRect(outputPaneHeader_, nullptr, FALSE);
  }

  editorWorkspace_.ApplyThemeToAllEditors(theme);
  InvalidateTabs();
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
  if (activeDocumentIndex_ >= 0 && activeDocumentIndex_ < static_cast<int>(documents_.size()))
  {
    const EditorDocument &doc = documents_[activeDocumentIndex_];
    input.path = doc.path;
    if (!doc.languageOverride.empty())
    {
      input.languageName = doc.languageOverride;
    }
    else
    {
      input.languageName = editorWorkspace_.ActiveEditor().CurrentLanguageName();
    }
  }
  else
  {
    input.languageName = editorWorkspace_.ActiveEditor().CurrentLanguageName();
  }

  const StatusBarParts parts = StatusBarModel::FormatParts(input);
  SendMessageW(statusBar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(parts.left.c_str()));
  SendMessageW(statusBar_, SB_SETTEXTW, 1,
               reinterpret_cast<LPARAM>(parts.right.c_str()) | SBT_NOBORDERS);
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

  const std::vector<std::wstring> &items = recentFiles_.Items();
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
  editorWorkspace_.ForEachEditor([&](EditorView &editor) { editor.SetWordWrap(wordWrapEnabled_); });
  CheckMenuItem(GetMenu(hwnd_), kCmdViewWordWrap,
                MF_BYCOMMAND | (wordWrapEnabled_ ? MF_CHECKED : MF_UNCHECKED));
  editorSettings_.Save();
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
  MapWindowPoints(HWND_DESKTOP, hwnd_, reinterpret_cast<POINT *>(&statusRect), 2);
  client.bottom = statusRect.top;

  const int clientWidth = client.right - client.left;
  int contentHeight = client.bottom - client.top;
  if (contentHeight < 0)
  {
    contentHeight = 0;
  }

  toolbarHeight_ = ToolbarHeight();
  if (rebar_)
  {
    MoveWindow(rebar_, 0, 0, clientWidth, toolbarHeight_, TRUE);
    ShowWindow(rebar_, SW_SHOW);
    contentHeight -= toolbarHeight_;
  }

  int bottomHeight = showOutputPane_ ? outputPaneHeight_ : 0;
  int leftWidth = showProjectPane_ ? projectPaneWidth_ : 0;
  const int minProjectWidth = UiMetrics::ScalePx(hwnd_, kMinProjectPaneWidth);
  const int minOutputHeight = UiMetrics::ScalePx(hwnd_, kMinOutputPaneHeight);
  const int panePadding = UiMetrics::ScalePx(hwnd_, kPaneInnerPadding);
  const int paneGap = UiMetrics::ScalePx(hwnd_, kPaneControlGap);

  constexpr int minEditorWidth = 320;
  constexpr int minEditorHeight = 160;

  if (showProjectPane_ && leftWidth > 0)
  {
    leftWidth = (std::max)(leftWidth, minProjectWidth);
  }

  if (clientWidth - leftWidth < minEditorWidth)
  {
    leftWidth = clientWidth > minEditorWidth ? clientWidth - minEditorWidth : 0;
  }

  if (contentHeight - bottomHeight < minEditorHeight)
  {
    bottomHeight = contentHeight > minEditorHeight ? contentHeight - minEditorHeight : 0;
  }

  const int contentTop = toolbarHeight_;
  const int mainHeight = contentHeight > bottomHeight ? contentHeight - bottomHeight : 0;
  const int editorWidth = clientWidth > leftWidth ? clientWidth - leftWidth : 0;
  const int outputHeaderHeight = UiMetrics::ScalePx(hwnd_, kDefaultTabStripHeight);
  const int splitterThickness = UiMetrics::ScalePx(hwnd_, kSplitterHitWidth);

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

  // Sidebar spans the full height above the status bar; tab strip is
  // editor-column only.
  if (showProjectPane_ && leftWidth > kSidebarDividerWidth)
  {
    const int sidebarContentWidth = leftWidth - kSidebarDividerWidth;
    const int bodyTop = contentTop + projectPaneHeaderHeight_;
    const int bodyBottom = contentTop + contentHeight;
    const int bodyHeight = bodyBottom > bodyTop ? bodyBottom - bodyTop : 0;
    const int dividerBottom = showOutputPane_ && bottomHeight > minOutputHeight && editorWidth > 0
                                  ? contentTop + mainHeight
                                  : bodyBottom;
    const int dividerHeight = dividerBottom > bodyTop ? dividerBottom - bodyTop : 0;

    if (projectPaneHeader_)
    {
      MoveWindow(projectPaneHeader_, 0, contentTop, leftWidth, projectPaneHeaderHeight_, TRUE);
      ShowWindow(projectPaneHeader_, SW_SHOW);
    }
    const int filterHeight = UiMetrics::ScalePx(hwnd_, 24);
    const int controlX = panePadding;
    const int controlWidth = (std::max)(0, sidebarContentWidth - 2 * panePadding);
    const int filterTop = bodyTop + panePadding;
    if (projectFilterEdit_)
    {
      MoveWindow(projectFilterEdit_, controlX, filterTop, controlWidth, filterHeight, TRUE);
      ShowWindow(projectFilterEdit_, SW_SHOW);
    }
    if (projectTree_)
    {
      const int treeTop = filterTop + filterHeight + paneGap;
      const int treeBottom = bodyBottom - panePadding;
      const int adjustedTreeHeight = treeBottom > treeTop ? treeBottom - treeTop : 0;
      MoveWindow(projectTree_, controlX, treeTop, controlWidth, adjustedTreeHeight, TRUE);
      ShowWindow(projectTree_, SW_SHOW);
    }
    if (projectPaneDivider_)
    {
      MoveWindow(projectPaneDivider_, leftWidth - kSidebarDividerWidth, bodyTop,
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
    if (projectFilterEdit_)
    {
      ShowWindow(projectFilterEdit_, SW_HIDE);
    }
    if (projectPaneDivider_)
    {
      ShowWindow(projectPaneDivider_, SW_HIDE);
    }
  }

  const int editorLeft = leftWidth;
  const int editorPaneWidth = editorWidth > 0 ? editorWidth : clientWidth;
  const int outputLeft = showProjectPane_ && leftWidth > kSidebarDividerWidth
                             ? editorLeft - kSidebarDividerWidth
                             : editorLeft;
  const int outputPaneWidth = editorPaneWidth + (editorLeft - outputLeft);

  RECT workspaceBounds = {editorLeft, contentTop, editorLeft + editorPaneWidth,
                          contentTop + mainHeight};
  editorWorkspace_.SetBounds(workspaceBounds);

  if (showOutputPane_ && bottomHeight > minOutputHeight && editorPaneWidth > 0)
  {
    const int outputTop = contentTop + mainHeight;
    if (outputPaneHeader_)
    {
      MoveWindow(outputPaneHeader_, outputLeft, outputTop, outputPaneWidth, outputHeaderHeight,
                 TRUE);
      ShowWindow(outputPaneHeader_, SW_SHOW);
    }
    if (outputSplitter_)
    {
      MoveWindow(outputSplitter_, outputLeft, outputTop + outputHeaderHeight, outputPaneWidth,
                 splitterThickness, TRUE);
      ShowWindow(outputSplitter_, SW_SHOW);
    }
    const int paneTop = outputTop + outputHeaderHeight + splitterThickness;
    const int paneBottomInset = showProjectPane_ ? panePadding : 0;
    const int paneHeight = bottomHeight - outputHeaderHeight - splitterThickness - paneBottomInset;
    if (outputPane_ && paneHeight > 0)
    {
      MoveWindow(outputPane_, outputLeft, paneTop, outputPaneWidth, paneHeight, TRUE);
      ShowWindow(outputPane_, SW_SHOW);
    }
    else if (outputPane_)
    {
      ShowWindow(outputPane_, SW_HIDE);
    }
  }
  else
  {
    if (outputPaneHeader_)
    {
      ShowWindow(outputPaneHeader_, SW_HIDE);
    }
    if (outputSplitter_)
    {
      ShowWindow(outputSplitter_, SW_HIDE);
    }
    if (outputPane_)
    {
      ShowWindow(outputPane_, SW_HIDE);
    }
  }

  ApplyUiTheme();
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
  CheckMenuItem(GetMenu(hwnd_), kCmdViewProjectPane,
                MF_BYCOMMAND | (showProjectPane_ ? MF_CHECKED : MF_UNCHECKED));
  LayoutChildren();
}

void MainWindow::ToggleOutputPane()
{
  showOutputPane_ = !showOutputPane_;
  CheckMenuItem(GetMenu(hwnd_), kCmdViewOutputPane,
                MF_BYCOMMAND | (showOutputPane_ ? MF_CHECKED : MF_UNCHECKED));
  LayoutChildren();
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
    if (editorWorkspace_.DocumentReferenceCount(docIndex) > 1)
    {
      SaveCurrentDocumentState();
      if (editorWorkspace_.RemoveDocumentFromGroup(groupId, localTabIndex, documents_))
      {
        activeDocumentIndex_ = editorWorkspace_.ActiveDocumentIndex();
        UpdateStatusBar();
        UpdateActiveTabTitle();
        SyncTabBar();
        LayoutChildren();
        SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
      }
      return;
    }

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

void MainWindow::ToggleCodeFolding()
{
  codeFoldingEnabled_ = !codeFoldingEnabled_;
  editorWorkspace_.ForEachEditor(
      [&](EditorView &editor) { editor.EnableCodeFolding(codeFoldingEnabled_); });
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
  commandPalette_.Show(hwnd_, CommandRegistry::DefaultCommands(), [this](unsigned int commandId) {
    SendMessageW(hwnd_, WM_COMMAND, commandId, 0);
  });
}

void MainWindow::OnStatusBarClick(const NMHDR *header)
{
  UNREFERENCED_PARAMETER(header);
  if (!statusBar_)
  {
    return;
  }

  POINT pt = {};
  GetCursorPos(&pt);
  ScreenToClient(statusBar_, &pt);

  RECT rightPart = {};
  SendMessageW(statusBar_, SB_GETRECT, 1, reinterpret_cast<LPARAM>(&rightPart));
  if (PtInRect(&rightPart, pt))
  {
    ShowLanguageOverrideMenu();
  }
}

void MainWindow::ShowLanguageOverrideMenu()
{
  HMENU menu = CreatePopupMenu();
  if (!menu)
  {
    return;
  }

  std::vector<std::wstring> languageLabels;
  AppendMenuW(menu, MF_STRING, 1, L"(Auto-detect)");
  UINT commandId = 2;
  if (SyntaxRegistry::Shared().IsLoaded())
  {
    for (const SyntaxDefinition &definition : SyntaxRegistry::Shared().Definitions())
    {
      languageLabels.push_back(ConfigJson::Utf8ToWide(definition.filetype));
      AppendMenuW(menu, MF_STRING, commandId, languageLabels.back().c_str());
      ++commandId;
    }
  }

  POINT pt = {};
  GetCursorPos(&pt);
  const UINT selected =
      TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  if (selected == 0 || activeDocumentIndex_ < 0 ||
      activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument &doc = documents_[activeDocumentIndex_];
  if (selected == 1)
  {
    doc.languageOverride.clear();
    if (!doc.path.empty())
    {
      editorWorkspace_.ActiveEditor().ApplySyntaxForPath(doc.path);
    }
    else
    {
      editorWorkspace_.ActiveEditor().ApplySyntaxForLanguage(L"");
    }
  }
  else if (selected >= 2)
  {
    const size_t index = static_cast<size_t>(selected - 2);
    if (index < languageLabels.size())
    {
      doc.languageOverride = languageLabels[index];
      editorWorkspace_.ActiveEditor().ApplySyntaxForLanguage(doc.languageOverride);
    }
  }

  UpdateStatusBar();
}

void MainWindow::BeginProjectSplitterDrag(int clientX)
{
  draggingProjectSplitter_ = true;
  dragSplitterAnchor_ = clientX;
  dragSplitterStartSize_ = projectPaneWidth_;
  SetCapture(hwnd_);
}

void MainWindow::BeginOutputSplitterDrag(int clientY)
{
  draggingOutputSplitter_ = true;
  dragSplitterAnchor_ = clientY;
  dragSplitterStartSize_ = outputPaneHeight_;
  SetCapture(hwnd_);
}

void MainWindow::ContinueSplitterDrag(POINT clientPoint)
{
  if (draggingProjectSplitter_)
  {
    const int delta = clientPoint.x - dragSplitterAnchor_;
    projectPaneWidth_ =
        (std::max)(UiMetrics::ScalePx(hwnd_, kMinProjectPaneWidth), dragSplitterStartSize_ + delta);
    LayoutChildren();
    return;
  }

  if (draggingOutputSplitter_)
  {
    const int delta = dragSplitterAnchor_ - clientPoint.y;
    outputPaneHeight_ =
        (std::max)(UiMetrics::ScalePx(hwnd_, kMinOutputPaneHeight), dragSplitterStartSize_ + delta);
    LayoutChildren();
  }
}

void MainWindow::EndSplitterDrag()
{
  draggingProjectSplitter_ = false;
  draggingOutputSplitter_ = false;
  ReleaseCapture();
}
