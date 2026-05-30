#include "EditorWorkspace.h"

#include "MainWindow.h"
#include "TabBarLogic.h"

#include <algorithm>

int EditorWorkspace::LocalIndexForDocument(const EditorGroup &group, int documentIndex) const
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

void EditorWorkspace::SaveGroupEditorState(EditorGroup &group,
                                           const std::vector<EditorDocument> &documents)
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

  EditorDocument &doc = const_cast<EditorDocument &>(documents[docIndex]);
  if (doc.scintillaDoc)
  {
    group.editor.SetDocumentIfNeeded(doc.scintillaDoc);
    doc.modified = group.editor.IsModified();
  }
  else
  {
    void *const shown = group.editor.GetDocumentPointer();
    if (shown)
    {
      doc.scintillaDoc = shown;
      doc.modified = group.editor.IsModified();
    }
  }
}

void EditorWorkspace::SelectLocalTab(EditorGroup &group, int localIndex,
                                     const std::vector<EditorDocument> &documents)
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
    EditorDocument &doc = const_cast<EditorDocument &>(documents[docIndex]);
    if (doc.scintillaDoc)
    {
      group.editor.SetDocumentIfNeeded(doc.scintillaDoc);
      if (!doc.languageOverride.empty())
      {
        group.editor.ApplySyntaxForLanguage(doc.languageOverride);
      }
      else if (!doc.path.empty())
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

void EditorWorkspace::SyncGroup(EditorGroup &group, const std::vector<EditorDocument> &documents)
{
  std::vector<int> order(group.documentIndices.size());
  for (size_t i = 0; i < order.size(); ++i)
  {
    order[i] = static_cast<int>(i);
  }

  std::vector<bool> pinnedFlags;
  pinnedFlags.reserve(group.documentIndices.size());
  for (int docIndex : group.documentIndices)
  {
    const bool pinned = docIndex >= 0 && docIndex < static_cast<int>(documents.size()) &&
                        documents[docIndex].pinned;
    pinnedFlags.push_back(pinned);
  }

  const std::vector<int> sortedOrder = TabBarLogic::SortIndicesPinnedFirst(order, pinnedFlags);

  std::vector<int> newDocumentIndices;
  newDocumentIndices.reserve(group.documentIndices.size());
  std::vector<std::wstring> titles;
  titles.reserve(group.documentIndices.size());

  int newSelected = 0;
  for (size_t sortedPos = 0; sortedPos < sortedOrder.size(); ++sortedPos)
  {
    const int localIndex = sortedOrder[sortedPos];
    const int docIndex = group.documentIndices[static_cast<size_t>(localIndex)];
    newDocumentIndices.push_back(docIndex);

    if (localIndex == group.selectedTabIndex)
    {
      newSelected = static_cast<int>(sortedPos);
    }

    if (docIndex >= 0 && docIndex < static_cast<int>(documents.size()))
    {
      const EditorDocument &doc = documents[docIndex];
      std::wstring title = doc.displayTitle.empty() ? doc.tabTitle : doc.displayTitle;
      if (doc.pinned)
      {
        title = L"\x2022 " + title;
      }
      titles.push_back(std::move(title));
    }
    else
    {
      titles.push_back(L"Untitled");
    }
  }

  group.documentIndices = std::move(newDocumentIndices);
  group.selectedTabIndex = newSelected;
  group.tabBar.SetTabs(titles, group.selectedTabIndex);
}

void EditorWorkspace::SyncGroupTabs(const std::vector<EditorDocument> &documents)
{
  for (auto &group : groups_)
  {
    SyncGroup(*group, documents);
  }
}

void EditorWorkspace::AttachDocumentToActiveGroup(int documentIndex,
                                                  const std::vector<EditorDocument> &documents)
{
  EditorGroup *group = FindGroup(activeGroupId_);
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
                                                const std::vector<EditorDocument> &documents)
{
  AttachDocumentToActiveGroup(documentIndex, documents);
}

void EditorWorkspace::RemoveDocumentFromAllGroups(int documentIndex)
{
  for (auto &group : groups_)
  {
    const auto it =
        std::find(group->documentIndices.begin(), group->documentIndices.end(), documentIndex);
    if (it != group->documentIndices.end())
    {
      group->documentIndices.erase(it);
      if (group->selectedTabIndex >= static_cast<int>(group->documentIndices.size()))
      {
        group->selectedTabIndex = group->documentIndices.empty()
                                      ? 0
                                      : static_cast<int>(group->documentIndices.size()) - 1;
      }
    }
  }
}

int EditorWorkspace::DocumentReferenceCount(int documentIndex) const
{
  int count = 0;
  for (const auto &group : groups_)
  {
    count += static_cast<int>(
        std::count(group->documentIndices.begin(), group->documentIndices.end(), documentIndex));
  }
  return count;
}

bool EditorWorkspace::RemoveDocumentFromGroup(int groupId, int localTabIndex,
                                              const std::vector<EditorDocument> &documents)
{
  EditorGroup *group = FindGroup(groupId);
  if (!group || localTabIndex < 0 ||
      localTabIndex >= static_cast<int>(group->documentIndices.size()))
  {
    return false;
  }

  const int oldSelected = group->selectedTabIndex;
  group->documentIndices.erase(group->documentIndices.begin() + localTabIndex);

  if (group->documentIndices.empty())
  {
    if (groups_.size() > 1)
    {
      DestroyGroup(groupId);
    }
    else
    {
      group->selectedTabIndex = 0;
      SyncGroup(*group, documents);
    }
    Layout();
    return true;
  }

  int newSelected = oldSelected;
  if (newSelected > localTabIndex)
  {
    --newSelected;
  }
  else if (newSelected == localTabIndex)
  {
    newSelected = (std::min)(localTabIndex, static_cast<int>(group->documentIndices.size()) - 1);
  }

  newSelected =
      (std::max)(0, (std::min)(newSelected, static_cast<int>(group->documentIndices.size()) - 1));
  group->selectedTabIndex = newSelected;
  SyncGroup(*group, documents);
  SelectLocalTab(*group, group->selectedTabIndex, documents);
  Layout();
  return true;
}

void EditorWorkspace::ReindexDocumentsAfterClose(int closedIndex)
{
  for (auto &group : groups_)
  {
    for (int &docIndex : group->documentIndices)
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
    for (const auto &group : groups_)
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
  for (auto &group : groups_)
  {
    for (int &docIndex : group->documentIndices)
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
  EditorGroup *group = FindGroup(groupId);
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
