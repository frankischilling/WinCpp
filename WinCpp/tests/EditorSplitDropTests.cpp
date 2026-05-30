#include <gtest/gtest.h>

#include "EditorSplitDrop.h"

namespace
{
EditorSplitDropResult Hit(int x, int y, int w = 900, int h = 600, bool vertical = true)
{
  return EditorSplitDrop::HitTest(x, y, w, h, vertical, false, true);
}
}

TEST(EditorSplitDrop, CenterIsMergeTarget)
{
  const auto r = Hit(450, 300);
  EXPECT_TRUE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::None);
}

TEST(EditorSplitDrop, LeftThirdSplitsLeft)
{
  const auto r = Hit(50, 300);
  EXPECT_FALSE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::Left);
}

TEST(EditorSplitDrop, RightThirdSplitsRight)
{
  const auto r = Hit(850, 300);
  EXPECT_FALSE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::Right);
}

TEST(EditorSplitDrop, MiddleThirdTopSplitsUp)
{
  const auto r = Hit(450, 50);
  EXPECT_FALSE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::Up);
}

TEST(EditorSplitDrop, MiddleThirdBottomSplitsDown)
{
  const auto r = Hit(450, 550);
  EXPECT_FALSE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::Down);
}

TEST(EditorSplitDrop, HorizontalPreferenceUsesRowThirds)
{
  const auto top = EditorSplitDrop::HitTest(450, 50, 900, 600, false, false, true);
  EXPECT_EQ(top.direction, EditorSplitDirection::Up);

  const auto bottom = EditorSplitDrop::HitTest(450, 550, 900, 600, false, false, true);
  EXPECT_EQ(bottom.direction, EditorSplitDirection::Down);

  // Outside the center merge band on the left edge strip.
  const auto left = EditorSplitDrop::HitTest(50, 300, 900, 600, false, false, true);
  EXPECT_EQ(left.direction, EditorSplitDirection::Left);
}

TEST(EditorSplitDrop, PreviewRectHalfPane)
{
  const RECT editor{100, 100, 500, 400};
  const RECT left = EditorSplitDrop::PreviewRect(editor, EditorSplitDirection::Left);
  EXPECT_EQ(left.left, 100);
  EXPECT_EQ(left.right, 300);
  EXPECT_EQ(left.top, 100);
  EXPECT_EQ(left.bottom, 400);

  const RECT down = EditorSplitDrop::PreviewRect(editor, EditorSplitDirection::Down);
  EXPECT_EQ(down.top, 250);
  EXPECT_EQ(down.bottom, 400);
}

TEST(EditorSplitDrop, ScreenPointOutsideEditorReturnsEmpty)
{
  const RECT editor{100, 100, 500, 400};
  const POINT outside{50, 50};
  const auto r = EditorSplitDrop::HitTestScreenPoint(outside, editor, true, false, true);
  EXPECT_FALSE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::None);
}

TEST(EditorSplitDrop, DisabledSplittingReturnsEmpty)
{
  const auto r = EditorSplitDrop::HitTest(50, 300, 900, 600, true, false, false);
  EXPECT_FALSE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::None);
}

TEST(EditorSplitDrop, ZeroSizeEditorReturnsEmpty)
{
  const auto r = EditorSplitDrop::HitTest(0, 0, 0, 600, true, false, true);
  EXPECT_FALSE(r.mergeTarget);
  EXPECT_EQ(r.direction, EditorSplitDirection::None);
}

TEST(EditorSplitDrop, GroupDragUsesWiderHorizontalEdges)
{
  const auto normal = EditorSplitDrop::HitTest(200, 300, 900, 600, true, false, true);
  EXPECT_TRUE(normal.mergeTarget);

  const auto group = EditorSplitDrop::HitTest(200, 300, 900, 600, true, true, true);
  EXPECT_FALSE(group.mergeTarget);
  EXPECT_EQ(group.direction, EditorSplitDirection::Left);
}

TEST(EditorSplitDrop, PreviewRectMergeUsesFullEditorBounds)
{
  const RECT editor{10, 20, 410, 320};
  const RECT preview =
      EditorSplitDrop::PreviewRect(editor, EditorSplitDirection::None);
  EXPECT_EQ(preview.left, editor.left);
  EXPECT_EQ(preview.right, editor.right);
  EXPECT_EQ(preview.top, editor.top);
  EXPECT_EQ(preview.bottom, editor.bottom);
}

TEST(EditorSplitDrop, LeftEdgeInsideLeftThird)
{
  const auto left = EditorSplitDrop::HitTest(10, 300, 900, 600, true, false, true);
  EXPECT_EQ(left.direction, EditorSplitDirection::Left);
  EXPECT_FALSE(left.mergeTarget);
}

TEST(EditorSplitDrop, RightEdgeInsideRightThird)
{
  const auto right = EditorSplitDrop::HitTest(890, 300, 900, 600, true, false, true);
  EXPECT_EQ(right.direction, EditorSplitDirection::Right);
  EXPECT_FALSE(right.mergeTarget);
}
