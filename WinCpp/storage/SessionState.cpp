#include "SessionState.h"

#include "ConfigJson.h"

namespace
{
std::wstring ParseJsonStringValue(const std::string& json, const char* key, size_t searchFrom)
{
  const size_t keyPos = json.find(key, searchFrom);
  if (keyPos == std::string::npos)
  {
    return {};
  }

  size_t stringPos = json.find(':', keyPos);
  if (stringPos == std::string::npos)
  {
    return {};
  }
  stringPos = json.find('"', stringPos);
  if (stringPos == std::string::npos)
  {
    return {};
  }
  ++stringPos;

  std::string value;
  while (stringPos < json.size())
  {
    const char ch = json[stringPos++];
    if (ch == '"')
    {
      return ConfigJson::Utf8ToWide(value);
    }
    if (ch == '\\' && stringPos < json.size())
    {
      const char esc = json[stringPos++];
      if (esc == 'n')
      {
        value.push_back('\n');
      }
      else if (esc == 'r')
      {
        value.push_back('\r');
      }
      else if (esc == 't')
      {
        value.push_back('\t');
      }
      else
      {
        value.push_back(esc);
      }
      continue;
    }
    value.push_back(ch);
  }

  return {};
}

bool ParseBoolValue(const std::string& json, const char* key, size_t searchFrom, bool& out)
{
  const size_t keyPos = json.find(key, searchFrom);
  if (keyPos == std::string::npos)
  {
    return false;
  }

  const size_t colon = json.find(':', keyPos);
  if (colon == std::string::npos)
  {
    return false;
  }

  size_t pos = colon + 1;
  while (pos < json.size() &&
         (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
  {
    ++pos;
  }

  if (json.compare(pos, 4, "true") == 0)
  {
    out = true;
    return true;
  }

  if (json.compare(pos, 5, "false") == 0)
  {
    out = false;
    return true;
  }

  return false;
}
}

namespace SessionStateCodec
{
std::string Serialize(const SessionState& state)
{
  std::string json = "{\n  \"session\": {\n    \"activeGroupId\": " +
                     std::to_string(state.activeGroupId) + ",\n    \"activeTabIndex\": " +
                     std::to_string(state.activeTabIndex) + ",\n    \"projectRoot\": " +
                     ConfigJson::EscapeString(state.projectRoot) + ",\n    \"showProjectPane\": " +
                     (state.showProjectPane ? "true" : "false") + ",\n    \"projectPaneWidth\": " +
                     std::to_string(state.projectPaneWidth) + ",\n    \"outputPaneHeight\": " +
                     std::to_string(state.outputPaneHeight) + ",\n    \"tabs\": [\n";
  for (size_t i = 0; i < state.tabs.size(); ++i)
  {
    const SessionTab& tab = state.tabs[i];
    json += "      { \"path\": " + ConfigJson::EscapeString(tab.path) +
            ", \"groupId\": " + std::to_string(tab.groupId) +
            ", \"pinned\": " + (tab.pinned ? "true" : "false") + " }";
    if (i + 1 < state.tabs.size())
    {
      json += ",";
    }
    json += "\n";
  }
  json += "    ]\n  }\n}\n";
  return json;
}

bool Deserialize(const std::string& json, SessionState* state)
{
  if (!state)
  {
    return false;
  }

  state->tabs.clear();
  const size_t sessionPos = json.find("\"session\"");
  if (sessionPos == std::string::npos)
  {
    return false;
  }

  size_t pos = 0;
  const auto readInt = [&](const char* key, int& out) {
    const size_t keyPos = json.find(key, sessionPos);
    if (keyPos == std::string::npos)
    {
      return;
    }
    const size_t colon = json.find(':', keyPos);
    if (colon == std::string::npos)
    {
      return;
    }
    out = std::stoi(json.substr(colon + 1));
  };

  readInt("\"activeGroupId\"", state->activeGroupId);
  readInt("\"activeTabIndex\"", state->activeTabIndex);
  readInt("\"projectPaneWidth\"", state->projectPaneWidth);
  readInt("\"outputPaneHeight\"", state->outputPaneHeight);
  state->projectRoot = ParseJsonStringValue(json, "\"projectRoot\"", sessionPos);
  ParseBoolValue(json, "\"showProjectPane\"", sessionPos, state->showProjectPane);

  const size_t tabsPos = json.find("\"tabs\"", sessionPos);
  if (tabsPos == std::string::npos)
  {
    return false;
  }

  pos = json.find('[', tabsPos);
  if (pos == std::string::npos)
  {
    return false;
  }

  ++pos;
  while (pos < json.size())
  {
    while (pos < json.size() &&
           (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n' ||
            json[pos] == ','))
    {
      ++pos;
    }

    if (pos >= json.size() || json[pos] == ']')
    {
      break;
    }

    const size_t pathKey = json.find("\"path\"", pos);
    if (pathKey == std::string::npos)
    {
      break;
    }

    size_t stringPos = json.find(':', pathKey);
    if (stringPos == std::string::npos)
    {
      break;
    }
    stringPos = json.find('"', stringPos);
    if (stringPos == std::string::npos)
    {
      break;
    }
    ++stringPos;

    std::wstring path;
    std::string value;
    while (stringPos < json.size())
    {
      const char ch = json[stringPos++];
      if (ch == '"')
      {
        path = ConfigJson::Utf8ToWide(value);
        break;
      }
      if (ch == '\\' && stringPos < json.size())
      {
        const char esc = json[stringPos++];
        if (esc == 'n')
        {
          value.push_back('\n');
        }
        else
        {
          value.push_back(esc);
        }
        continue;
      }
      value.push_back(ch);
    }

    if (path.empty())
    {
      pos = pathKey + 1;
      continue;
    }

    SessionTab tab;
    tab.path = path;
    const size_t groupKey = json.find("\"groupId\"", stringPos);
    if (groupKey != std::string::npos)
    {
      const size_t colon = json.find(':', groupKey);
      if (colon != std::string::npos)
      {
        tab.groupId = std::stoi(json.substr(colon + 1));
      }
    }

    const size_t pinnedKey = json.find("\"pinned\"", stringPos);
    if (pinnedKey != std::string::npos)
    {
      const size_t colon = json.find(':', pinnedKey);
      if (colon != std::string::npos)
      {
        size_t valuePos = colon + 1;
        while (valuePos < json.size() &&
               (json[valuePos] == ' ' || json[valuePos] == '\t' || json[valuePos] == '\r' ||
                json[valuePos] == '\n'))
        {
          ++valuePos;
        }
        tab.pinned = json.compare(valuePos, 4, "true") == 0;
      }
    }

    state->tabs.push_back(tab);
    pos = json.find('}', stringPos);
    if (pos == std::string::npos)
    {
      break;
    }
    ++pos;
  }

  return true;
}
}
