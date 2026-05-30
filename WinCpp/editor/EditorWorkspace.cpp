#include "EditorWorkspace.h"

#include "EditorSplitDrop.h"
#include "MainWindow.h"
#include "SyntaxRegistry.h"
#include "TabBarLogic.h"
#include "UiHelpers.h"
#include "UiTheme.h"

#include <algorithm>
#include <functional>
#include <windowsx.h>

namespace
{
constexpr wchar_t kWorkspaceHostClass[] = L"WinCppEditorWorkspaceHost";
constexpr wchar_t kDropOverlayClass[] = L"WinCppEditorDropOverlay";
constexpr int kMinGroupWidth = 160;
constexpr int kMinGroupHeight = 120;

EditorWorkspace *GetWorkspaceFromHost(HWND hwnd)
{
  return reinterpret_cast<EditorWorkspace *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

EditorWorkspace *GetWorkspaceFromOverlay(HWND hwnd)
{
  return reinterpret_cast<EditorWorkspace *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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

} // namespace

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

  host_ = CreateWindowExW(0, kWorkspaceHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0,
                          0, 0, 0, parent, nullptr, instance, this);

  if (!host_)
  {
    return false;
  }

  dropOverlay_.hwnd =
      CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kDropOverlayClass, L"",
                      WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, this);

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

void EditorWorkspace::SetBounds(const RECT &bounds)
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

EditorView &EditorWorkspace::ActiveEditor()
{
  EditorGroup *group = FindGroup(activeGroupId_);
  if (!group && !groups_.empty())
  {
    activeGroupId_ = groups_.front()->id;
    group = groups_.front().get();
  }
  static EditorView fallback;
  return group ? group->editor : fallback;
}

const EditorView &EditorWorkspace::ActiveEditor() const
{
  return const_cast<EditorWorkspace *>(this)->ActiveEditor();
}

EditorView &EditorWorkspace::PrimaryEditor()
{
  if (groups_.empty())
  {
    EnsureDefaultGroup();
  }
  return groups_.empty() ? ActiveEditor() : groups_.front()->editor;
}

const EditorView &EditorWorkspace::PrimaryEditor() const
{
  return const_cast<EditorWorkspace *>(this)->PrimaryEditor();
}

int EditorWorkspace::ActiveDocumentIndex() const
{
  const EditorGroup *group = FindGroup(activeGroupId_);
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
  for (const auto &group : groups_)
  {
    if (group->container)
    {
      InvalidateRect(group->container, nullptr, FALSE);
    }
  }
}

void EditorWorkspace::LoadSyntaxDirectory(const std::wstring &directory)
{
  syntaxDirectory_ = directory;
  std::string error;
  if (!SyntaxRegistry::LoadSharedDirectory(directory, &error))
  {
    OutputDebugStringA(error.c_str());
    OutputDebugStringA("\n");
  }
}

void EditorWorkspace::ApplySettingsToAllEditors(const EditorSettings &settings)
{
  ForEachEditor([&](EditorView &editor) { editor.ApplySettings(settings); });
}

void EditorWorkspace::ApplyThemeToAllEditors(const AppUiTheme &theme)
{
  ForEachEditor([&](EditorView &editor) { editor.ApplyTheme(theme); });
}

void EditorWorkspace::ForEachEditor(const std::function<void(EditorView &)> &fn)
{
  for (auto &group : groups_)
  {
    fn(group->editor);
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
  group->container = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0,
                                     0, 0, 0, host_, nullptr, instance_, nullptr);

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

bool EditorWorkspace::RemoveGroupFromTree(std::unique_ptr<SplitNode> &node, int groupId)
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

std::unique_ptr<EditorWorkspace::SplitNode> EditorWorkspace::CloneTree(const SplitNode *node) const
{
  if (!node)
  {
    return nullptr;
  }

  auto copy = std::make_unique<SplitNode>();
  copy->leaf = node->leaf;
  copy->groupId = node->groupId;
  copy->horizontal = node->horizontal;
  copy->ratio = node->ratio;
  copy->first = CloneTree(node->first.get());
  copy->second = CloneTree(node->second.get());
  return copy;
}

void EditorWorkspace::DestroyGroup(int groupId)
{
  if (groups_.size() <= 1)
  {
    return;
  }

  auto it = std::find_if(groups_.begin(), groups_.end(),
                         [groupId](const auto &g) { return g->id == groupId; });
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

bool EditorWorkspace::SplitGroupLeaf(int targetGroupId, int newGroupId,
                                     EditorSplitDirection direction)
{
  std::function<bool(std::unique_ptr<SplitNode> &)> splitAt =
      [&](std::unique_ptr<SplitNode> &node) -> bool {
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

bool EditorWorkspace::MoveGroupLeaf(int sourceGroupId, int targetGroupId,
                                    EditorSplitDirection direction)
{
  if (!root_ || sourceGroupId == targetGroupId || direction == EditorSplitDirection::None ||
      !FindGroup(sourceGroupId) || !FindGroup(targetGroupId))
  {
    return false;
  }

  std::unique_ptr<SplitNode> original = CloneTree(root_.get());
  if (!RemoveGroupFromTree(root_, sourceGroupId))
  {
    return false;
  }

  if (!SplitGroupLeaf(targetGroupId, sourceGroupId, direction))
  {
    root_ = std::move(original);
    return false;
  }

  return true;
}

void EditorWorkspace::SplitActive(EditorSplitDirection direction)
{
  if (direction == EditorSplitDirection::None)
  {
    return;
  }

  EditorGroup *source = FindGroup(activeGroupId_);
  if (!source)
  {
    return;
  }

  const int sourceGroupId = activeGroupId_;
  const int docIndex = ActiveDocumentIndex();
  if (docIndex < 0)
  {
    return;
  }

  const int newGroupId = CreateGroup();
  EditorGroup *target = FindGroup(newGroupId);
  if (!target)
  {
    return;
  }

  if (!SplitGroupLeaf(sourceGroupId, newGroupId, direction))
  {
    DestroyGroup(newGroupId);
    return;
  }

  target->documentIndices.clear();
  target->documentIndices.push_back(docIndex);
  target->selectedTabIndex = 0;

  activeGroupId_ = newGroupId;
  Layout();

  if (notifyHwnd_)
  {
    SendMessageW(notifyHwnd_, WM_WORKSPACE_REQUEST_SYNC, 0, 0);
    SendMessageW(notifyHwnd_, WM_WORKSPACE_TAB_SELECT, 0, static_cast<LPARAM>(newGroupId));
  }
}

void EditorWorkspace::CloseActiveGroup()
{
  DestroyGroup(activeGroupId_);
}

EditorWorkspace::EditorGroup *EditorWorkspace::FindGroup(int groupId)
{
  for (auto &group : groups_)
  {
    if (group->id == groupId)
    {
      return group.get();
    }
  }
  return nullptr;
}

const EditorWorkspace::EditorGroup *EditorWorkspace::FindGroup(int groupId) const
{
  return const_cast<EditorWorkspace *>(this)->FindGroup(groupId);
}

int EditorWorkspace::GetDocumentIndex(int groupId, int localTabIndex) const
{
  const EditorGroup *group = FindGroup(groupId);
  if (!group || localTabIndex < 0 ||
      localTabIndex >= static_cast<int>(group->documentIndices.size()))
  {
    return -1;
  }

  return group->documentIndices[localTabIndex];
}

void EditorWorkspace::FocusGroupTab(int groupId, int localTabIndex,
                                    const std::vector<EditorDocument> &documents)
{
  EditorGroup *group = FindGroup(groupId);
  if (!group)
  {
    return;
  }

  SelectLocalTab(*group, localTabIndex, documents);
}

int EditorWorkspace::FindGroupIdByTabBar(HWND tabBarHwnd) const
{
  for (const auto &group : groups_)
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
  const EditorGroup *group = FindGroup(groupId);
  return group ? group->tabBar.Hwnd() : nullptr;
}

bool EditorWorkspace::IsGroupEmpty(int groupId) const
{
  const EditorGroup *group = FindGroup(groupId);
  return !group || group->documentIndices.empty();
}

int EditorWorkspace::FindEmptySplitSibling(int groupId, EditorSplitDirection direction) const
{
  if (!root_ || direction == EditorSplitDirection::None)
  {
    return -1;
  }

  const SplitNode *parent = nullptr;
  bool isFirstChild = false;

  std::function<bool(const SplitNode *)> findParent = [&](const SplitNode *node) -> bool {
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

  const int siblingId = isFirstChild ? parent->second->groupId : parent->first->groupId;
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

void EditorWorkspace::ReparentGroupEditor(EditorGroup &group)
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

void EditorWorkspace::SwapGroupEditors(EditorGroup &a, EditorGroup &b)
{
  std::swap(a.editor, b.editor);
  ReparentGroupEditor(a);
  ReparentGroupEditor(b);
}

void EditorWorkspace::LayoutNode(SplitNode *node, const RECT &bounds, int tabStripHeight)
{
  if (!node)
  {
    return;
  }

  if (!node->leaf)
  {
    const bool firstEmpty = node->first && node->first->leaf && IsGroupEmpty(node->first->groupId);
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
    EditorGroup *group = FindGroup(node->groupId);
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
  for (const auto &group : groups_)
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
