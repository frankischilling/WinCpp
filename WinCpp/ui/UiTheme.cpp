#include "UiTheme.h"

namespace
{
int Luminance(COLORREF color)
{
  return static_cast<int>(GetRValue(color)) + GetGValue(color) + GetBValue(color);
}
}

namespace UiTheme
{
bool IsDarkUi(HWND hwnd)
{
  UNREFERENCED_PARAMETER(hwnd);
  return Luminance(GetSysColor(COLOR_WINDOW)) < 380;
}

AppUiTheme Resolve(HWND hwnd)
{
  UNREFERENCED_PARAMETER(hwnd);
  AppUiTheme theme;
  theme.isDark = IsDarkUi(hwnd);

  if (theme.isDark)
  {
    theme.chrome.stripBackground = GetSysColor(COLOR_3DFACE);
    theme.chrome.inactiveBackground = RGB(55, 55, 55);
    theme.chrome.activeBackground = RGB(30, 30, 30);
    theme.chrome.inactiveForeground = RGB(170, 170, 170);
    theme.chrome.activeForeground = RGB(255, 255, 255);
    theme.chrome.separator = GetSysColor(COLOR_BTNSHADOW);
    theme.chrome.hoverBackground = RGB(70, 70, 70);
    theme.chrome.bottomBorder = GetSysColor(COLOR_3DSHADOW);
    theme.chrome.sidebarHeaderBackground = GetSysColor(COLOR_3DFACE);
    theme.chrome.sidebarHeaderForeground = GetSysColor(COLOR_BTNTEXT);
    theme.chrome.sidebarBorder = GetSysColor(COLOR_3DSHADOW);
    theme.chrome.accent = GetSysColor(COLOR_HIGHLIGHT);
    theme.chrome.dropFill = RGB(60, 60, 90);
    theme.chrome.dropBorder = GetSysColor(COLOR_HOTLIGHT);
    theme.chrome.dropMergeFill = RGB(70, 70, 70);
    theme.chrome.dropMergeBorder = GetSysColor(COLOR_GRAYTEXT);
    theme.chrome.dragGhostBack = GetSysColor(COLOR_HIGHLIGHT);
    theme.chrome.dragGhostText = GetSysColor(COLOR_HIGHLIGHTTEXT);

    theme.editor.defaultFore = RGB(220, 220, 220);
    theme.editor.defaultBack = RGB(30, 30, 30);
    theme.editor.lineNumberFore = RGB(140, 140, 140);
    theme.editor.lineNumberBack = RGB(40, 40, 40);
    theme.editor.caretLineBack = RGB(45, 45, 45);
    theme.editor.findIndicatorFore = RGB(120, 90, 40);

    theme.syntax.comment = RGB(87, 166, 74);
    theme.syntax.stringColor = RGB(214, 157, 133);
    theme.syntax.number = RGB(181, 206, 168);
    theme.syntax.keyword = RGB(86, 156, 214);
    theme.syntax.type = RGB(78, 201, 176);
    theme.syntax.preproc = RGB(155, 155, 155);
    theme.syntax.operatorColor = RGB(220, 220, 220);
    theme.syntax.identifier = RGB(220, 220, 220);
  }
  else
  {
    theme.chrome.stripBackground = GetSysColor(COLOR_3DFACE);
    theme.chrome.inactiveBackground = GetSysColor(COLOR_BTNFACE);
    theme.chrome.activeBackground = GetSysColor(COLOR_WINDOW);
    theme.chrome.inactiveForeground = GetSysColor(COLOR_GRAYTEXT);
    theme.chrome.activeForeground = GetSysColor(COLOR_WINDOWTEXT);
    theme.chrome.separator = GetSysColor(COLOR_BTNSHADOW);
    theme.chrome.hoverBackground = GetSysColor(COLOR_BTNFACE);
    theme.chrome.bottomBorder = GetSysColor(COLOR_3DSHADOW);
    theme.chrome.sidebarHeaderBackground = GetSysColor(COLOR_3DFACE);
    theme.chrome.sidebarHeaderForeground = GetSysColor(COLOR_BTNTEXT);
    theme.chrome.sidebarBorder = GetSysColor(COLOR_3DSHADOW);
    theme.chrome.accent = GetSysColor(COLOR_HIGHLIGHT);
    theme.chrome.dropFill = RGB(173, 214, 255);
    theme.chrome.dropBorder = GetSysColor(COLOR_HOTLIGHT);
    theme.chrome.dropMergeFill = RGB(220, 220, 220);
    theme.chrome.dropMergeBorder = GetSysColor(COLOR_GRAYTEXT);
    theme.chrome.dragGhostBack = GetSysColor(COLOR_HIGHLIGHT);
    theme.chrome.dragGhostText = GetSysColor(COLOR_HIGHLIGHTTEXT);

    theme.editor.defaultFore = RGB(32, 32, 32);
    theme.editor.defaultBack = RGB(255, 255, 255);
    theme.editor.lineNumberFore = RGB(96, 96, 96);
    theme.editor.lineNumberBack = RGB(248, 248, 248);
    theme.editor.caretLineBack = RGB(242, 242, 242);
    theme.editor.findIndicatorFore = RGB(255, 230, 150);

    theme.syntax.comment = RGB(0, 128, 0);
    theme.syntax.stringColor = RGB(163, 21, 21);
    theme.syntax.number = RGB(9, 51, 166);
    theme.syntax.keyword = RGB(0, 0, 160);
    theme.syntax.type = RGB(43, 145, 175);
    theme.syntax.preproc = RGB(128, 64, 0);
    theme.syntax.operatorColor = RGB(0, 0, 0);
    theme.syntax.identifier = RGB(0, 0, 0);
  }

  return theme;
}
}
