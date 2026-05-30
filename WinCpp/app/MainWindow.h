#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "CommandPalette.h"
#include "EditorSettings.h"
#include "EditorView.h"
#include "EditorWorkspace.h"
#include "RecentFilesStore.h"
#include "TabBar.h"
#include "UiTheme.h"

struct EditorDocument
{
  void *scintillaDoc = nullptr;
  std::wstring path;
  std::wstring tabTitle;
  std::wstring displayTitle;
  std::wstring languageOverride;
  bool modified = false;
  bool pinned = false;
};

class MainWindow
{
public:
  MainWindow();
  bool Create(HINSTANCE instance, int nCmdShow);
  HWND Hwnd() const
  {
    return hwnd_;
  }
  bool TranslateAcceleratorMessage(MSG *msg);
  LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR subclassId, DWORD_PTR refData);
  static LRESULT CALLBACK ProjectPaneHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                        LPARAM lParam, UINT_PTR subclassId,
                                                        DWORD_PTR refData);
  static LRESULT CALLBACK OutputPaneHeaderSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                       LPARAM lParam, UINT_PTR subclassId,
                                                       DWORD_PTR refData);
  bool IsPointOverTabBar(POINT screenPoint) const;
  void CreateMenus();
  void CreateAccelerators();
  void CreateChildPanes();
  void LayoutChildren();
  void MeasureTabStripHeight();
  void SyncTabBar();
  void ApplyUiTheme();
  void ShowLanguageOverrideMenu();
  void OnStatusBarClick(const NMHDR *header);
  void BeginProjectSplitterDrag(int clientX);
  void BeginOutputSplitterDrag(int clientY);
  void ContinueSplitterDrag(POINT clientPoint);
  void EndSplitterDrag();
  void CreateToolbar();
  void EnsureTreeImageList();
  int ToolbarHeight() const;
  void CenterDialogOnWindow(HWND dialog, int dialogWidth, int dialogHeight) const;
  void InvalidateTabs();

  void NewFile();
  void OpenFile();
  void OpenFileAtPath(const std::wstring &path, bool addToRecents);
  void OpenFolder();
  void SaveFile();
  void SaveFileAs();
  void SaveAllFiles();
  bool SaveDocumentAt(int index);
  void UpdateStatusBar();
  void UpdateRecentFilesMenu();
  void ClearRecentFiles();
  void ToggleProjectPane();
  void ToggleOutputPane();
  void ToggleWordWrap();
  void ToggleCodeFolding();
  void ZoomIn();
  void ZoomOut();
  void ZoomReset();
  void PinActiveTab();
  void ShowFindReplaceDialog(bool replaceMode);
  void ShowGoToLineDialog();
  void ShowCommandPalette();
  void ShowRunCommandDialog();
  void FindFromDialog(HWND dialog, bool forward);
  void OnFindDialogDestroyed(HWND dialog);
  void FocusFindEdit(HWND dialog, bool selectAll);
  void LoadSession();
  void SaveSession();
  void ApplyEditorSettingsToAllGroups();
  void ApplySyntaxForDocument(int index);
  void PopulateProjectTree(const std::wstring &rootPath);
  void ClearProjectTree();
  void AddTreeDirectory(HTREEITEM parent, const std::filesystem::path &directory);
  HTREEITEM InsertTreeItem(HTREEITEM parent, const std::wstring &label, bool isDirectory);
  void OnProjectTreeDoubleClick();
  void EnsureInitialDocument();
  void CreateUntitledDocument();
  int FindDocumentByPath(const std::wstring &path) const;
  void SwitchToDocument(int index);
  void AttachEditorToDocument(int index);
  void AddDocumentTab(const std::wstring &path, void *document);
  void UpdateActiveTabTitle();
  void SyncDocumentDisplayTitle(int index);
  std::wstring MakeUntitledName();
  std::wstring TabLabelForPath(const std::wstring &path) const;
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
  HWND outputPaneHeader_;
  HWND outputSplitter_;
  HWND outputPane_;
  HWND statusBar_;
  HWND rebar_;
  HWND toolbar_;
  HWND toolbarTooltip_;
  EditorWorkspace editorWorkspace_;
  HACCEL accelerators_;

  std::vector<EditorDocument> documents_;
  int activeDocumentIndex_;
  int untitledCounter_;
  int contextMenuTabIndex_;

  RecentFilesStore recentFiles_;
  EditorSettings editorSettings_;
  HMENU recentFilesMenu_;
  HMENU tabContextMenu_;
  std::unordered_map<HTREEITEM, std::wstring> treeItemPaths_;
  std::wstring projectRootPath_;
  std::wstring projectFilterText_;

  int projectPaneWidth_;
  int projectPaneHeaderHeight_;
  int tabStripHeight_;
  int outputPaneHeight_;
  int toolbarHeight_;
  bool draggingProjectSplitter_;
  bool draggingOutputSplitter_;
  int dragSplitterAnchor_;
  int dragSplitterStartSize_;
  bool showProjectPane_;
  bool showOutputPane_;
  bool wordWrapEnabled_;
  bool codeFoldingEnabled_;
  HWND findDialog_ = nullptr;
  HWND projectFilterEdit_ = nullptr;
  HBRUSH chromeBackgroundBrush_ = nullptr;
  HBRUSH chromeSidebarHeaderBrush_ = nullptr;
  HBRUSH chromeSidebarBorderBrush_ = nullptr;
  HIMAGELIST treeImageList_ = nullptr;
  HFONT outputFont_ = nullptr;
  CommandPalette commandPalette_;
};
