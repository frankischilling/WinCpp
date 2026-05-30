#include "EditorWorkspace.h"

#include "EditorSplitDrop.h"
#include "MainWindow.h"
#include "SyntaxRegistry.h"
#include "UiHelpers.h"

#include <algorithm>
#include <functional>
#include <windowsx.h>

namespace
{
constexpr wchar_t kWorkspaceHostClass[] = L"WinCppEditorWorkspaceHost";
constexpr wchar_t kDropOverlayClass[] = L"WinCppEditorDropOverlay";
constexpr int kMinGroupWidth = 160;
constexpr int kMinGroupHeight = 120;

EditorWorkspace* GetWorkspaceFromHost(HWND hwnd)
{
  return reinterpret_cast<EditorWorkspace*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

EditorWorkspace* GetWorkspaceFromOverlay(HWND hwnd)
{
  return reinterpret_cast<EditorWorkspace*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void DrawVerticalLine(HDC hdc, int x, int top, int bottom, COLORREF color)
{
  if (bottom <= top)
  {
    return;
  }

  HPEN pen = CreatePen(PS_SOLID, 1, color);
  HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, pen));
  MoveToEx(hdc, x, top, nullptr);
  LineTo(hdc, x, bottom);
  SelectObject(hdc, oldPen);
  DeleteObject(pen);
}

}

bool EditorWorkspace::Create(HWND parent, HINSTANCE instance, HWND notifyHwnd,
                           DWORD_PTR mainWindowRef)
{
  parent_ = parent;
  instance_ = instance;
  notifyHwnd_ = notifyHwnd;
  mainWindowRef_ = mainWindowRef;

  WNDCLASSEXW hostClass = {};
  if (!GetClassInfoExW(instance, kWorkspaceHostClass, &hostClass))
  {
    hostClass = {};
    hostClass.cbSize = sizeof(hostClass);
    hostClass.lpfnWndProc = WorkspaceHostProc;
    hostClass.hInstance = instance;
    hostClass.lpszClassName = kWorkspaceHostClass;
    hostClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&hostClass))
    {
      return false;
    }
  }

  WNDCLASSEXW overlayClass = {};
  if (!GetClassInfoExW(instance, kDropOverlayClass, &overlayClass))
  {
    overlayClass = {};
    overlayClass.cbSize = sizeof(overlayClass);
    overlayClass.lpfnWndProc = DropOverlayProc;
    overlayClass.hInstance = instance;
    overlayClass.lpszClassName = kDropOverlayClass;
    if (!RegisterClassExW(&overlayClass))
    {
      return false;
    }
  }

  host_ = CreateWindowExW(
      0,
      kWorkspaceHostClass,
      L"",
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
      0,
      0,
      0,
      0,
      parent,
      nullptr,
      instance,
      this);

  if (!host_)
  {
    return false;
  }

  dropOverlay_.hwnd = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kDropOverlayClass,
      L"",
      WS_POPUP,
      0,
      0,
      0,
      0,
      nullptr,
      nullptr,
      instance,
      this);

  if (!dropOverlay_.hwnd)
  {
    return false;
  }

  ShowWindow(dropOverlay_.hwnd, SW_HIDE);

  if (!TabBar::RegisterClass(instance))
  {
    return false;
  }

  EnsureDefaultGroup();
  return true;
}

void EditorWorkspace::Destroy()
{
  groups_.clear();
  root_.reset();
  if (dropOverlay_.hwnd)
  {
    DestroyWindow(dropOverlay_.hwnd);
    dropOverlay_.hwnd = nullptr;
  }
  if (host_)
  {
    DestroyWindow(host_);
    host_ = nullptr;
  }
}

void EditorWorkspace::SetBounds(const RECT& bounds)
{
  bounds_ = bounds;
  if (host_)
  {
    MoveWindow(host_, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
  }
  Layout();
}

int EditorWorkspace::GetTabStripHeight() const
{
  return tabStripHeight_ > 0 ? tabStripHeight_ : MulDiv(28, 96, 96);
}

EditorView& EditorWorkspace::ActiveEditor()
{
  EditorGroup* group = FindGroup(activeGroupId_);
  if (!group && !groups_.empty())
  {
    activeGroupId_ = groups_.front()->id;
    group = groups_.front().get();
  }
  static EditorView fallback;
  return group ? group->editor : fallback;
}

const EditorView& EditorWorkspace::ActiveEditor() const
{
  return const_cast<EditorWorkspace*>(this)->ActiveEditor();
}

EditorView& EditorWorkspace::PrimaryEditor()
{
  if (groups_.empty())
  {
    EnsureDefaultGroup();
  }
  return groups_.empty() ? ActiveEditor() : groups_.front()->editor;
}

const EditorView& EditorWorkspace::PrimaryEditor() const
{
  return const_cast<EditorWorkspace*>(this)->PrimaryEditor();
}

int EditorWorkspace::ActiveDocumentIndex() const
{
  const EditorGroup* group = FindGroup(activeGroupId_);
  if (!group || group->documentIndices.empty())
  {
    return -1;
  }

  if (group->selectedTabIndex < 0 ||
      group->selectedTabIndex >= static_cast<int>(group->documentIndices.size()))
  {
    return group->documentIndices.front();
  }

  return group->documentIndices[group->selectedTabIndex];
}

void EditorWorkspace::SetActiveGroup(int groupId)
{
  if (!FindGroup(groupId))
  {
    return;
  }

  activeGroupId_ = groupId;
  for (const auto& group : groups_)
  {
    if (group->container)
    {
      InvalidateRect(group->container, nullptr, FALSE);
    }
  }
}

void EditorWorkspace::LoadSyntaxDirectory(const std::wstring& directory)
{
  syntaxDirectory_ = directory;
  std::string error;
  if (!SyntaxRegistry::LoadSharedDirectory(directory, &error))
  {
    OutputDebugStringA(error.c_str());
    OutputDebugStringA("\n");
  }
}

void EditorWorkspace::EnsureDefaultGroup()
{
  if (!groups_.empty() && root_)
  {
    if (!FindGroup(activeGroupId_))
    {
      activeGroupId_ = groups_.front()->id;
    }
    return;
  }

  groups_.clear();
  root_.reset();
  const int id = CreateGroup();
  root_ = std::make_unique<SplitNode>();
  root_->leaf = true;
  root_->groupId = id;
  activeGroupId_ = id;
  Layout();
}

int EditorWorkspace::CreateGroup()
{
  auto group = std::make_unique<EditorGroup>();
  group->id = nextGroupId_++;

  const int tabControlId = nextControlId_++;
  group->container = CreateWindowExW(
      0,
      L"STATIC",
      L"",
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
      0,
      0,
      0,
      0,
      host_,
      nullptr,
      instance_,
      nullptr);

  if (!group->container)
  {
    return -1;
  }

  group->tabBar.Create(group->container, instance_, tabControlId);
  group->tabBar.SetNotifyTarget(notifyHwnd_, group->id);
  group->editor.Create(group->container, instance_);
#if !defined(WINCPP_TEST_BUILD)
  if (mainWindowRef_ != 0)
  {
    SetWindowSubclass(group->editor.Hwnd(), MainWindow::EditorSubclassProc, 2, mainWindowRef_);
  }
#endif

  if (!syntaxDirectory_.empty())
  {
    group->editor.LoadSyntaxDirectory(syntaxDirectory_);
  }

  const int id = group->id;
  groups_.push_back(std::move(group));
  return id;
}

bool EditorWorkspace::RemoveGroupFromTree(std::unique_ptr<SplitNode>& node, int groupId)
{
  if (!node)
  {
    return false;
  }
  if (node->leaf)
  {
    if (node->groupId == groupId)
    {
      node.reset();
      return true;
    }
    return false;
  }

  if (RemoveGroupFromTree(node->first, groupId))
  {
    if (!node->second)
    {
      return true;
    }
    node = std::move(node->second);
    return true;
  }
  if (RemoveGroupFromTree(node->second, groupId))
  {
    if (!node->first)
    {
      return true;
    }
    node = std::move(node->first);
    return true;
  }
  return false;
}

void EditorWorkspace::DestroyGroup(int groupId)
{
  if (groups_.size() <= 1)
  {
    return;
  }

  auto it = std::find_if(groups_.begin(), groups_.end(),
                         [groupId](const auto& g) { return g->id == groupId; });
  if (it == groups_.end())
  {
    return;
  }

  if ((*it)->container)
  {
    DestroyWindow((*it)->container);
  }

  groups_.erase(it);

  if (activeGroupId_ == groupId)
  {
    activeGroupId_ = groups_.empty() ? 0 : groups_.front()->id;
  }

  RemoveGroupFromTree(root_, groupId);

  if (!root_ && !groups_.empty())
  {
    root_ = std::make_unique<SplitNode>();
    root_->leaf = true;
    root_->groupId = groups_.front()->id;
    activeGroupId_ = root_->groupId;
  }

  Layout();
}

bool EditorWorkspace::HasMultipleGroups() const
{
  return groups_.size() > 1;
}

bool EditorWorkspace::SplitGroupLeaf(int targetGroupId, int newGroupId, EditorSplitDirection direction)
{
  std::function<bool(std::unique_ptr<SplitNode>&)> splitAt = [&](std::unique_ptr<SplitNode>& node) -> bool {
    if (!node || !node->leaf)
    {
      if (!node)
      {
        return false;
      }
      return splitAt(node->first) || splitAt(node->second);
    }

    if (node->groupId != targetGroupId)
    {
      return false;
    }

    const int oldGroupId = node->groupId;
    auto oldLeaf = std::make_unique<SplitNode>();
    oldLeaf->leaf = true;
    oldLeaf->groupId = oldGroupId;

    auto newLeaf = std::make_unique<SplitNode>();
    newLeaf->leaf = true;
    newLeaf->groupId = newGroupId;

    node->leaf = false;
    node->ratio = 500;
    switch (direction)
    {
      case EditorSplitDirection::Left:
        node->horizontal = true;
        node->first = std::move(newLeaf);
        node->second = std::move(oldLeaf);
        break;
      case EditorSplitDirection::Right:
        node->horizontal = true;
        node->first = std::move(oldLeaf);
        node->second = std::move(newLeaf);
        break;
      case EditorSplitDirection::Up:
        node->horizontal = false;
        node->first = std::move(newLeaf);
        node->second = std::move(oldLeaf);
        break;
      case EditorSplitDirection::Down:
        node->horizontal = false;
        node->first = std::move(oldLeaf);
        node->second = std::move(newLeaf);
        break;
      default:
        return false;
    }
    return true;
  };

  return splitAt(root_);
}

void EditorWorkspace::SplitActive(EditorSplitDirection direction)
{
  if (direction == EditorSplitDirection::None)
  {
    return;
  }

  EditorGroup* source = FindGroup(activeGroupId_);
  if (!source)
  {
    return;
  }

  const int docIndex = ActiveDocumentIndex();
  const int newGroupId = CreateGroup();
  EditorGroup* target = FindGroup(newGroupId);
  if (!target)
  {
    return;
  }

  if (docIndex >= 0)
  {
    target->documentIndices.push_back(docIndex);
    target->selectedTabIndex = 0;
  }

  if (!SplitGroupLeaf(activeGroupId_, newGroupId, direction))
  {
    DestroyGroup(newGroupId);
    return;
  }

  activeGroupId_ = newGroupId;
  Layout();

  if (notifyHwnd_ && docIndex >= 0)
  {
    SendMessageW(notifyHwnd_, WM_WORKSPACE_TAB_SELECT, 0, static_cast<LPARAM>(newGroupId));
  }
}

void EditorWorkspace::CloseActiveGroup()
{
  DestroyGroup(activeGroupId_);
}

EditorWorkspace::EditorGroup* EditorWorkspace::FindGroup(int groupId)
{
  for (auto& group : groups_)
  {
    if (group->id == groupId)
    {
      return group.get();
    }
  }
  return nullptr;
}

const EditorWorkspace::EditorGroup* EditorWorkspace::FindGroup(int groupId) const
{
  return const_cast<EditorWorkspace*>(this)->FindGroup(groupId);
}

int EditorWorkspace::GetDocumentIndex(int groupId, int localTabIndex) const
{
  const EditorGroup* group = FindGroup(groupId);
  if (!group || localTabIndex < 0 ||
      localTabIndex >= static_cast<int>(group->documentIndices.size()))
  {
    return -1;
  }

  return group->documentIndices[localTabIndex];
}

void EditorWorkspace::FocusGroupTab(int groupId, int localTabIndex,
                                    const std::vector<EditorDocument>& documents)
{
  EditorGroup* group = FindGroup(groupId);
  if (!group)
  {
    return;
  }

  SelectLocalTab(*group, localTabIndex, documents);
}

int EditorWorkspace::FindGroupIdByTabBar(HWND tabBarHwnd) const
{
  for (const auto& group : groups_)
  {
    if (group->tabBar.Hwnd() == tabBarHwnd)
    {
      return group->id;
    }
  }
  return -1;
}

HWND EditorWorkspace::TabBarHwndForGroup(int groupId) const
{
  const EditorGroup* group = FindGroup(groupId);
  return group ? group->tabBar.Hwnd() : nullptr;
}

bool EditorWorkspace::IsGroupEmpty(int groupId) const
{
  const EditorGroup* group = FindGroup(groupId);
  return !group || group->documentIndices.empty();
}

int EditorWorkspace::FindEmptySplitSibling(int groupId, EditorSplitDirection direction) const
{
  if (!root_ || direction == EditorSplitDirection::None)
  {
    return -1;
  }

  const SplitNode* parent = nullptr;
  bool isFirstChild = false;

  std::function<bool(const SplitNode*)> findParent = [&](const SplitNode* node) -> bool {
    if (!node || node->leaf)
    {
      return false;
    }
    if (node->first && node->first->leaf && node->first->groupId == groupId)
    {
      parent = node;
      isFirstChild = true;
      return true;
    }
    if (node->second && node->second->leaf && node->second->groupId == groupId)
    {
      parent = node;
      isFirstChild = false;
      return true;
    }
    return findParent(node->first.get()) || findParent(node->second.get());
  };

  if (!findParent(root_.get()) || !parent)
  {
    return -1;
  }

  const int siblingId =
      isFirstChild ? parent->second->groupId : parent->first->groupId;
  if (!IsGroupEmpty(siblingId))
  {
    return -1;
  }

  switch (direction)
  {
    case EditorSplitDirection::Left:
    case EditorSplitDirection::Up:
      return isFirstChild ? -1 : siblingId;
    case EditorSplitDirection::Right:
    case EditorSplitDirection::Down:
      return isFirstChild ? siblingId : -1;
    default:
      return -1;
  }
}

void EditorWorkspace::ReparentGroupEditor(EditorGroup& group)
{
  if (!group.editor.Hwnd() || !group.container)
  {
    return;
  }

  SetParent(group.editor.Hwnd(), group.container);
#if !defined(WINCPP_TEST_BUILD)
  if (mainWindowRef_ != 0)
  {
    SetWindowSubclass(group.editor.Hwnd(), MainWindow::EditorSubclassProc, 2, mainWindowRef_);
  }
#endif
}

void EditorWorkspace::SwapGroupEditors(EditorGroup& a, EditorGroup& b)
{
  std::swap(a.editor, b.editor);
  ReparentGroupEditor(a);
  ReparentGroupEditor(b);
}

void EditorWorkspace::LayoutNode(SplitNode* node, const RECT& bounds, int tabStripHeight)
{
  if (!node)
  {
    return;
  }

  if (!node->leaf)
  {
    const bool firstEmpty =
        node->first && node->first->leaf && IsGroupEmpty(node->first->groupId);
    const bool secondEmpty =
        node->second && node->second->leaf && IsGroupEmpty(node->second->groupId);
    if (firstEmpty && !secondEmpty)
    {
      LayoutNode(node->second.get(), bounds, tabStripHeight);
      return;
    }
    if (secondEmpty && !firstEmpty)
    {
      LayoutNode(node->first.get(), bounds, tabStripHeight);
      return;
    }
  }

  if (node->leaf)
  {
    EditorGroup* group = FindGroup(node->groupId);
    if (!group || !group->container)
    {
      return;
    }

    if (IsGroupEmpty(node->groupId))
    {
      ShowWindow(group->container, SW_HIDE);
      return;
    }

    ShowWindow(group->container, SW_SHOW);

    MoveWindow(group->container, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);

    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int editorHeight = height > tabStripHeight ? height - tabStripHeight : 0;

    if (group->tabBar.Hwnd())
    {
      MoveWindow(group->tabBar.Hwnd(), 0, 0, width, tabStripHeight, TRUE);
    }
    if (group->editor.Hwnd())
    {
      MoveWindow(group->editor.Hwnd(), 0, tabStripHeight, width, editorHeight, TRUE);
    }
    return;
  }

  const int totalW = bounds.right - bounds.left;
  const int totalH = bounds.bottom - bounds.top;

  if (node->horizontal)
  {
    int firstW = totalW * node->ratio / 1000;
    firstW = (std::max)(firstW, kMinGroupWidth);
    firstW = (std::min)(firstW, totalW - kMinGroupWidth);
    if (firstW < kMinGroupWidth)
    {
      firstW = totalW / 2;
    }

    RECT first = bounds;
    first.right = first.left + firstW;
    RECT second = bounds;
    second.left = first.right;

    LayoutNode(node->first.get(), first, tabStripHeight);
    LayoutNode(node->second.get(), second, tabStripHeight);
  }
  else
  {
    int firstH = totalH * node->ratio / 1000;
    firstH = (std::max)(firstH, kMinGroupHeight);
    firstH = (std::min)(firstH, totalH - kMinGroupHeight);
    if (firstH < kMinGroupHeight)
    {
      firstH = totalH / 2;
    }

    RECT first = bounds;
    first.bottom = first.top + firstH;
    RECT second = bounds;
    second.top = first.bottom;

    LayoutNode(node->first.get(), first, tabStripHeight);
    LayoutNode(node->second.get(), second, tabStripHeight);
  }
}

void EditorWorkspace::Layout()
{
  if (!host_)
  {
    return;
  }

  RECT client = {};
  GetClientRect(host_, &client);

  tabStripHeight_ = 0;
  for (const auto& group : groups_)
  {
    if (group->tabBar.Hwnd())
    {
      const int h = group->tabBar.GetPreferredHeight();
      tabStripHeight_ = (std::max)(tabStripHeight_, h);
    }
  }

  if (tabStripHeight_ <= 0)
  {
    const UINT dpi = GetDpiForWindow(host_);
    tabStripHeight_ = MulDiv(28, static_cast<int>(dpi), 96);
  }

  if (root_)
  {
    LayoutNode(root_.get(), client, tabStripHeight_);
  }
}

int EditorWorkspace::LocalIndexForDocument(const EditorGroup& group, int documentIndex) const
{
  for (size_t i = 0; i < group.documentIndices.size(); ++i)
  {
    if (group.documentIndices[i] == documentIndex)
    {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void EditorWorkspace::SaveGroupEditorState(EditorGroup& group,
                                           const std::vector<EditorDocument>& documents)
{
  if (group.selectedTabIndex < 0 ||
      group.selectedTabIndex >= static_cast<int>(group.documentIndices.size()))
  {
    return;
  }

  const int docIndex = group.documentIndices[group.selectedTabIndex];
  if (docIndex < 0 || docIndex >= static_cast<int>(documents.size()))
  {
    return;
  }

  EditorDocument& doc = const_cast<EditorDocument&>(documents[docIndex]);
  if (doc.scintillaDoc)
  {
    group.editor.SetDocumentIfNeeded(doc.scintillaDoc);
    doc.modified = group.editor.IsModified();
  }
  else
  {
    void* const shown = group.editor.GetDocumentPointer();
    if (shown)
    {
      doc.scintillaDoc = shown;
      doc.modified = group.editor.IsModified();
    }
  }
}

void EditorWorkspace::SelectLocalTab(EditorGroup& group, int localIndex,
                                     const std::vector<EditorDocument>& documents)
{
  if (localIndex < 0 || localIndex >= static_cast<int>(group.documentIndices.size()))
  {
    return;
  }

  SaveGroupEditorState(group, documents);

  group.selectedTabIndex = localIndex;
  const int docIndex = group.documentIndices[localIndex];
  if (docIndex >= 0 && docIndex < static_cast<int>(documents.size()))
  {
    EditorDocument& doc = const_cast<EditorDocument&>(documents[docIndex]);
    if (doc.scintillaDoc)
    {
      group.editor.SetDocumentIfNeeded(doc.scintillaDoc);
      if (!doc.path.empty())
      {
        group.editor.ApplySyntaxForPath(doc.path);
      }
      doc.modified = group.editor.IsModified();
    }
  }

  group.tabBar.SetSelectedIndex(localIndex);
  SetActiveGroup(group.id);
  SetFocus(group.editor.Hwnd());
}

void EditorWorkspace::SyncGroup(EditorGroup& group, const std::vector<EditorDocument>& documents)
{
  std::vector<std::wstring> titles;
  titles.reserve(group.documentIndices.size());
  for (int docIndex : group.documentIndices)
  {
    if (docIndex >= 0 && docIndex < static_cast<int>(documents.size()))
    {
      const EditorDocument& doc = documents[docIndex];
      titles.push_back(doc.displayTitle.empty() ? doc.tabTitle : doc.displayTitle);
    }
    else
    {
      titles.push_back(L"Untitled");
    }
  }

  if (group.selectedTabIndex >= static_cast<int>(group.documentIndices.size()))
  {
    group.selectedTabIndex =
        group.documentIndices.empty() ? 0 : static_cast<int>(group.documentIndices.size()) - 1;
  }

  group.tabBar.SetTabs(titles, group.selectedTabIndex);
}

void EditorWorkspace::SyncGroupTabs(const std::vector<EditorDocument>& documents)
{
  for (auto& group : groups_)
  {
    SyncGroup(*group, documents);
  }
}

void EditorWorkspace::AttachDocumentToActiveGroup(int documentIndex,
                                                  const std::vector<EditorDocument>& documents)
{
  EditorGroup* group = FindGroup(activeGroupId_);
  if (!group)
  {
    return;
  }

  if (LocalIndexForDocument(*group, documentIndex) < 0)
  {
    group->documentIndices.push_back(documentIndex);
  }

  const int localIndex = LocalIndexForDocument(*group, documentIndex);
  SelectLocalTab(*group, localIndex, documents);
  SyncGroup(*group, documents);
}

void EditorWorkspace::OpenDocumentInActiveGroup(int documentIndex,
                                                const std::vector<EditorDocument>& documents)
{
  AttachDocumentToActiveGroup(documentIndex, documents);
}

void EditorWorkspace::RemoveDocumentFromAllGroups(int documentIndex)
{
  for (auto& group : groups_)
  {
    const auto it =
        std::find(group->documentIndices.begin(), group->documentIndices.end(), documentIndex);
    if (it != group->documentIndices.end())
    {
      group->documentIndices.erase(it);
      if (group->selectedTabIndex >= static_cast<int>(group->documentIndices.size()))
      {
        group->selectedTabIndex =
            group->documentIndices.empty() ? 0 : static_cast<int>(group->documentIndices.size()) - 1;
      }
    }
  }
}

void EditorWorkspace::ReindexDocumentsAfterClose(int closedIndex)
{
  for (auto& group : groups_)
  {
    for (int& docIndex : group->documentIndices)
    {
      if (docIndex > closedIndex)
      {
        --docIndex;
      }
    }
  }
}

void EditorWorkspace::PruneEmptyGroups()
{
  while (groups_.size() > 1)
  {
    bool destroyed = false;
    for (const auto& group : groups_)
    {
      if (group->documentIndices.empty())
      {
        DestroyGroup(group->id);
        destroyed = true;
        break;
      }
    }

    if (!destroyed)
    {
      break;
    }
  }
}

void EditorWorkspace::ReindexDocumentsAfterReorder(int fromIndex, int toIndex)
{
  for (auto& group : groups_)
  {
    for (int& docIndex : group->documentIndices)
    {
      if (docIndex == fromIndex)
      {
        docIndex = toIndex;
      }
      else if (fromIndex < toIndex)
      {
        if (docIndex > fromIndex && docIndex <= toIndex)
        {
          --docIndex;
        }
      }
      else if (fromIndex > toIndex)
      {
        if (docIndex >= toIndex && docIndex < fromIndex)
        {
          ++docIndex;
        }
      }
    }
  }
}

void EditorWorkspace::OnTabSelect(int groupId, int localTabIndex)
{
  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_WORKSPACE_TAB_SELECT, static_cast<WPARAM>(localTabIndex),
                 static_cast<LPARAM>(groupId));
  }
}

void EditorWorkspace::OnTabClose(int groupId, int localTabIndex)
{
  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_WORKSPACE_TAB_CLOSE, static_cast<WPARAM>(localTabIndex),
                 static_cast<LPARAM>(groupId));
  }
}

void EditorWorkspace::OnTabReorder(int groupId, int fromIndex, int insertBefore)
{
  EditorGroup* group = FindGroup(groupId);
  if (!group)
  {
    return;
  }

  const int count = static_cast<int>(group->documentIndices.size());
  if (fromIndex < 0 || fromIndex >= count || insertBefore < 0 || insertBefore > count)
  {
    return;
  }

  if (insertBefore == fromIndex || insertBefore == fromIndex + 1)
  {
    return;
  }

  const int docId = group->documentIndices[fromIndex];
  group->documentIndices.erase(group->documentIndices.begin() + fromIndex);

  int target = insertBefore;
  if (fromIndex < insertBefore)
  {
    --target;
  }

  group->documentIndices.insert(group->documentIndices.begin() + target, docId);

  if (group->selectedTabIndex == fromIndex)
  {
    group->selectedTabIndex = target;
  }
  else if (fromIndex < group->selectedTabIndex && target <= group->selectedTabIndex)
  {
    --group->selectedTabIndex;
  }
  else if (fromIndex > group->selectedTabIndex && target > group->selectedTabIndex)
  {
    ++group->selectedTabIndex;
  }

  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_WORKSPACE_REQUEST_SYNC, 0, 0);
  }
}

EditorWorkspace::EditorGroup* EditorWorkspace::GroupAtPoint(POINT screenPoint, RECT* groupBounds)
{
  for (auto& group : groups_)
  {
    if (!group->container)
    {
      continue;
    }

    RECT rect = {};
    GetWindowRect(group->container, &rect);
    if (PtInRect(&rect, screenPoint))
    {
      if (groupBounds)
      {
        *groupBounds = rect;
      }
      return group.get();
    }
  }
  return nullptr;
}

EditorWorkspace::EditorGroup* EditorWorkspace::TabBarGroupAtPoint(POINT screenPoint)
{
  for (auto& group : groups_)
  {
    if (!group->tabBar.Hwnd())
    {
      continue;
    }

    RECT rect = {};
    GetWindowRect(group->tabBar.Hwnd(), &rect);
    if (PtInRect(&rect, screenPoint))
    {
      return group.get();
    }
  }
  return nullptr;
}

void EditorWorkspace::ShowDropPreview(EditorGroup* target, const EditorSplitDropResult& drop)
{
  if (!dropOverlay_.hwnd || !target || !target->editor.Hwnd())
  {
    return;
  }

  RECT editorScreen = {};
  GetWindowRect(target->editor.Hwnd(), &editorScreen);

  RECT preview = editorScreen;
  dropOverlay_.mergePreview = drop.mergeTarget;
  dropOverlay_.direction = drop.direction;

  if (drop.mergeTarget)
  {
    preview = editorScreen;
  }
  else if (drop.direction != EditorSplitDirection::None)
  {
    preview = EditorSplitDrop::PreviewRect(editorScreen, drop.direction);
  }
  else
  {
    return;
  }

  const int overlayW = preview.right - preview.left;
  const int overlayH = preview.bottom - preview.top;
  if (overlayW <= 0 || overlayH <= 0)
  {
    return;
  }

  SetWindowPos(dropOverlay_.hwnd, HWND_TOPMOST, preview.left, preview.top, overlayW, overlayH,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  InvalidateRect(dropOverlay_.hwnd, nullptr, TRUE);
}

void EditorWorkspace::HideDropOverlay()
{
  dropOverlay_.direction = EditorSplitDirection::None;
  dropOverlay_.mergePreview = false;
  if (dropOverlay_.hwnd)
  {
    ShowWindow(dropOverlay_.hwnd, SW_HIDE);
    SetWindowPos(dropOverlay_.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW);
  }
}

void EditorWorkspace::BeginDragTracking(int groupId, int tabIndex)
{
  dragSourceGroupId_ = groupId;
  dragSourceTabIndex_ = tabIndex;
  if (!dragTracking_ && host_)
  {
    dragTracking_ = true;
    SetTimer(host_, kDragTrackTimerId, 16, nullptr);
  }
}

void EditorWorkspace::EndDragTracking()
{
  HideDropOverlay();
  dragTracking_ = false;
  if (host_)
  {
    KillTimer(host_, kDragTrackTimerId);
  }
  dragSourceGroupId_ = -1;
  dragSourceTabIndex_ = -1;
}

void EditorWorkspace::MoveTabToGroup(int sourceGroupId, int localTabIndex, int targetGroupId,
                                     EditorSplitDirection splitDirection)
{
  EditorGroup* source = FindGroup(sourceGroupId);
  EditorGroup* target = FindGroup(targetGroupId);
  if (!source || !target || localTabIndex < 0 ||
      localTabIndex >= static_cast<int>(source->documentIndices.size()))
  {
    return;
  }

  const int docIndex = source->documentIndices[localTabIndex];
  source->documentIndices.erase(source->documentIndices.begin() + localTabIndex);
  if (source->selectedTabIndex >= static_cast<int>(source->documentIndices.size()))
  {
    source->selectedTabIndex =
        source->documentIndices.empty() ? 0 : static_cast<int>(source->documentIndices.size()) - 1;
  }

  const bool sourceEmpty = source->documentIndices.empty();
  const int sourceSelectTab = source->selectedTabIndex;
  const bool intraGroupSplit =
      splitDirection != EditorSplitDirection::None && sourceGroupId == targetGroupId;

  if (intraGroupSplit || splitDirection != EditorSplitDirection::None)
  {
    const int leafGroupId = target->id;
    int destGroupId = -1;
    if (intraGroupSplit)
    {
      destGroupId = FindEmptySplitSibling(leafGroupId, splitDirection);
    }

    if (destGroupId < 0)
    {
      destGroupId = CreateGroup();
      EditorGroup* newGroup = FindGroup(destGroupId);
      if (!newGroup)
      {
        source->documentIndices.insert(source->documentIndices.begin() + localTabIndex, docIndex);
        return;
      }

      if (!SplitGroupLeaf(leafGroupId, destGroupId, splitDirection))
      {
        DestroyGroup(destGroupId);
        source->documentIndices.insert(source->documentIndices.begin() + localTabIndex, docIndex);
        return;
      }
    }

    EditorGroup* destGroup = FindGroup(destGroupId);
    if (!destGroup)
    {
      source->documentIndices.insert(source->documentIndices.begin() + localTabIndex, docIndex);
      return;
    }

    destGroup->documentIndices.push_back(docIndex);
    destGroup->selectedTabIndex = LocalIndexForDocument(*destGroup, docIndex);

    // Keep the live Scintilla on the pane that received the tab; the vacated pane gets the
    // placeholder control so we do not show two full editor surfaces for one document.
    if (intraGroupSplit && sourceEmpty)
    {
      SwapGroupEditors(*source, *destGroup);
    }

    activeGroupId_ = destGroupId;
  }
  else
  {
    if (LocalIndexForDocument(*target, docIndex) < 0)
    {
      target->documentIndices.push_back(docIndex);
    }
    target->selectedTabIndex = LocalIndexForDocument(*target, docIndex);
    activeGroupId_ = target->id;
  }

  // After splitting within the same pane, the source leaf stays in the tree (possibly
  // with no tabs). Destroying it would remove a split child and collapse the layout.
  if (sourceEmpty && !intraGroupSplit)
  {
    if (groups_.size() > 1)
    {
      DestroyGroup(sourceGroupId);
    }
  }
  else if (!sourceEmpty && notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_WORKSPACE_TAB_SELECT, static_cast<WPARAM>(sourceSelectTab),
                 static_cast<LPARAM>(sourceGroupId));
  }

  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_WORKSPACE_REQUEST_SYNC, 0, 0);
    EditorGroup* active = FindGroup(activeGroupId_);
    if (active && !active->documentIndices.empty())
    {
      SendMessageW(notifyHwnd_, WM_WORKSPACE_TAB_SELECT,
                   static_cast<WPARAM>(active->selectedTabIndex),
                   static_cast<LPARAM>(activeGroupId_));
    }
  }

  Layout();
}

void EditorWorkspace::OnTabDragMove(int groupId, int tabIndex, POINT screenPoint)
{
  BeginDragTracking(groupId, tabIndex);

  if (IsPointOverAnyTabBar(screenPoint))
  {
    HideDropOverlay();
    return;
  }

  EditorGroup* target = GroupAtPoint(screenPoint, nullptr);
  if (!target || !target->editor.Hwnd())
  {
    HideDropOverlay();
    return;
  }

  RECT editorScreen = {};
  GetWindowRect(target->editor.Hwnd(), &editorScreen);

  const EditorSplitDropResult drop = EditorSplitDrop::HitTestScreenPoint(
      screenPoint, editorScreen, preferVerticalSplit_, false, true);
  if (!drop.mergeTarget && drop.direction == EditorSplitDirection::None)
  {
    HideDropOverlay();
    return;
  }

  ShowDropPreview(target, drop);
}

#if defined(WINCPP_TEST_BUILD)
void EditorWorkspace::TestMoveTabToGroup(int sourceGroupId, int localTabIndex, int targetGroupId,
                                         EditorSplitDirection splitDirection)
{
  MoveTabToGroup(sourceGroupId, localTabIndex, targetGroupId, splitDirection);
}
#endif

void EditorWorkspace::OnTabDragEnd(int groupId, int tabIndex, POINT screenPoint)
{
  EndDragTracking();

  EditorGroup* source = FindGroup(groupId);
  if (!source || tabIndex < 0 || tabIndex >= static_cast<int>(source->documentIndices.size()))
  {
    return;
  }

  EditorGroup* target = GroupAtPoint(screenPoint, nullptr);
  if (target && target->editor.Hwnd())
  {
    RECT editorScreen = {};
    GetWindowRect(target->editor.Hwnd(), &editorScreen);
    if (PtInRect(&editorScreen, screenPoint))
    {
      const EditorSplitDropResult drop = EditorSplitDrop::HitTestScreenPoint(
          screenPoint, editorScreen, preferVerticalSplit_, false, true);
      if (drop.mergeTarget)
      {
        if (target->id != groupId)
        {
          MoveTabToGroup(groupId, tabIndex, target->id, EditorSplitDirection::None);
        }
        return;
      }
      if (drop.direction != EditorSplitDirection::None)
      {
        MoveTabToGroup(groupId, tabIndex, target->id, drop.direction);
        return;
      }
    }
  }

  if (EditorGroup* targetTabBar = TabBarGroupAtPoint(screenPoint))
  {
    if (targetTabBar->id != groupId)
    {
      MoveTabToGroup(groupId, tabIndex, targetTabBar->id, EditorSplitDirection::None);
      return;
    }

    RECT tabRect = {};
    GetWindowRect(source->tabBar.Hwnd(), &tabRect);
    if (PtInRect(&tabRect, screenPoint))
    {
      // Reorder handled by TabBar if still over same bar.
    }
  }
}

bool EditorWorkspace::IsTabBarHwnd(HWND hwnd) const
{
  if (!hwnd)
  {
    return false;
  }

  for (const auto& group : groups_)
  {
    if (group->tabBar.Hwnd() == hwnd)
    {
      return true;
    }
  }
  return false;
}

bool EditorWorkspace::IsPointOverAnyTabBar(POINT screenPoint) const
{
  for (const auto& group : groups_)
  {
    if (!group->tabBar.Hwnd())
    {
      continue;
    }

    RECT rect = {};
    GetWindowRect(group->tabBar.Hwnd(), &rect);
    if (PtInRect(&rect, screenPoint))
    {
      return true;
    }
  }
  return false;
}

bool EditorWorkspace::ForwardWheelToTabBar(POINT screenPoint, UINT msg, WPARAM wParam,
                                            LPARAM lParam)
{
  for (auto& group : groups_)
  {
    if (!group->tabBar.Hwnd())
    {
      continue;
    }

    RECT rect = {};
    GetWindowRect(group->tabBar.Hwnd(), &rect);
    if (PtInRect(&rect, screenPoint))
    {
      return SendMessageW(group->tabBar.Hwnd(), msg, wParam, lParam) != 0;
    }
  }
  return false;
}

LRESULT CALLBACK EditorWorkspace::WorkspaceHostProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  EditorWorkspace* workspace = nullptr;
  if (msg == WM_NCCREATE)
  {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    workspace = reinterpret_cast<EditorWorkspace*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(workspace));
  }
  else
  {
    workspace = GetWorkspaceFromHost(hwnd);
  }

  if (workspace && msg == WM_SIZE)
  {
    workspace->Layout();
    return 0;
  }

  if (workspace && msg == WM_TIMER && wParam == EditorWorkspace::kDragTrackTimerId)
  {
    if (!workspace->dragTracking_)
    {
      KillTimer(hwnd, EditorWorkspace::kDragTrackTimerId);
      return 0;
    }

    if (workspace->dragSourceGroupId_ >= 0 && workspace->dragSourceTabIndex_ >= 0)
    {
      POINT pt = {};
      GetCursorPos(&pt);
      workspace->OnTabDragMove(workspace->dragSourceGroupId_, workspace->dragSourceTabIndex_, pt);
    }
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK EditorWorkspace::DropOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  EditorWorkspace* workspace = nullptr;
  if (msg == WM_NCCREATE)
  {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    workspace = reinterpret_cast<EditorWorkspace*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(workspace));
  }
  else
  {
    workspace = GetWorkspaceFromOverlay(hwnd);
  }

  if (workspace && msg == WM_ERASEBKGND)
  {
    return 1;
  }

  if (workspace && msg == WM_PAINT)
  {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client = {};
    GetClientRect(hwnd, &client);

    const int w = client.right - client.left;
    const int h = client.bottom - client.top;
    if (w > 0 && h > 0)
    {
      const bool merge = workspace->dropOverlay_.mergePreview;
      const COLORREF fillColor = merge ? RGB(220, 220, 220) : RGB(173, 214, 255);
      const COLORREF borderColor = merge ? RGB(120, 120, 120) : RGB(0, 122, 204);

      HBRUSH fill = CreateSolidBrush(fillColor);
      FillRect(hdc, &client, fill);
      DeleteObject(fill);

      const int border = MulDiv(merge ? 1 : 2, GetDpiForWindow(hwnd), 96);
      const UINT penStyle = merge ? PS_DOT : PS_SOLID;
      HPEN pen = CreatePen(penStyle, border, borderColor);
      HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, pen));
      HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
      Rectangle(hdc, client.left, client.top, client.right - 1, client.bottom - 1);
      SelectObject(hdc, oldBrush);
      SelectObject(hdc, oldPen);
      DeleteObject(pen);
    }

    EndPaint(hwnd, &ps);
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
