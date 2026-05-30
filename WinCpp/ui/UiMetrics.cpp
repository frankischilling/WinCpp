#include "UiMetrics.h"

namespace UiMetrics
{
UINT GetDpi(HWND hwnd)
{
  if (hwnd)
  {
    return GetDpiForWindow(hwnd);
  }
  HDC hdc = GetDC(nullptr);
  const int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
  if (hdc)
  {
    ReleaseDC(nullptr, hdc);
  }
  return static_cast<UINT>(dpi > 0 ? dpi : 96);
}

int ScalePx(HWND hwnd, int logical96)
{
  return MulDiv(logical96, static_cast<int>(GetDpi(hwnd)), 96);
}

int ScalePx(UINT dpi, int logical96)
{
  return MulDiv(logical96, static_cast<int>(dpi), 96);
}
}
