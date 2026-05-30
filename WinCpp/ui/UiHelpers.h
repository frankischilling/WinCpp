#pragma once

#include "WinCpp.h"

// Shared message-box font (Segoe UI on modern Windows) for controls and dialogs.
HFONT GetUiFont();
void ReleaseUiFont();
void ApplyUiFont(HWND hwnd);
void ApplyUiFontToDescendants(HWND root);
void ApplyEditInnerMargins(HWND hwnd, int logicalMargin);

// Match the editor face/size to the UI font.
void ApplyEditorUiFont(HWND scintillaHwnd);

constexpr int kUiMargin = 16;
constexpr int kUiGap = 12;
constexpr int kUiButtonHeight = 28;
