#pragma once

#include "WinCpp.h"

struct UiThemeChrome
{
  COLORREF stripBackground = 0;
  COLORREF inactiveBackground = 0;
  COLORREF activeBackground = 0;
  COLORREF inactiveForeground = 0;
  COLORREF activeForeground = 0;
  COLORREF separator = 0;
  COLORREF hoverBackground = 0;
  COLORREF bottomBorder = 0;
  COLORREF sidebarHeaderBackground = 0;
  COLORREF sidebarHeaderForeground = 0;
  COLORREF sidebarBorder = 0;
  COLORREF accent = 0;
  COLORREF dropFill = 0;
  COLORREF dropBorder = 0;
  COLORREF dropMergeFill = 0;
  COLORREF dropMergeBorder = 0;
  COLORREF dragGhostBack = 0;
  COLORREF dragGhostText = 0;
};

struct UiThemeEditor
{
  COLORREF defaultFore = 0;
  COLORREF defaultBack = 0;
  COLORREF lineNumberFore = 0;
  COLORREF lineNumberBack = 0;
  COLORREF caretLineBack = 0;
  COLORREF findIndicatorFore = 0;
};

struct UiThemeSyntax
{
  COLORREF comment = 0;
  COLORREF stringColor = 0;
  COLORREF number = 0;
  COLORREF keyword = 0;
  COLORREF type = 0;
  COLORREF preproc = 0;
  COLORREF operatorColor = 0;
  COLORREF identifier = 0;
};

struct AppUiTheme
{
  bool isDark = false;
  UiThemeChrome chrome;
  UiThemeEditor editor;
  UiThemeSyntax syntax;
};

namespace UiTheme
{
bool IsDarkUi(HWND hwnd = nullptr);
AppUiTheme Resolve(HWND hwnd = nullptr);
}
