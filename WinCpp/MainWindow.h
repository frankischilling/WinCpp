#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "EditorView.h"
#include "EditorWorkspace.h"
#include "RecentFilesStore.h"
#include "TabBar.h"

struct EditorDocument
{
  void* scintillaDoc = nullptr;
  std::wstring path;
  std::wstring tabTitle;
  std::wstring displayTitle;
  bool modified = false;
};

class MainWindow
{
public:
  MainWindow();
  bool Create(HINSTANCE instance, int nCmdShow);
  HWND Hwnd() const { return hwnd_; }
  bool TranslateAcceleratorMessage(MSG* msg);
  LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR subclassId, DWORD_PTR refData);
  static LRESULT CALLBACK ProjectPaneHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                        LPARAM lParam, UINT_PTR subclassId,
                                                        DWORD_PTR refData);
  bool IsPointOverTabBar(POINT screenPoint) const;
  void CreateMenus();
  void CreateAccelerators();
  void CreateChildPanes();
  void LayoutChildren();
  void MeasureTabStripHeight();
  void SyncTabBar();
  void SyncChromeColors();
  void CenterDialogOnWindow(HWND dialog, int dialogWidth, int dialogHeight) const;
  void InvalidateTabs();

  void NewFile();
  void OpenFile();
  void OpenFileAtPath(const std::wstring& path, bool addToRecents);
  void OpenFolder();
  void SaveFile();
  void SaveFileAs();
  bool SaveDocumentAt(int index);
  void UpdateStatusBar();
  void UpdateRecentFilesMenu();
  void ClearRecentFiles();
  void ToggleProjectPane();
  void ToggleOutputPane();
  void ToggleWordWrap();
  void ShowFindReplaceDialog(bool replaceMode);
  void ShowGoToLineDialog();
  void PopulateProjectTree(const std::wstring& rootPath);
  void ClearProjectTree();
  void AddTreeDirectory(HTREEITEM parent, const std::filesystem::path& directory);
  HTREEITEM InsertTreeItem(HTREEITEM parent, const std::wstring& label, bool isDirectory);
  void OnProjectTreeDoubleClick();
  void EnsureInitialDocument();
  void CreateUntitledDocument();
  int FindDocumentByPath(const std::wstring& path) const;
  void SwitchToDocument(int index);
  void AttachEditorToDocument(int index);
  void AddDocumentTab(const std::wstring& path, void* document);
  void UpdateActiveTabTitle();
  void SyncDocumentDisplayTitle(int index);
  std::wstring MakeUntitledName();
  std::wstring TabLabelForPath(const std::wstring& path) const;
  std::wstring BuildDisplayTitle(int index) const;
  void SaveCurrentDocumentState();
  bool PromptSaveDocument(int index);
  void CloseDocumentAt(int index);
  void ReorderDocument(int fromIndex, int insertBefore);
  void CloseOtherDocuments(int keepIndex);
  void CloseAllDocuments();
  void ShowTabContextMenu(int tabIndex);
  void ShowCreditsDialog();
  void SplitEditor(EditorSplitDirection direction);
  void OnWorkspaceTabSelect(int groupId, int localTabIndex);
  void OnWorkspaceTabClose(int groupId, int localTabIndex);
  int DocumentIndexForGroupTab(int groupId, int localTabIndex) const;

  friend class EditorWorkspace;

  HWND hwnd_;
  HWND projectPaneHeader_;
  HWND projectPaneDivider_;
  HWND projectTree_;
  HWND outputPane_;
  HWND statusBar_;
  EditorWorkspace editorWorkspace_;
  HACCEL accelerators_;

  std::vector<EditorDocument> documents_;
  int activeDocumentIndex_;
  int untitledCounter_;
  int contextMenuTabIndex_;

  RecentFilesStore recentFiles_;
  HMENU recentFilesMenu_;
  HMENU tabContextMenu_;
  std::unordered_map<HTREEITEM, std::wstring> treeItemPaths_;

  int projectPaneWidth_;
  int projectPaneHeaderHeight_;
  int tabStripHeight_;
  int outputPaneHeight_;
  bool showProjectPane_;
  bool showOutputPane_;
  bool wordWrapEnabled_;
  HBRUSH chromeBackgroundBrush_ = nullptr;
  HBRUSH chromeSidebarHeaderBrush_ = nullptr;
  HBRUSH chromeSidebarBorderBrush_ = nullptr;
};
