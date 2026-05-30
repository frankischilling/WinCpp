#pragma once

#include <vector>

#include "WinCpp.h"

namespace TabBarLogic
{
// VS Code computeDropTarget: insert index from x within tab strip (client coords).
int HitTestInsertIndex(int clientX, int scrollOffset, const std::vector<RECT>& tabRects);

// Pinned tabs stay at the front; unpinned preserve relative order.
std::vector<int> SortIndicesPinnedFirst(const std::vector<int>& indices,
                                        const std::vector<bool>& pinned);
}
