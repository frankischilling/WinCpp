#include "TabBarLogic.h"

namespace TabBarLogic
{
int HitTestInsertIndex(int clientX, int scrollOffset, const std::vector<RECT>& tabRects)
{
  if (tabRects.empty())
  {
    return 0;
  }

  const int adjustedX = clientX + scrollOffset;
  if (adjustedX < tabRects[0].left)
  {
    return 0;
  }

  for (size_t i = 0; i < tabRects.size(); ++i)
  {
    const RECT& rc = tabRects[i];
    if (adjustedX >= rc.left && adjustedX < rc.right)
    {
      const int mid = (rc.left + rc.right) / 2;
      return adjustedX < mid ? static_cast<int>(i) : static_cast<int>(i) + 1;
    }
  }

  return static_cast<int>(tabRects.size());
}

std::vector<int> SortIndicesPinnedFirst(const std::vector<int>& indices,
                                        const std::vector<bool>& pinned)
{
  std::vector<int> result;
  if (indices.size() != pinned.size())
  {
    return indices;
  }

  for (size_t i = 0; i < indices.size(); ++i)
  {
    if (pinned[i])
    {
      result.push_back(indices[i]);
    }
  }

  for (size_t i = 0; i < indices.size(); ++i)
  {
    if (!pinned[i])
    {
      result.push_back(indices[i]);
    }
  }

  return result;
}
}
