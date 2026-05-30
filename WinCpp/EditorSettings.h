#pragma once

#include <filesystem>
#include <string>

struct EditorSettings
{
  int tabSize = 4;
  bool tabToSpaces = false;
  bool autoIndent = true;
  bool matchBrace = true;
  bool wordWrap = false;
  int fontSize = 11;
  int zoom = 0;
  bool trimTrailingWhitespaceOnSave = false;

  static EditorSettings Defaults();

  void Load();
  void Save() const;

  void SetConfigPathOverride(const std::wstring& path);
  void ClearConfigPathOverride();

  std::wstring ConfigPath() const;

private:
  std::filesystem::path configPathOverride_;
};
