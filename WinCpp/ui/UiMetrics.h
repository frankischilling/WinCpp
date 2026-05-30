#pragma once

#include "WinCpp.h"

namespace UiMetrics
{
UINT GetDpi(HWND hwnd);
int ScalePx(HWND hwnd, int logical96);
int ScalePx(UINT dpi, int logical96);
}
