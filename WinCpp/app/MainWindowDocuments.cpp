#include "MainWindow.h"

#include "DocumentCollectionLogic.h"
#include "DocumentNaming.h"
#include "MainWindowInternal.h"
#include "SyntaxRegistry.h"

void MainWindow::EnsureInitialDocument()
{
  if (!documents_.empty())
  {
    return;
  }

  EditorView &editor = editorWorkspace_.PrimaryEditor();
  void *document = editor.CreateDocument();
  editor.SetDocument(document);
  editor.Clear();

  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.tabTitle = MakeUntitledName();
  entry.displayTitle = entry.tabTitle;
  documents_.push_back(entry);
  activeDocumentIndex_ = 0;
  editorWorkspace_.AttachDocumentToActiveGroup(0, documents_);
  SyncTabBar();
}

std::wstring MainWindow::MakeUntitledName()
{
  ++untitledCounter_;
  return L"Untitled-" + std::to_wstring(untitledCounter_);
}

std::wstring MainWindow::TabLabelForPath(const std::wstring &path) const
{
  return DocumentNaming::TabLabelForPath(path);
}

void MainWindow::SaveCurrentDocumentState()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument &doc = documents_[activeDocumentIndex_];
  EditorView &editor = editorWorkspace_.ActiveEditor();
  if (doc.scintillaDoc)
  {
    editor.SetDocumentIfNeeded(doc.scintillaDoc);
    doc.modified = editor.IsModified();
  }
  else
  {
    void *const shown = editor.GetDocumentPointer();
    if (shown)
    {
      doc.scintillaDoc = shown;
      doc.modified = editor.IsModified();
    }
  }
}

void MainWindow::AttachEditorToDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  editorWorkspace_.AttachDocumentToActiveGroup(index, documents_);
  activeDocumentIndex_ = index;
}

void MainWindow::SwitchToDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  SaveCurrentDocumentState();

  if (index == activeDocumentIndex_ &&
      editorWorkspace_.ActiveEditor().GetDocumentPointer() == documents_[index].scintillaDoc)
  {
    ApplySyntaxForDocument(index);
    return;
  }

  editorWorkspace_.OpenDocumentInActiveGroup(index, documents_);
  activeDocumentIndex_ = index;
  UpdateStatusBar();
  SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
}

void MainWindow::AddDocumentTab(const std::wstring &path, void *document)
{
  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.path = path;
  entry.tabTitle = path.empty() ? MakeUntitledName() : TabLabelForPath(path);
  entry.displayTitle = entry.tabTitle;

  documents_.push_back(entry);
  const int index = static_cast<int>(documents_.size()) - 1;
  SwitchToDocument(index);
  SyncTabBar();
}

int MainWindow::FindDocumentByPath(const std::wstring &path) const
{
  for (size_t i = 0; i < documents_.size(); ++i)
  {
    if (!documents_[i].path.empty() && _wcsicmp(documents_[i].path.c_str(), path.c_str()) == 0)
    {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void MainWindow::UpdateActiveTabTitle()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  SyncDocumentDisplayTitle(activeDocumentIndex_);
  const EditorDocument &doc = documents_[activeDocumentIndex_];
  SyncTabBar();
}

void MainWindow::NewFile()
{
  SaveCurrentDocumentState();

  EditorView &editor = editorWorkspace_.PrimaryEditor();
  void *const document = editor.CreateDocument();
  AddDocumentTab(L"", document);
  editor.Clear();
  UpdateStatusBar();
}

void MainWindow::OpenFile()
{
  const std::wstring path = ShowOpenFileDialog(hwnd_);
  if (!path.empty())
  {
    OpenFileAtPath(path, true);
  }
}

void MainWindow::OpenFileAtPath(const std::wstring &path, bool addToRecents)
{
  const int existing = FindDocumentByPath(path);
  if (existing >= 0)
  {
    SwitchToDocument(existing);
    return;
  }

  SaveCurrentDocumentState();

  EditorView &editor = editorWorkspace_.PrimaryEditor();
  void *const document = editor.CreateDocument();
  if (!document)
  {
    MessageBoxW(hwnd_, L"Failed to open file.\n\nEditor is not ready.", L"Open File",
                MB_OK | MB_ICONERROR);
    return;
  }

  std::string error;
  if (!editor.LoadFromFileIntoDocument(document, path, &error))
  {
    editor.ReleaseDocument(document);
    std::wstring message = L"Failed to open file.";
    const std::wstring detail = Utf8ToWide(error);
    if (!detail.empty())
    {
      message += L"\n\n" + detail;
    }
    MessageBoxW(hwnd_, message.c_str(), L"Open File", MB_OK | MB_ICONERROR);
    return;
  }

  AddDocumentTab(path, document);
  UpdateActiveTabTitle();

  if (addToRecents)
  {
    recentFiles_.Add(path);
    UpdateRecentFilesMenu();
  }

  UpdateStatusBar();
}

void MainWindow::OpenFolder()
{
  const std::wstring folder = ShowFolderDialog(hwnd_);
  if (!folder.empty())
  {
    PopulateProjectTree(folder);
  }
}

void MainWindow::SaveFile()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument &doc = documents_[activeDocumentIndex_];
  if (doc.path.empty())
  {
    SaveFileAs();
    return;
  }

  if (editorSettings_.trimTrailingWhitespaceOnSave)
  {
    editorWorkspace_.ActiveEditor().TrimTrailingWhitespace();
  }

  std::string error;
  if (!editorWorkspace_.ActiveEditor().SaveToFile(doc.path, &error))
  {
    std::wstring message = L"Failed to save file.";
    const std::wstring detail = Utf8ToWide(error);
    if (!detail.empty())
    {
      message += L"\n\n" + detail;
    }
    MessageBoxW(hwnd_, message.c_str(), L"Save File", MB_OK | MB_ICONERROR);
    return;
  }

  recentFiles_.Add(doc.path);
  UpdateRecentFilesMenu();
  doc.modified = false;
  UpdateStatusBar();
  UpdateActiveTabTitle();
}

void MainWindow::SaveFileAs()
{
  if (activeDocumentIndex_ < 0 || activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument &doc = documents_[activeDocumentIndex_];
  const std::wstring path = ShowSaveFileDialog(hwnd_, doc.path);
  if (path.empty())
  {
    return;
  }

  std::string error;
  if (!editorWorkspace_.ActiveEditor().SaveToFile(path, &error))
  {
    std::wstring message = L"Failed to save file.";
    const std::wstring detail = Utf8ToWide(error);
    if (!detail.empty())
    {
      message += L"\n\n" + detail;
    }
    MessageBoxW(hwnd_, message.c_str(), L"Save File", MB_OK | MB_ICONERROR);
    return;
  }

  doc.path = path;
  recentFiles_.Add(path);
  UpdateRecentFilesMenu();
  doc.modified = false;
  UpdateStatusBar();
  UpdateActiveTabTitle();
}

std::wstring MainWindow::BuildDisplayTitle(int index) const
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return {};
  }

  const EditorDocument &doc = documents_[index];
  return DocumentNaming::DisplayTitle(doc.path, doc.tabTitle, false);
}

void MainWindow::SyncDocumentDisplayTitle(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  EditorDocument &doc = documents_[index];
  doc.displayTitle = DocumentNaming::DisplayTitle(doc.path, doc.tabTitle, doc.modified);
  if (doc.path.empty())
  {
    std::wstring base = doc.displayTitle;
    if (base.size() >= 2 && base.compare(base.size() - 2, 2, L" *") == 0)
    {
      base.resize(base.size() - 2);
    }
    doc.tabTitle = base;
  }
}

bool MainWindow::SaveDocumentAt(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return false;
  }

  const int previous = activeDocumentIndex_;
  SwitchToDocument(index);

  EditorDocument &doc = documents_[index];
  if (doc.path.empty())
  {
    SaveFileAs();
  }
  else
  {
    SaveFile();
  }

  const bool saved = !editorWorkspace_.ActiveEditor().IsModified();
  documents_[index].modified = !saved;
  if (previous >= 0 && previous < static_cast<int>(documents_.size()) && previous != index)
  {
    SwitchToDocument(previous);
  }
  return saved;
}

bool MainWindow::PromptSaveDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return true;
  }

  if (index == activeDocumentIndex_)
  {
    documents_[index].modified = editorWorkspace_.ActiveEditor().IsModified();
  }

  if (!documents_[index].modified)
  {
    return true;
  }

  std::wstring prompt = L"Save changes to ";
  prompt += documents_[index].displayTitle.empty() ? BuildDisplayTitle(index)
                                                   : documents_[index].displayTitle;
  prompt += L"?";

  const int result =
      MessageBoxW(hwnd_, prompt.c_str(), L"WinCpp", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (result == IDCANCEL)
  {
    return false;
  }

  if (result == IDYES)
  {
    return SaveDocumentAt(index);
  }

  return true;
}

void MainWindow::CreateUntitledDocument()
{
  EditorView &editor = editorWorkspace_.PrimaryEditor();
  void *const document = editor.CreateDocument();

  EditorDocument entry;
  entry.scintillaDoc = document;
  entry.tabTitle = MakeUntitledName();
  entry.displayTitle = entry.tabTitle;
  documents_.push_back(entry);
  AttachEditorToDocument(0);
  editorWorkspace_.ActiveEditor().Clear();
  UpdateStatusBar();
  SyncTabBar();
  SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
}

void MainWindow::ReorderDocument(int fromIndex, int insertBefore)
{
  const int count = static_cast<int>(documents_.size());
  if (fromIndex < 0 || fromIndex >= count || insertBefore < 0 || insertBefore > count)
  {
    return;
  }

  if (insertBefore == fromIndex || insertBefore == fromIndex + 1)
  {
    return;
  }

  SaveCurrentDocumentState();

  void *const activeDoc = (activeDocumentIndex_ >= 0 && activeDocumentIndex_ < count)
                              ? documents_[activeDocumentIndex_].scintillaDoc
                              : nullptr;

  EditorDocument moved = std::move(documents_[fromIndex]);
  documents_.erase(documents_.begin() + fromIndex);

  int target = insertBefore;
  if (fromIndex < insertBefore)
  {
    --target;
  }

  documents_.insert(documents_.begin() + target, std::move(moved));

  editorWorkspace_.ReindexDocumentsAfterReorder(fromIndex, target);

  if (activeDoc)
  {
    for (int i = 0; i < static_cast<int>(documents_.size()); ++i)
    {
      if (documents_[i].scintillaDoc == activeDoc)
      {
        activeDocumentIndex_ = i;
        break;
      }
    }
  }

  SyncTabBar();
}

void MainWindow::CloseDocumentAt(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  if (!PromptSaveDocument(index))
  {
    return;
  }

  void *const documentToClose = documents_[index].scintillaDoc;
  const bool closingOnlyTab = documents_.size() == 1;

  void *const editorDocument = editorWorkspace_.ActiveEditor().GetDocumentPointer();

  if (editorDocument == documentToClose)
  {
    if (closingOnlyTab)
    {
      void *const replacement = editorWorkspace_.PrimaryEditor().CreateDocument();
      editorWorkspace_.ActiveEditor().SetDocumentIfNeeded(replacement);
      editorWorkspace_.ActiveEditor().ReleaseDocument(documentToClose);

      documents_.clear();

      EditorDocument entry;
      entry.scintillaDoc = replacement;
      entry.tabTitle = MakeUntitledName();
      entry.displayTitle = entry.tabTitle;
      documents_.push_back(entry);
      activeDocumentIndex_ = 0;
      editorWorkspace_.EnsureDefaultGroup();
      editorWorkspace_.AttachDocumentToActiveGroup(0, documents_);
      UpdateStatusBar();
      SyncTabBar();
      LayoutChildren();
      SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
      return;
    }

    const int switchIndex = index > 0 ? index - 1 : 1;
    editorWorkspace_.ActiveEditor().SetDocumentIfNeeded(documents_[switchIndex].scintillaDoc);
    activeDocumentIndex_ = switchIndex;
  }
  else
  {
    SaveCurrentDocumentState();
  }

  if (editorWorkspace_.ActiveEditor().GetDocumentPointer() == documentToClose)
  {
    for (size_t i = 0; i < documents_.size(); ++i)
    {
      if (static_cast<int>(i) != index)
      {
        editorWorkspace_.ActiveEditor().SetDocumentIfNeeded(documents_[i].scintillaDoc);
        break;
      }
    }
  }

  editorWorkspace_.ActiveEditor().ReleaseDocument(documentToClose);
  editorWorkspace_.RemoveDocumentFromAllGroups(index);
  editorWorkspace_.PruneEmptyGroups();

  documents_.erase(documents_.begin() + index);
  editorWorkspace_.ReindexDocumentsAfterClose(index);

  if (documents_.empty())
  {
    activeDocumentIndex_ = -1;
    CreateUntitledDocument();
    LayoutChildren();
    return;
  }

  if (index < activeDocumentIndex_)
  {
    activeDocumentIndex_--;
  }

  if (activeDocumentIndex_ >= static_cast<int>(documents_.size()))
  {
    activeDocumentIndex_ = static_cast<int>(documents_.size()) - 1;
  }

  if (activeDocumentIndex_ < 0)
  {
    activeDocumentIndex_ = 0;
  }

  AttachEditorToDocument(activeDocumentIndex_);
  editorWorkspace_.PruneEmptyGroups();
  UpdateStatusBar();
  UpdateActiveTabTitle();
  SyncTabBar();
  LayoutChildren();
  SetFocus(editorWorkspace_.ActiveEditor().Hwnd());
}

void MainWindow::CloseOtherDocuments(int keepIndex)
{
  if (keepIndex < 0 || keepIndex >= static_cast<int>(documents_.size()))
  {
    return;
  }

  for (int i = static_cast<int>(documents_.size()) - 1; i >= 0; --i)
  {
    if (i != keepIndex)
    {
      CloseDocumentAt(i);
      if (keepIndex >= static_cast<int>(documents_.size()))
      {
        keepIndex = static_cast<int>(documents_.size()) - 1;
      }
    }
  }
}

void MainWindow::CloseAllDocuments()
{
  while (documents_.size() > 1)
  {
    const size_t countBefore = documents_.size();
    CloseDocumentAt(static_cast<int>(documents_.size()) - 1);
    if (documents_.size() >= countBefore)
    {
      return;
    }
  }

  if (!documents_.empty())
  {
    CloseDocumentAt(0);
  }
}

void MainWindow::ApplySyntaxForDocument(int index)
{
  if (index < 0 || index >= static_cast<int>(documents_.size()))
  {
    return;
  }

  const EditorDocument &doc = documents_[index];
  EditorView &editor = editorWorkspace_.ActiveEditor();
  if (!doc.languageOverride.empty())
  {
    editor.ApplySyntaxForLanguage(doc.languageOverride);
  }
  else if (!doc.path.empty())
  {
    editor.ApplySyntaxForPath(doc.path);
  }
  else
  {
    editor.ApplySyntaxForLanguage(L"");
  }
}

void MainWindow::SaveAllFiles()
{
  std::vector<DocumentState> states;
  states.reserve(documents_.size());
  for (const EditorDocument &doc : documents_)
  {
    states.push_back({doc.modified, doc.path});
  }

  for (size_t index : DocumentCollectionLogic::IndicesNeedingSave(states))
  {
    if (!SaveDocumentAt(static_cast<int>(index)))
    {
      break;
    }
  }
}
