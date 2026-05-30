#include "EditorWorkspace.h"

#include "EditorSplitDrop.h"
#include "MainWindow.h"
#include "UiTheme.h"

#include <algorithm>
#include <windowsx.h>

namespace
{
EditorWorkspace *GetWorkspaceFromHost(HWND hwnd)
{
  return reinterpret_cast<EditorWorkspace *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

EditorWorkspace *GetWorkspaceFromOverlay(HWND hwnd)
{
  return reinterpret_cast<EditorWorkspace *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}
} // namespace

EditorWorkspace::EditorGroup *EditorWorkspace::GroupAtPoint(POINT screenPoint, RECT *groupBounds)
{
  for (auto &group : groups_)
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

EditorWorkspace::EditorGroup *EditorWorkspace::TabBarGroupAtPoint(POINT screenPoint)
{
  for (auto &group : groups_)
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

void EditorWorkspace::ShowDropPreview(EditorGroup *target, const EditorSplitDropResult &drop)
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
  EditorGroup *source = FindGroup(sourceGroupId);
  EditorGroup *target = FindGroup(targetGroupId);
  if (!source || !target || localTabIndex < 0 ||
      localTabIndex >= static_cast<int>(source->documentIndices.size()))
  {
    return;
  }

  const bool wantsSplit = splitDirection != EditorSplitDirection::None;
  const bool sameGroup = sourceGroupId == targetGroupId;
  const int sourceCount = static_cast<int>(source->documentIndices.size());
  if (wantsSplit && sameGroup && sourceCount < 2)
  {
    return;
  }

  if (wantsSplit && !sameGroup && sourceCount == 1)
  {
    if (MoveGroupLeaf(sourceGroupId, targetGroupId, splitDirection))
    {
      activeGroupId_ = sourceGroupId;
      if (notifyHwnd_)
      {
        SendMessageW(notifyHwnd_, WM_WORKSPACE_REQUEST_SYNC, 0, 0);
        SendMessageW(notifyHwnd_, WM_WORKSPACE_TAB_SELECT,
                     static_cast<WPARAM>(source->selectedTabIndex),
                     static_cast<LPARAM>(sourceGroupId));
      }
      Layout();
    }
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
  const bool intraGroupSplit = wantsSplit && sameGroup;

  if (intraGroupSplit || wantsSplit)
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
      EditorGroup *newGroup = FindGroup(destGroupId);
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

    EditorGroup *destGroup = FindGroup(destGroupId);
    if (!destGroup)
    {
      source->documentIndices.insert(source->documentIndices.begin() + localTabIndex, docIndex);
      return;
    }

    destGroup->documentIndices.push_back(docIndex);
    destGroup->selectedTabIndex = LocalIndexForDocument(*destGroup, docIndex);

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

  // After splitting within the same pane, the source leaf stays in the tree
  // (possibly with no tabs). Destroying it would remove a split child and
  // collapse the layout.
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
    EditorGroup *active = FindGroup(activeGroupId_);
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

  EditorGroup *target = GroupAtPoint(screenPoint, nullptr);
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

  EditorGroup *source = FindGroup(groupId);
  if (!source || tabIndex < 0 || tabIndex >= static_cast<int>(source->documentIndices.size()))
  {
    return;
  }

  EditorGroup *target = GroupAtPoint(screenPoint, nullptr);
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

  if (EditorGroup *targetTabBar = TabBarGroupAtPoint(screenPoint))
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

  for (const auto &group : groups_)
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
  for (const auto &group : groups_)
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
  for (auto &group : groups_)
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

LRESULT CALLBACK EditorWorkspace::WorkspaceHostProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                    LPARAM lParam)
{
  EditorWorkspace *workspace = nullptr;
  if (msg == WM_NCCREATE)
  {
    auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    workspace = reinterpret_cast<EditorWorkspace *>(create->lpCreateParams);
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
  EditorWorkspace *workspace = nullptr;
  if (msg == WM_NCCREATE)
  {
    auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    workspace = reinterpret_cast<EditorWorkspace *>(create->lpCreateParams);
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
      const UiThemeChrome &chrome = UiTheme::Resolve(hwnd).chrome;
      const COLORREF fillColor = merge ? chrome.dropMergeFill : chrome.dropFill;
      const COLORREF borderColor = merge ? chrome.dropMergeBorder : chrome.dropBorder;

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
