#pragma once

#include "WinCpp.h"

// VS Code editorGroupsService.GroupDirection (editorDropTarget split zones).
enum class EditorSplitDirection
{
  None = 0,
  Left,
  Right,
  Up,
  Down,
};

// Hit-testing and preview geometry for editor tab drag-and-drop splits.
// Mirrors VS Code editorDropTarget.ts positionOverlay() (single-editor drag).
struct EditorSplitDropResult
{
  EditorSplitDirection direction = EditorSplitDirection::None;
  bool mergeTarget = false;
};

namespace EditorSplitDrop
{
// Mouse position in editor client coordinates (origin top-left of editor surface).
EditorSplitDropResult HitTest(int mouseX,
                              int mouseY,
                              int editorWidth,
                              int editorHeight,
                              bool preferSplitVertically = true,
                              bool isDraggingGroup = false,
                              bool enableSplitting = true);

EditorSplitDropResult HitTestScreenPoint(POINT screenPoint,
                                         const RECT& editorScreenBounds,
                                         bool preferSplitVertically = true,
                                         bool isDraggingGroup = false,
                                         bool enableSplitting = true);

RECT PreviewRect(const RECT& editorScreenBounds, EditorSplitDirection direction);

}  // namespace EditorSplitDrop
