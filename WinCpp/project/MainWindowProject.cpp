#include "MainWindow.h"

#include "PathLineParser.h"
#include "TreeFilterLogic.h"
#include "UiMetrics.h"

#include <algorithm>
#include <commctrl.h>
#include <filesystem>
#include <shellapi.h>

void MainWindow::EnsureTreeImageList()

{

  if (treeImageList_ || !projectTree_)

  {

    return;
  }

  treeImageList_ = ImageList_Create(UiMetrics::ScalePx(hwnd_, 16), UiMetrics::ScalePx(hwnd_, 16),

                                    ILC_COLOR32 | ILC_MASK, 4, 4);

  if (!treeImageList_)

  {

    return;
  }

  SHFILEINFOW info = {};

  if (SHGetFileInfoW(L"C:\\", FILE_ATTRIBUTE_DIRECTORY, &info, sizeof(info),

                     SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))

  {

    ImageList_AddIcon(treeImageList_, info.hIcon);

    DestroyIcon(info.hIcon);
  }

  else

  {

    ImageList_AddIcon(treeImageList_, LoadIconW(nullptr, IDI_APPLICATION));
  }

  if (SHGetFileInfoW(L".txt", FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),

                     SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))

  {

    ImageList_AddIcon(treeImageList_, info.hIcon);

    DestroyIcon(info.hIcon);
  }

  else

  {

    ImageList_AddIcon(treeImageList_, LoadIconW(nullptr, IDI_APPLICATION));
  }

  TreeView_SetImageList(projectTree_, treeImageList_, TVSIL_NORMAL);
}

HTREEITEM MainWindow::InsertTreeItem(HTREEITEM parent, const std::wstring &label, bool isDirectory)

{

  EnsureTreeImageList();

  TVINSERTSTRUCTW insert = {};

  insert.hParent = parent ? parent : TVI_ROOT;

  insert.hInsertAfter = TVI_LAST;

  insert.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

  insert.item.pszText = const_cast<LPWSTR>(label.c_str());

  insert.item.iImage = isDirectory ? 0 : 1;

  insert.item.iSelectedImage = insert.item.iImage;

  if (!isDirectory && treeImageList_)

  {

    SHFILEINFOW info = {};

    const std::wstring query = label.empty() ? L".txt" : label;

    if (SHGetFileInfoW(query.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),

                       SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))

    {

      const int index = ImageList_AddIcon(treeImageList_, info.hIcon);

      DestroyIcon(info.hIcon);

      if (index >= 0)

      {

        insert.item.iImage = index;

        insert.item.iSelectedImage = index;
      }
    }
  }

  return TreeView_InsertItem(projectTree_, &insert);
}

void MainWindow::AddTreeDirectory(HTREEITEM parent, const std::filesystem::path &directory)

{

  std::vector<std::filesystem::directory_entry> entries;

  std::error_code ec;

  for (const auto &entry : std::filesystem::directory_iterator(directory, ec))

  {

    if (ec)

    {

      break;
    }

    entries.push_back(entry);
  }

  std::sort(
      entries.begin(), entries.end(),

      [](const std::filesystem::directory_entry &a, const std::filesystem::directory_entry &b) {
        if (a.is_directory() != b.is_directory())

        {

          return a.is_directory() > b.is_directory();
        }

        return a.path().filename().wstring() < b.path().filename().wstring();
      });

  for (const auto &entry : entries)

  {

    const std::wstring name = entry.path().filename().wstring();

    if (name.empty() || name == L"." || name == L"..")

    {

      continue;
    }

    if (entry.is_directory(ec))

    {

      const HTREEITEM folderItem = InsertTreeItem(parent, name, true);

      AddTreeDirectory(folderItem, entry.path());
    }

    else if (entry.is_regular_file(ec))

    {

      if (!projectRootPath_.empty() && !projectFilterText_.empty())

      {

        std::error_code relEc;

        const std::wstring relative =

            std::filesystem::relative(entry.path(), std::filesystem::path(projectRootPath_), relEc)

                .wstring();

        if (!relEc && !TreeFilterLogic::PathMatchesFilter(relative, projectFilterText_))

        {

          continue;
        }
      }

      const HTREEITEM fileItem = InsertTreeItem(parent, name, false);

      treeItemPaths_[fileItem] = entry.path().wstring();
    }
  }
}

void MainWindow::ClearProjectTree()

{

  TreeView_DeleteAllItems(projectTree_);

  treeItemPaths_.clear();
}

void MainWindow::PopulateProjectTree(const std::wstring &rootPath)

{

  projectRootPath_ = rootPath;

  ClearProjectTree();

  const std::filesystem::path root(rootPath);

  std::wstring rootLabel = root.filename().wstring();

  if (rootLabel.empty())

  {

    rootLabel = root.wstring();
  }

  const HTREEITEM rootItem = InsertTreeItem(nullptr, rootLabel, true);

  AddTreeDirectory(rootItem, root);

  TreeView_Expand(projectTree_, rootItem, TVE_EXPAND);
}

void MainWindow::OnProjectTreeDoubleClick()

{

  const HTREEITEM selected = TreeView_GetSelection(projectTree_);

  if (!selected)

  {

    return;
  }

  const auto it = treeItemPaths_.find(selected);

  if (it != treeItemPaths_.end())

  {

    if (const auto target = PathLineParser::Parse(it->second))

    {

      OpenFileAtPath(target->path, true);

      editorWorkspace_.ActiveEditor().GoToLine(target->line);
    }

    else

    {

      OpenFileAtPath(it->second, true);
    }
  }
}
