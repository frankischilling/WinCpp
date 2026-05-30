#pragma once

#include <string>
#include <vector>

struct SessionTab
{
  std::wstring path;
  int groupId = 0;
  bool pinned = false;
};

struct SessionState
{
  std::vector<SessionTab> tabs;
  int activeGroupId = 0;
  int activeTabIndex = 0;
  std::wstring projectRoot;
  bool showProjectPane = false;
  int projectPaneWidth = 240;
  int outputPaneHeight = 180;
};

namespace SessionStateCodec
{
std::string Serialize(const SessionState& state);
bool Deserialize(const std::string& json, SessionState* state);
}
