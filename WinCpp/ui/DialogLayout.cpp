#include "DialogLayout.h"

#include "UiHelpers.h"
#include "UiMetrics.h"

#include <algorithm>
#include <commctrl.h>

namespace
{
void MoveCtrl(HWND hwnd, int x, int y, int width, int height)
{
  if (hwnd)
  {
    MoveWindow(hwnd, x, y, (std::max)(width, 0), (std::max)(height, 0), TRUE);
  }
}

int GetIdealControlWidth(HWND hwnd, int fallbackWidth)
{
  if (!hwnd)
  {
    return fallbackWidth;
  }

#if defined(BCM_GETIDEALSIZE)
  SIZE size = {};
  const LRESULT result = SendMessageW(hwnd, BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&size));
  if (result && size.cx > 0)
  {
    return size.cx;
  }
#endif

  return fallbackWidth;
}

int LabelGap(const DialogMetrics& m)
{
  return (std::max)(1, m.gap - m.gap / 3);
}

int FindReplaceLabelWidth(const DialogMetrics& m)
{
  return (std::max)(m.minButtonWidth - m.gap - m.gap / 3, m.labelWidth - m.gap * 2);
}

int FindLabelWidth(const DialogMetrics& m)
{
  return m.gap * 3 + m.gap / 3;
}

int FindEditWidth(const DialogMetrics& m)
{
  return m.minEditWidth;
}

int ReplaceEditWidth(const DialogMetrics& m)
{
  return m.minEditWidth + m.buttonWidth;
}

int ButtonGap(const DialogMetrics& m)
{
  return (std::max)(1, m.gap - m.gap / 3);
}

int SectionGap(const DialogMetrics& m)
{
  return m.gap + m.gap / 3;
}

int CheckMatchCaseWidth(const DialogMetrics& m)
{
  return m.buttonWidth;
}

int CheckWholeWordWidth(const DialogMetrics& m)
{
  return m.buttonWidth + m.gap;
}

int CheckRegexWidth(const DialogMetrics& m)
{
  return (std::max)(m.minButtonWidth, m.buttonWidth - m.gap * 2);
}

int CheckRowWidth(const DialogMetrics& m)
{
  return CheckMatchCaseWidth(m) + CheckWholeWordWidth(m) + CheckRegexWidth(m) +
         2 * ButtonGap(m);
}

int ButtonStripWidth(const DialogMetrics& m, int count)
{
  if (count <= 0)
  {
    return 0;
  }
  return count * m.buttonWidth + (count - 1) * ButtonGap(m);
}

int SingleFieldLabelWidth(const DialogMetrics& m)
{
  return m.labelWidth + m.gap * 2 + m.gap * 2 / 3;
}

int SingleFieldRightPadding(const DialogMetrics& m)
{
  return m.gap;
}

DialogClientSize ComputeSingleFieldClientSize(const DialogMetrics& m, int editWidth)
{
  DialogClientSize size;
  const int labelGap = LabelGap(m);
  const int fieldWidth =
      SingleFieldLabelWidth(m) + labelGap + editWidth + SingleFieldRightPadding(m);
  const int buttonWidth = 2 * m.buttonWidth + m.gap;
  size.width = 2 * m.margin + (std::max)(fieldWidth, buttonWidth);
  size.height = m.margin + m.rowHeight + SectionGap(m) + m.buttonHeight + m.margin;
  return size;
}

void LayoutBottomButtonsCenteredUnderRect(const DialogMetrics& m, int buttonY, const RECT& anchor,
                                          HWND primaryButton, HWND secondaryButton)
{
  const int buttonStripWidth = 2 * m.buttonWidth + m.gap;
  const int anchorWidth = anchor.right - anchor.left;
  int buttonX = anchor.left + (anchorWidth - buttonStripWidth) / 2;
  buttonX = (std::max)(buttonX, m.margin);

  MoveCtrl(primaryButton, buttonX, buttonY, m.buttonWidth, m.buttonHeight);
  MoveCtrl(secondaryButton, buttonX + m.buttonWidth + m.gap, buttonY, m.buttonWidth,
           m.buttonHeight);
}

void LayoutFindReplaceControls(HWND hwnd, bool replaceMode)
{
  const DialogMetrics m = DialogLayout::GetMetrics(hwnd);
  RECT client = {};
  GetClientRect(hwnd, &client);

  const int labelGap = LabelGap(m);
  const int checkGap = ButtonGap(m);
  const int buttonGap = ButtonGap(m);
  const int sectionGap = SectionGap(m);
  const int labelWidth = replaceMode ? FindReplaceLabelWidth(m) : FindLabelWidth(m);
  const int contentLeft = m.margin + labelWidth + labelGap;
  const int contentRight = static_cast<int>(client.right) - m.margin;
  const int targetEditWidth = replaceMode ? ReplaceEditWidth(m) : FindEditWidth(m);
  const int editWidth = (std::max)(0, (std::min)(contentRight - contentLeft, targetEditWidth));

  int y = m.margin;

  MoveCtrl(GetDlgItem(hwnd, kDlgFindLabel), m.margin, y, labelWidth, m.rowHeight);
  MoveCtrl(GetDlgItem(hwnd, kDlgFindEdit), contentLeft, y, editWidth, m.editHeight);
  y += m.rowHeight;

  HWND replaceLabel = GetDlgItem(hwnd, kDlgReplaceLabel);
  HWND replaceEdit = GetDlgItem(hwnd, kDlgReplaceEdit);
  if (replaceMode)
  {
    y += m.gap;
    ShowWindow(replaceLabel, SW_SHOW);
    ShowWindow(replaceEdit, SW_SHOW);
    MoveCtrl(replaceLabel, m.margin, y, labelWidth, m.rowHeight);
    MoveCtrl(replaceEdit, contentLeft, y, editWidth, m.editHeight);
    y += m.rowHeight;
  }
  else
  {
    ShowWindow(replaceLabel, SW_HIDE);
    ShowWindow(replaceEdit, SW_HIDE);
  }

  y += m.gap;
  int checkX = contentLeft;
  const int matchWidth = GetIdealControlWidth(GetDlgItem(hwnd, kDlgMatchCase), CheckMatchCaseWidth(m));
  const int wholeWordWidth =
      GetIdealControlWidth(GetDlgItem(hwnd, kDlgWholeWord), CheckWholeWordWidth(m));
  const int regexWidth = GetIdealControlWidth(GetDlgItem(hwnd, kDlgRegex), CheckRegexWidth(m));

  MoveCtrl(GetDlgItem(hwnd, kDlgMatchCase), checkX, y, matchWidth, m.checkHeight);
  checkX += matchWidth + checkGap;
  MoveCtrl(GetDlgItem(hwnd, kDlgWholeWord), checkX, y, wholeWordWidth, m.checkHeight);
  checkX += wholeWordWidth + checkGap;
  MoveCtrl(GetDlgItem(hwnd, kDlgRegex), checkX, y, regexWidth, m.checkHeight);
  y += m.checkHeight + sectionGap;

  HWND replaceBtn = GetDlgItem(hwnd, kDlgReplace);
  HWND replaceAllBtn = GetDlgItem(hwnd, kDlgReplaceAll);
  ShowWindow(replaceBtn, replaceMode ? SW_SHOW : SW_HIDE);
  ShowWindow(replaceAllBtn, replaceMode ? SW_SHOW : SW_HIDE);

  const int buttonCount = replaceMode ? 5 : 3;
  const int buttonStripWidth = ButtonStripWidth(m, buttonCount);
  const int minButtonY = y;
  const int bottomButtonY = static_cast<int>(client.bottom) - m.margin - m.buttonHeight;
  const int buttonY = (std::max)(minButtonY, bottomButtonY);
  int buttonX = static_cast<int>(client.right) - m.margin - buttonStripWidth;
  if (!replaceMode)
  {
    buttonX = contentLeft + (editWidth - buttonStripWidth) / 2;
    buttonX = (std::min)(buttonX, static_cast<int>(client.right) - m.margin - buttonStripWidth);
  }
  buttonX = (std::max)(buttonX, m.margin);

  MoveCtrl(GetDlgItem(hwnd, kDlgFindPrevious), buttonX, buttonY, m.buttonWidth, m.buttonHeight);
  buttonX += m.buttonWidth + buttonGap;
  MoveCtrl(GetDlgItem(hwnd, kDlgFindNext), buttonX, buttonY, m.buttonWidth, m.buttonHeight);
  buttonX += m.buttonWidth + buttonGap;
  if (replaceMode)
  {
    MoveCtrl(replaceBtn, buttonX, buttonY, m.buttonWidth, m.buttonHeight);
    buttonX += m.buttonWidth + buttonGap;
    MoveCtrl(replaceAllBtn, buttonX, buttonY, m.buttonWidth, m.buttonHeight);
    buttonX += m.buttonWidth + buttonGap;
  }
  MoveCtrl(GetDlgItem(hwnd, kDlgClose), buttonX, buttonY, m.buttonWidth, m.buttonHeight);
}
}

namespace DialogLayout
{
DialogMetrics GetMetrics(HWND hwnd)
{
  DialogMetrics m;
  m.margin = UiMetrics::ScalePx(hwnd, kUiMargin);
  m.gap = UiMetrics::ScalePx(hwnd, kUiGap);
  m.rowHeight = UiMetrics::ScalePx(hwnd, 28);
  m.buttonHeight = UiMetrics::ScalePx(hwnd, kUiButtonHeight);
  m.checkHeight = UiMetrics::ScalePx(hwnd, 22);
  m.labelWidth = UiMetrics::ScalePx(hwnd, 80);
  m.editHeight = UiMetrics::ScalePx(hwnd, kUiButtonHeight);
  m.buttonWidth = UiMetrics::ScalePx(hwnd, 96);
  m.closeButtonWidth = UiMetrics::ScalePx(hwnd, 96);
  m.minButtonWidth = UiMetrics::ScalePx(hwnd, 72);
  m.minEditWidth = UiMetrics::ScalePx(hwnd, 300);
  return m;
}

DialogClientSize ComputeMinClientSizeFind(const DialogMetrics& m)
{
  DialogClientSize size;
  const int labelWidth = FindLabelWidth(m);
  const int fieldRowWidth = labelWidth + LabelGap(m) + FindEditWidth(m);
  const int checkRowWidth = labelWidth + LabelGap(m) + CheckRowWidth(m);
  const int buttonStripWidth = ButtonStripWidth(m, 3);
  const int contentWidth = (std::max)({fieldRowWidth, checkRowWidth, buttonStripWidth});

  size.width = 2 * m.margin + contentWidth;
  size.height =
      m.margin + m.rowHeight + m.gap + m.checkHeight + SectionGap(m) + m.buttonHeight + m.margin;
  return size;
}

DialogClientSize ComputeMinClientSizeFindReplace(bool replaceMode, const DialogMetrics& m)
{
  if (!replaceMode)
  {
    return ComputeMinClientSizeFind(m);
  }

  DialogClientSize size;
  const int labelWidth = FindReplaceLabelWidth(m);
  const int fieldRowWidth = labelWidth + LabelGap(m) + ReplaceEditWidth(m);
  const int checkRowWidth = labelWidth + LabelGap(m) + CheckRowWidth(m);
  const int buttonStripWidth = ButtonStripWidth(m, 5);
  const int contentWidth = (std::max)({fieldRowWidth, checkRowWidth, buttonStripWidth});

  size.width = 2 * m.margin + contentWidth;
  size.height = m.margin + m.rowHeight + m.gap + m.rowHeight + m.gap + m.checkHeight +
                SectionGap(m) + m.buttonHeight + m.margin;
  return size;
}

DialogClientSize ComputeMinClientSizeGoTo(const DialogMetrics& m)
{
  return ComputeSingleFieldClientSize(m, m.minEditWidth);
}

DialogClientSize ComputeMinClientSizeRun(const DialogMetrics& m)
{
  return ComputeSingleFieldClientSize(m, m.minEditWidth + m.buttonWidth);
}

DialogWindowSize WindowSizeForClient(DWORD style, DWORD exStyle, int clientWidth, int clientHeight)
{
  RECT rect = {0, 0, clientWidth, clientHeight};
  AdjustWindowRectEx(&rect, style, FALSE, exStyle);
  DialogWindowSize size;
  size.width = rect.right - rect.left;
  size.height = rect.bottom - rect.top;
  return size;
}

void LayoutFindDialog(HWND hwnd)
{
  LayoutFindReplaceControls(hwnd, false);
}

void LayoutFindReplaceDialog(HWND hwnd, bool replaceMode)
{
  LayoutFindReplaceControls(hwnd, replaceMode);
}

void LayoutGoToDialog(HWND hwnd)
{
  const DialogMetrics m = GetMetrics(hwnd);
  RECT client = {};
  GetClientRect(hwnd, &client);

  const int labelWidth = SingleFieldLabelWidth(m);
  const int labelGap = LabelGap(m);
  const int editX = m.margin + labelWidth + labelGap;
  const int editRight = static_cast<int>(client.right) - m.margin - SingleFieldRightPadding(m);
  const int editWidth = (std::max)(0, editRight - editX);
  const int y = m.margin;

  MoveCtrl(GetDlgItem(hwnd, kDlgGoToLabel), m.margin, y, labelWidth, m.rowHeight);
  MoveCtrl(GetDlgItem(hwnd, kDlgGoToLineEdit), editX, y, editWidth, m.editHeight);

  const int buttonY = client.bottom - m.margin - m.buttonHeight;
  RECT editRect = {editX, y, editX + editWidth, y + m.editHeight};
  LayoutBottomButtonsCenteredUnderRect(m, buttonY, editRect, GetDlgItem(hwnd, kDlgGoToOk),
                                       GetDlgItem(hwnd, kDlgGoToCancel));
}

void LayoutRunDialog(HWND hwnd)
{
  const DialogMetrics m = GetMetrics(hwnd);
  RECT client = {};
  GetClientRect(hwnd, &client);

  const int labelWidth = SingleFieldLabelWidth(m);
  const int labelGap = LabelGap(m);
  const int editX = m.margin + labelWidth + labelGap;
  const int editRight = static_cast<int>(client.right) - m.margin - SingleFieldRightPadding(m);
  const int editWidth = (std::max)(0, editRight - editX);
  const int y = m.margin;

  MoveCtrl(GetDlgItem(hwnd, kDlgRunLabel), m.margin, y, labelWidth, m.rowHeight);
  MoveCtrl(GetDlgItem(hwnd, kDlgRunCommandEdit), editX, y, editWidth, m.editHeight);

  const int buttonY = client.bottom - m.margin - m.buttonHeight;
  RECT editRect = {editX, y, editX + editWidth, y + m.editHeight};
  LayoutBottomButtonsCenteredUnderRect(m, buttonY, editRect, GetDlgItem(hwnd, kDlgRunOk),
                                       GetDlgItem(hwnd, kDlgRunCancel));
}

LRESULT OnDialogCtlColor(UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg != WM_CTLCOLORSTATIC && msg != WM_CTLCOLOREDIT)
  {
    return 0;
  }

  (void)lParam;
  HDC hdc = reinterpret_cast<HDC>(wParam);
  SetBkMode(hdc, OPAQUE);
  SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
  SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
  return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
}

void LayoutCreditsDialog(HWND hwnd)
{
  const DialogMetrics m = GetMetrics(hwnd);
  RECT client = {};
  GetClientRect(hwnd, &client);

  const int contentWidth = client.right - 2 * m.margin;
  const int buttonY = client.bottom - m.margin - m.buttonHeight;
  const int editHeight = buttonY - m.gap - m.margin;

  MoveCtrl(GetDlgItem(hwnd, kDlgCreditsText), m.margin, m.margin, contentWidth,
           (std::max)(editHeight, m.rowHeight));
  MoveCtrl(GetDlgItem(hwnd, kDlgCreditsClose), client.right - m.margin - m.closeButtonWidth, buttonY,
           m.closeButtonWidth, m.buttonHeight);
}

HWND CreateToolbarTooltipHost(HWND owner)
{
  return CreateWindowExW(
      WS_EX_TOPMOST,
      TOOLTIPS_CLASSW,
      nullptr,
      TTS_NOPREFIX | TTS_ALWAYSTIP,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      owner,
      nullptr,
      GetModuleHandleW(nullptr),
      nullptr);
}

void AddToolbarTooltip(HWND tooltip, HWND toolbar, UINT commandId, const wchar_t* text)
{
  if (!tooltip || !toolbar || !text)
  {
    return;
  }

  TTTOOLINFOW info = {};
  info.cbSize = sizeof(TTTOOLINFOW);
  info.uFlags = TTF_SUBCLASS;
  info.hwnd = toolbar;
  info.uId = commandId;
  info.lpszText = const_cast<wchar_t*>(text);
  SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
}

void AttachToolbarTooltips(HWND toolbar, HWND tooltip)
{
  if (toolbar && tooltip)
  {
    SendMessageW(toolbar, TB_SETTOOLTIPS, reinterpret_cast<WPARAM>(tooltip), 0);
  }
}
}
