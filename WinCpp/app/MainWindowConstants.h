#pragma once

#include <windows.h>

constexpr wchar_t kMainWindowClassName[] = L"WinCppMainWindow";
constexpr int kDefaultTabStripHeight = 28;
constexpr int kStatusCaretWidth = 180;
#ifndef SB_SETBORDERS
#define SB_SETBORDERS (WM_USER + 6)
#endif
constexpr int kMinWindowWidth = 900;
constexpr int kMinWindowHeight = 500;

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
