#pragma once

#include "WinCpp.h"

// Dialog control IDs (shared with MainWindow dialog procs).
constexpr int kDlgFindLabel = 3011;
constexpr int kDlgReplaceLabel = 3012;
constexpr int kDlgFindEdit = 3001;
constexpr int kDlgReplaceEdit = 3002;
constexpr int kDlgMatchCase = 3003;
constexpr int kDlgWholeWord = 3004;
constexpr int kDlgFindNext = 3005;
constexpr int kDlgReplace = 3006;
constexpr int kDlgReplaceAll = 3007;
constexpr int kDlgClose = 3008;
constexpr int kDlgRegex = 3009;
constexpr int kDlgFindPrevious = 3010;

constexpr int kDlgGoToLineEdit = 3101;
constexpr int kDlgGoToOk = 3102;
constexpr int kDlgGoToCancel = 3103;
constexpr int kDlgGoToLabel = 3110;

constexpr int kDlgCreditsText = 3201;
constexpr int kDlgCreditsClose = 3202;

constexpr int kDlgRunCommandEdit = 3301;
constexpr int kDlgRunOk = 3302;
constexpr int kDlgRunCancel = 3303;
constexpr int kDlgRunLabel = 3310;

struct DialogMetrics
{
  int margin = 16;
  int gap = 12;
  int rowHeight = 28;
  int buttonHeight = 28;
  int checkHeight = 22;
  int labelWidth = 80;
  int editHeight = 28;
  int buttonWidth = 96;
  int closeButtonWidth = 96;
  int minButtonWidth = 72;
  int minEditWidth = 300;
};

struct DialogClientSize
{
  int width = 0;
  int height = 0;
};

struct DialogWindowSize
{
  int width = 0;
  int height = 0;
};

namespace DialogLayout
{
DialogMetrics GetMetrics(HWND hwnd);
DialogClientSize ComputeMinClientSizeFindReplace(bool replaceMode, const DialogMetrics& m);
DialogClientSize ComputeMinClientSizeFind(const DialogMetrics& m);
void LayoutFindDialog(HWND hwnd);
DialogClientSize ComputeMinClientSizeGoTo(const DialogMetrics& m);
DialogClientSize ComputeMinClientSizeRun(const DialogMetrics& m);
DialogWindowSize WindowSizeForClient(DWORD style, DWORD exStyle, int clientWidth, int clientHeight);
void LayoutFindReplaceDialog(HWND hwnd, bool replaceMode);
void LayoutGoToDialog(HWND hwnd);
void LayoutRunDialog(HWND hwnd);
void LayoutCreditsDialog(HWND hwnd);

// Match dialog client background; removes gray STATIC/EDIT fills.
LRESULT OnDialogCtlColor(UINT msg, WPARAM wParam, LPARAM lParam);

HWND CreateToolbarTooltipHost(HWND owner);
void AddToolbarTooltip(HWND tooltip, HWND toolbar, UINT commandId, const wchar_t* text);
void AttachToolbarTooltips(HWND toolbar, HWND tooltip);
}
