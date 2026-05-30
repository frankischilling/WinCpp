#include "UiHelpers.h"

#include <Scintilla.h>
#include <vector>

namespace
{
HFONT g_uiFont = nullptr;
}

HFONT GetUiFont()
{
  if (!g_uiFont)
  {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    g_uiFont = CreateFontIndirectW(&metrics.lfMessageFont);
  }

  return g_uiFont;
}

void ReleaseUiFont()
{
  if (g_uiFont)
  {
    DeleteObject(g_uiFont);
    g_uiFont = nullptr;
  }
}

void ApplyUiFont(HWND hwnd)
{
  if (hwnd && GetUiFont())
  {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetUiFont()), TRUE);
  }
}

void ApplyUiFontToDescendants(HWND root)
{
  if (!root)
  {
    return;
  }

  ApplyUiFont(root);
  for (HWND child = GetWindow(root, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
  {
    ApplyUiFontToDescendants(child);
  }
}

void ApplyEditorUiFont(HWND scintillaHwnd)
{
  if (!scintillaHwnd)
  {
    return;
  }

  NONCLIENTMETRICSW metrics = {};
  metrics.cbSize = sizeof(metrics);
  if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
  {
    return;
  }

  const int pointSize = metrics.lfMessageFont.lfHeight < 0
                            ? -metrics.lfMessageFont.lfHeight
                            : metrics.lfMessageFont.lfHeight;

  std::vector<char> faceUtf8;
  const int faceLen = WideCharToMultiByte(CP_UTF8, 0, metrics.lfMessageFont.lfFaceName, -1, nullptr, 0,
                                          nullptr, nullptr);
  if (faceLen > 0)
  {
    faceUtf8.assign(static_cast<size_t>(faceLen), '\0');
    WideCharToMultiByte(CP_UTF8, 0, metrics.lfMessageFont.lfFaceName, -1, faceUtf8.data(), faceLen,
                        nullptr, nullptr);
    SendMessage(scintillaHwnd, SCI_STYLESETFONT, STYLE_DEFAULT,
                reinterpret_cast<LPARAM>(faceUtf8.data()));
  }

  SendMessage(scintillaHwnd, SCI_STYLESETSIZE, STYLE_DEFAULT, pointSize);
}
