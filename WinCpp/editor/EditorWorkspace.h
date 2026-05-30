#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "EditorSettings.h"
#include "EditorSplitDrop.h"
#include "EditorView.h"
#include "TabBar.h"
#include "UiTheme.h"
#include "WinCpp.h"

struct EditorDocument;

constexpr UINT WM_WORKSPACE_TAB_SELECT = WM_USER + 210;
// wParam = local tab index, lParam = group id
constexpr UINT WM_WORKSPACE_TAB_CLOSE = WM_USER + 211;
constexpr UINT WM_WORKSPACE_REQUEST_SYNC = WM_USER + 212;

class EditorWorkspace
{
public:
  bool Create(HWND parent, HINSTANCE instance, HWND notifyHwnd, DWORD_PTR mainWindowRef);
  void Destroy();

  void SetBounds(const RECT& bounds);
  void Layout();
  int GetTabStripHeight() const;

  EditorView& ActiveEditor();
  const EditorView& ActiveEditor() const;
  EditorView& PrimaryEditor();
  const EditorView& PrimaryEditor() const;
  int ActiveGroupId() const { return activeGroupId_; }
  int ActiveDocumentIndex() const;
  void SetActiveGroup(int groupId);

  void LoadSyntaxDirectory(const std::wstring& directory);
  void ApplySettingsToAllEditors(const EditorSettings& settings);
  void ApplyThemeToAllEditors(const AppUiTheme& theme);
  void ForEachEditor(const std::function<void(EditorView&)>& fn);
  void EnsureDefaultGroup();
  void SplitActive(EditorSplitDirection direction);
  void CloseActiveGroup();
  bool HasMultipleGroups() const;
  int GroupCount() const { return static_cast<int>(groups_.size()); }

  void OnTabSelect(int groupId, int localTabIndex);
  void OnTabClose(int groupId, int localTabIndex);
  void OnTabReorder(int groupId, int fromIndex, int insertBefore);
  void OnTabDragMove(int groupId, int tabIndex, POINT screenPoint);
  void OnTabDragEnd(int groupId, int tabIndex, POINT screenPoint);
  void BeginDragTracking(int groupId, int tabIndex);
  void EndDragTracking();

  HWND TabBarHwndForGroup(int groupId) const;
  int FindGroupIdByTabBar(HWND tabBarHwnd) const;
  int GetDocumentIndex(int groupId, int localTabIndex) const;
  void FocusGroupTab(int groupId, int localTabIndex, const std::vector<EditorDocument>& documents);

  void SyncGroupTabs(const std::vector<EditorDocument>& documents);
  void AttachDocumentToActiveGroup(int documentIndex, const std::vector<EditorDocument>& documents);
  void OpenDocumentInActiveGroup(int documentIndex, const std::vector<EditorDocument>& documents);
  void RemoveDocumentFromAllGroups(int documentIndex);
  bool RemoveDocumentFromGroup(int groupId, int localTabIndex,
                               const std::vector<EditorDocument>& documents);
  int DocumentReferenceCount(int documentIndex) const;
  void ReindexDocumentsAfterClose(int closedIndex);
  void ReindexDocumentsAfterReorder(int fromIndex, int toIndex);
  void PruneEmptyGroups();

  bool IsPointOverAnyTabBar(POINT screenPoint) const;
  bool IsTabBarHwnd(HWND hwnd) const;
  bool ForwardWheelToTabBar(POINT screenPoint, UINT msg, WPARAM wParam, LPARAM lParam);

#if defined(WINCPP_TEST_BUILD)
  void TestMoveTabToGroup(int sourceGroupId, int localTabIndex, int targetGroupId,
                          EditorSplitDirection splitDirection);
#endif

private:
  struct EditorGroup
  {
    int id = 0;
    HWND container = nullptr;
    TabBar tabBar;
    EditorView editor;
    std::vector<int> documentIndices;
    int selectedTabIndex = 0;
  };

  struct SplitNode
  {
    bool leaf = true;
    int groupId = 0;
    bool horizontal = true;
    int ratio = 500;
    std::unique_ptr<SplitNode> first;
    std::unique_ptr<SplitNode> second;
  };

  struct DropOverlay
  {
    HWND hwnd = nullptr;
    EditorSplitDirection direction = EditorSplitDirection::None;
    bool mergePreview = false;
  };

  static LRESULT CALLBACK DropOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK WorkspaceHostProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  EditorGroup* FindGroup(int groupId);
  const EditorGroup* FindGroup(int groupId) const;
  EditorGroup* GroupAtPoint(POINT screenPoint, RECT* groupBounds = nullptr);
  EditorGroup* TabBarGroupAtPoint(POINT screenPoint);
  void ShowDropPreview(EditorGroup* target, const EditorSplitDropResult& drop);

  int CreateGroup();
  void DestroyGroup(int groupId);
  std::unique_ptr<SplitNode> CloneTree(const SplitNode* node) const;
  bool SplitGroupLeaf(int targetGroupId, int newGroupId, EditorSplitDirection direction);
  bool MoveGroupLeaf(int sourceGroupId, int targetGroupId, EditorSplitDirection direction);
  bool RemoveGroupFromTree(std::unique_ptr<SplitNode>& node, int groupId);
  void LayoutNode(SplitNode* node, const RECT& bounds, int tabStripHeight);
  void HideDropOverlay();
  void MoveTabToGroup(int sourceGroupId, int localTabIndex, int targetGroupId,
                      EditorSplitDirection splitDirection);
  void SyncGroup(EditorGroup& group, const std::vector<EditorDocument>& documents);
  void SelectLocalTab(EditorGroup& group, int localIndex, const std::vector<EditorDocument>& documents);
  void SaveGroupEditorState(EditorGroup& group, const std::vector<EditorDocument>& documents);
  int LocalIndexForDocument(const EditorGroup& group, int documentIndex) const;
  bool IsGroupEmpty(int groupId) const;
  int FindEmptySplitSibling(int groupId, EditorSplitDirection direction) const;
  void ReparentGroupEditor(EditorGroup& group);
  void SwapGroupEditors(EditorGroup& a, EditorGroup& b);

  HWND parent_ = nullptr;
  HWND notifyHwnd_ = nullptr;
  DWORD_PTR mainWindowRef_ = 0;
  HWND host_ = nullptr;
  HINSTANCE instance_ = nullptr;
  RECT bounds_ = {};
  int tabStripHeight_ = 0;
  int nextControlId_ = 2100;
  int nextGroupId_ = 1;
  int activeGroupId_ = 0;
  std::vector<std::unique_ptr<EditorGroup>> groups_;
  std::unique_ptr<SplitNode> root_;
  DropOverlay dropOverlay_;
  int dragSourceGroupId_ = -1;
  int dragSourceTabIndex_ = -1;
  bool dragTracking_ = false;
  std::wstring syntaxDirectory_;
  bool preferVerticalSplit_ = true;

  static constexpr UINT kDragTrackTimerId = 1;
};
