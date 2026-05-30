#pragma once

#include "EditorSettings.h"

#include <string>
#include <vector>

namespace ConfigJson
{
std::wstring Utf8ToWide(const std::string& text);
std::string WideToUtf8(const std::wstring& text);
std::string EscapeString(const std::wstring& text);

void ParseRecentFilesArray(const std::string& json, std::vector<std::wstring>& items);
bool ParseEditorSettings(const std::string& json, EditorSettings& settings);

void WriteConfigFile(const std::wstring& path, const std::vector<std::wstring>& recentFiles,
                     const EditorSettings& editor);
}
