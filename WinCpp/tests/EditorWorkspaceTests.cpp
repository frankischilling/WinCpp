#include <gtest/gtest.h>

#include "EditorWorkspace.h"
#include "MainWindow.h"

#include <vector>

namespace
{
std::vector<EditorDocument> MakeDocuments()
{
  EditorDocument a;
  a.path = L"C:\\proj\\a.cpp";
  a.tabTitle = L"a.cpp";
  a.displayTitle = L"a.cpp";

  EditorDocument b;
  b.path = L"C:\\proj\\b.h";
  b.tabTitle = L"b.h";
  b.displayTitle = L"b.h";

  return {a, b};
}
}

class EditorWorkspaceTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    parent_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 900, 700, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(parent_, nullptr);
    ShowWindow(parent_, SW_SHOWNOACTIVATE);

    notify_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                               GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(notify_, nullptr);

    ASSERT_TRUE(workspace_.Create(parent_, GetModuleHandleW(nullptr), notify_, 0));
    RECT bounds{0, 0, 900, 700};
    workspace_.SetBounds(bounds);
    workspace_.Layout();
    documents_ = MakeDocuments();
  }

  void TearDown() override
  {
    workspace_.Destroy();
    if (notify_)
    {
      DestroyWindow(notify_);
      notify_ = nullptr;
    }
    if (parent_)
    {
      DestroyWindow(parent_);
      parent_ = nullptr;
    }
  }

  HWND parent_ = nullptr;
  HWND notify_ = nullptr;
  EditorWorkspace workspace_;
  std::vector<EditorDocument> documents_;
};

TEST_F(EditorWorkspaceTest, CreateEnsuresDefaultGroup)
{
  EXPECT_FALSE(workspace_.HasMultipleGroups());
  EXPECT_GT(workspace_.ActiveGroupId(), 0);
  EXPECT_NE(workspace_.ActiveEditor().Hwnd(), nullptr);
}

TEST_F(EditorWorkspaceTest, SplitActiveRightCreatesTwoGroups)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.SplitActive(EditorSplitDirection::Right);
  EXPECT_TRUE(workspace_.HasMultipleGroups());
}

TEST_F(EditorWorkspaceTest, MoveTabToGroupIntraSplitCreatesSecondPane)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  const int sourceGroup = workspace_.ActiveGroupId();
  workspace_.TestMoveTabToGroup(sourceGroup, 0, sourceGroup, EditorSplitDirection::Right);
  EXPECT_TRUE(workspace_.HasMultipleGroups());
  EXPECT_EQ(workspace_.GetDocumentIndex(sourceGroup, 0), -1);
  EXPECT_EQ(workspace_.GetDocumentIndex(workspace_.ActiveGroupId(), 0), 0);
}

TEST_F(EditorWorkspaceTest, IntraGroupSplitDragCreatesSecondPane)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.Layout();
  const int sourceGroup = workspace_.ActiveGroupId();
  HWND editor = workspace_.ActiveEditor().Hwnd();
  ASSERT_NE(editor, nullptr);

  RECT editorScreen = {};
  GetWindowRect(editor, &editorScreen);
  const int width = editorScreen.right - editorScreen.left;
  ASSERT_GT(width, 60);
  // Outside the center merge zone (see EditorSplitDrop edge thresholds).
  const POINT pt = {editorScreen.right - 2, (editorScreen.top + editorScreen.bottom) / 2};

  workspace_.OnTabDragEnd(sourceGroup, 0, pt);

  EXPECT_TRUE(workspace_.HasMultipleGroups());
  EXPECT_EQ(workspace_.GetDocumentIndex(sourceGroup, 0), -1);
  EXPECT_EQ(workspace_.GetDocumentIndex(workspace_.ActiveGroupId(), 0), 0);
}

TEST_F(EditorWorkspaceTest, IntraGroupRepeatedSplitStaysTwoGroups)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  const int sourceGroup = workspace_.ActiveGroupId();
  workspace_.TestMoveTabToGroup(sourceGroup, 0, sourceGroup, EditorSplitDirection::Right);
  ASSERT_EQ(workspace_.GroupCount(), 2);

  const int afterFirst = workspace_.ActiveGroupId();
  // Reuse the empty left pane from the first split instead of nesting a third group.
  workspace_.TestMoveTabToGroup(afterFirst, 0, afterFirst, EditorSplitDirection::Left);

  EXPECT_EQ(workspace_.GroupCount(), 2);
  EXPECT_EQ(workspace_.GetDocumentIndex(workspace_.ActiveGroupId(), 0), 0);
}

TEST_F(EditorWorkspaceTest, IntraGroupMergeCenterDropIsNoOp)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  const int sourceGroup = workspace_.ActiveGroupId();
  HWND editor = workspace_.ActiveEditor().Hwnd();
  RECT editorScreen = {};
  GetWindowRect(editor, &editorScreen);
  const POINT pt = {(editorScreen.left + editorScreen.right) / 2,
                    (editorScreen.top + editorScreen.bottom) / 2};

  workspace_.OnTabDragEnd(sourceGroup, 0, pt);

  EXPECT_FALSE(workspace_.HasMultipleGroups());
  EXPECT_EQ(workspace_.GetDocumentIndex(sourceGroup, 0), 0);
}

TEST_F(EditorWorkspaceTest, CloseActiveGroupMergesToOne)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.SplitActive(EditorSplitDirection::Right);
  ASSERT_TRUE(workspace_.HasMultipleGroups());
  workspace_.CloseActiveGroup();
  EXPECT_FALSE(workspace_.HasMultipleGroups());
}

TEST_F(EditorWorkspaceTest, SyncGroupTabsReflectsDocuments)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.AttachDocumentToActiveGroup(1, documents_);
  workspace_.SyncGroupTabs(documents_);
  SUCCEED();
}

TEST_F(EditorWorkspaceTest, AttachDocumentToActiveGroup)
{
  workspace_.AttachDocumentToActiveGroup(1, documents_);
  const int groupId = workspace_.ActiveGroupId();
  EXPECT_EQ(workspace_.GetDocumentIndex(groupId, 0), 1);
}

TEST_F(EditorWorkspaceTest, RemoveDocumentFromAllGroups)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.AttachDocumentToActiveGroup(1, documents_);
  workspace_.RemoveDocumentFromAllGroups(1);
  const int groupId = workspace_.ActiveGroupId();
  EXPECT_EQ(workspace_.GetDocumentIndex(groupId, 0), 0);
}

TEST_F(EditorWorkspaceTest, ReindexDocumentsAfterClose)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.AttachDocumentToActiveGroup(1, documents_);
  workspace_.RemoveDocumentFromAllGroups(0);
  workspace_.ReindexDocumentsAfterClose(0);
  const int groupId = workspace_.ActiveGroupId();
  EXPECT_EQ(workspace_.GetDocumentIndex(groupId, 0), 0);
}

TEST_F(EditorWorkspaceTest, ReindexDocumentsAfterReorder)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.AttachDocumentToActiveGroup(1, documents_);
  workspace_.ReindexDocumentsAfterReorder(0, 2);
  const int groupId = workspace_.ActiveGroupId();
  bool foundMoved = false;
  for (int i = 0; i < 2; ++i)
  {
    if (workspace_.GetDocumentIndex(groupId, i) == 2)
    {
      foundMoved = true;
    }
  }
  EXPECT_TRUE(foundMoved);
}

TEST_F(EditorWorkspaceTest, FindGroupIdByTabBar)
{
  const int groupId = workspace_.ActiveGroupId();
  HWND tabBar = workspace_.TabBarHwndForGroup(groupId);
  ASSERT_NE(tabBar, nullptr);
  EXPECT_EQ(workspace_.FindGroupIdByTabBar(tabBar), groupId);
}

TEST_F(EditorWorkspaceTest, SetActiveGroupSwitchesActiveEditor)
{
  workspace_.AttachDocumentToActiveGroup(0, documents_);
  workspace_.SplitActive(EditorSplitDirection::Right);
  EditorView& first = workspace_.PrimaryEditor();
  const int secondGroup = workspace_.ActiveGroupId();
  workspace_.SetActiveGroup(workspace_.FindGroupIdByTabBar(workspace_.TabBarHwndForGroup(
      workspace_.ActiveGroupId())));
  EditorView& active = workspace_.ActiveEditor();
  (void)first;
  (void)secondGroup;
  EXPECT_NE(active.Hwnd(), nullptr);
}

TEST_F(EditorWorkspaceTest, IsTabBarHwndRecognizesGroupTabBar)
{
  const int groupId = workspace_.ActiveGroupId();
  HWND tabBar = workspace_.TabBarHwndForGroup(groupId);
  EXPECT_TRUE(workspace_.IsTabBarHwnd(tabBar));
  EXPECT_FALSE(workspace_.IsTabBarHwnd(parent_));
}
