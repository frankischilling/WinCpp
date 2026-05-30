#include "MainWindow.h"

#include "MainWindowConstants.h"
#include "MainWindowInternal.h"
#include "UiHelpers.h"

#include <commctrl.h>

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

      projectPaneWidth_(kDefaultProjectPaneWidth),

      projectPaneHeaderHeight_(0),

      tabStripHeight_(kDefaultTabStripHeight),

      outputPaneHeight_(kDefaultOutputPaneHeight),

      toolbarHeight_(0),

      showProjectPane_(true),

      showOutputPane_(true),

      wordWrapEnabled_(false),

      codeFoldingEnabled_(false),

      draggingProjectSplitter_(false),

      draggingOutputSplitter_(false),

      dragSplitterAnchor_(0),

      dragSplitterStartSize_(0),

      outputPaneHeader_(nullptr),

      outputSplitter_(nullptr),

      rebar_(nullptr),

      toolbar_(nullptr),

      toolbarTooltip_(nullptr)

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

bool MainWindow::TranslateAcceleratorMessage(MSG *msg)

{

  if (!accelerators_ || !hwnd_)

  {

    return false;
  }

  return TranslateAcceleratorW(hwnd_, accelerators_, msg) != 0;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)

{

  MainWindow *self = nullptr;

  if (msg == WM_NCCREATE)

  {

    auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);

    self = reinterpret_cast<MainWindow *>(create->lpCreateParams);

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

    self->hwnd_ = hwnd;
  }

  else

  {

    self = reinterpret_cast<MainWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self)

  {

    return self->HandleMessage(msg, wParam, lParam);
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
