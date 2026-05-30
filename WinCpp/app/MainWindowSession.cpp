#include "MainWindow.h"

#include "SessionState.h"

#include <filesystem>
#include <fstream>

void MainWindow::LoadSession()

{

  std::filesystem::path sessionPath(editorSettings_.ConfigPath());

  sessionPath += L".session";

  std::ifstream file(sessionPath, std::ios::binary);

  if (!file)

  {

    return;
  }

  const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  SessionState state;

  if (!SessionStateCodec::Deserialize(json, &state))

  {

    return;
  }

  if (state.projectPaneWidth > 0)

  {

    projectPaneWidth_ = state.projectPaneWidth;
  }

  if (state.outputPaneHeight > 0)

  {

    outputPaneHeight_ = state.outputPaneHeight;
  }

  if (!state.projectRoot.empty() && std::filesystem::is_directory(state.projectRoot))

  {

    showProjectPane_ = state.showProjectPane;

    CheckMenuItem(GetMenu(hwnd_), kCmdViewProjectPane,

                  MF_BYCOMMAND | (showProjectPane_ ? MF_CHECKED : MF_UNCHECKED));

    PopulateProjectTree(state.projectRoot);
  }

  for (const SessionTab &tab : state.tabs)

  {

    if (!tab.path.empty())

    {

      OpenFileAtPath(tab.path, false);
    }
  }

  LayoutChildren();
}

void MainWindow::SaveSession()

{

  SessionState state;

  state.activeGroupId = editorWorkspace_.ActiveGroupId();

  state.projectRoot = projectRootPath_;

  state.showProjectPane = showProjectPane_;

  state.projectPaneWidth = projectPaneWidth_;

  state.outputPaneHeight = outputPaneHeight_;

  for (const EditorDocument &doc : documents_)

  {

    if (doc.path.empty())

    {

      continue;
    }

    SessionTab tab;

    tab.path = doc.path;

    tab.pinned = doc.pinned;

    state.tabs.push_back(tab);
  }

  std::filesystem::path sessionPath(editorSettings_.ConfigPath());

  sessionPath += L".session";

  std::ofstream file(sessionPath, std::ios::binary | std::ios::trunc);

  if (file)

  {

    file << SessionStateCodec::Serialize(state);
  }
}
