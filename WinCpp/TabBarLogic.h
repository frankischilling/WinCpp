#pragma once

#include <vector>

#include "WinCpp.h"

namespace TabBarLogic
{
// VS Code computeDropTarget: insert index from x within tab strip (client coords).
int HitTestInsertIndex(int clientX, int scrollOffset, const std::vector<RECT>& tabRects);
}
