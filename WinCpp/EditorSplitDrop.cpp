#include "EditorSplitDrop.h"

namespace EditorSplitDrop
{
namespace
{
int ScaleLength(int value, int factorNumerator, int factorDenominator)
{
  return static_cast<int>(static_cast<long long>(value) * factorNumerator / factorDenominator);
}
}

EditorSplitDropResult HitTest(int mouseX,
                              int mouseY,
                              int editorWidth,
                              int editorHeight,
                              bool preferSplitVertically,
                              bool isDraggingGroup,
                              bool enableSplitting)
{
  EditorSplitDropResult result;

  if (!enableSplitting || editorWidth <= 0 || editorHeight <= 0)
  {
    return result;
  }

  // editorDropTarget.ts edgeWidthThresholdFactor / edgeHeightThresholdFactor
  int edgeWidthFactorPercent = 10;
  int edgeHeightFactorPercent = 10;
  if (isDraggingGroup)
  {
    if (preferSplitVertically)
    {
      edgeWidthFactorPercent = 30;
      edgeHeightFactorPercent = 10;
    }
    else
    {
      edgeWidthFactorPercent = 10;
      edgeHeightFactorPercent = 30;
    }
  }

  const int edgeWidthThreshold = ScaleLength(editorWidth, edgeWidthFactorPercent, 100);
  const int edgeHeightThreshold = ScaleLength(editorHeight, edgeHeightFactorPercent, 100);

  const int splitWidthThreshold = editorWidth / 3;
  const int splitHeightThreshold = editorHeight / 3;

  if (mouseX > edgeWidthThreshold && mouseX < editorWidth - edgeWidthThreshold &&
      mouseY > edgeHeightThreshold && mouseY < editorHeight - edgeHeightThreshold)
  {
    result.mergeTarget = true;
    return result;
  }

  if (preferSplitVertically)
  {
    if (mouseX < splitWidthThreshold)
    {
      result.direction = EditorSplitDirection::Left;
    }
    else if (mouseX > splitWidthThreshold * 2)
    {
      result.direction = EditorSplitDirection::Right;
    }
    else if (mouseY < editorHeight / 2)
    {
      result.direction = EditorSplitDirection::Up;
    }
    else
    {
      result.direction = EditorSplitDirection::Down;
    }
  }
  else
  {
    if (mouseY < splitHeightThreshold)
    {
      result.direction = EditorSplitDirection::Up;
    }
    else if (mouseY > splitHeightThreshold * 2)
    {
      result.direction = EditorSplitDirection::Down;
    }
    else if (mouseX < editorWidth / 2)
    {
      result.direction = EditorSplitDirection::Left;
    }
    else
    {
      result.direction = EditorSplitDirection::Right;
    }
  }

  return result;
}

EditorSplitDropResult HitTestScreenPoint(POINT screenPoint,
                                         const RECT& editorScreenBounds,
                                         bool preferSplitVertically,
                                         bool isDraggingGroup,
                                         bool enableSplitting)
{
  EditorSplitDropResult result;
  const int width = editorScreenBounds.right - editorScreenBounds.left;
  const int height = editorScreenBounds.bottom - editorScreenBounds.top;
  if (width <= 0 || height <= 0 || !PtInRect(&editorScreenBounds, screenPoint))
  {
    return result;
  }

  const int localX = screenPoint.x - editorScreenBounds.left;
  const int localY = screenPoint.y - editorScreenBounds.top;
  return HitTest(localX, localY, width, height, preferSplitVertically, isDraggingGroup,
                 enableSplitting);
}

RECT PreviewRect(const RECT& editorScreenBounds, EditorSplitDirection direction)
{
  RECT preview = editorScreenBounds;
  const int width = editorScreenBounds.right - editorScreenBounds.left;
  const int height = editorScreenBounds.bottom - editorScreenBounds.top;
  if (width <= 0 || height <= 0)
  {
    return preview;
  }

  switch (direction)
  {
    case EditorSplitDirection::Left:
      preview.right = editorScreenBounds.left + width / 2;
      break;
    case EditorSplitDirection::Right:
      preview.left = editorScreenBounds.left + width / 2;
      break;
    case EditorSplitDirection::Up:
      preview.bottom = editorScreenBounds.top + height / 2;
      break;
    case EditorSplitDirection::Down:
      preview.top = editorScreenBounds.top + height / 2;
      break;
    default:
      break;
  }

  return preview;
}

}  // namespace EditorSplitDrop
