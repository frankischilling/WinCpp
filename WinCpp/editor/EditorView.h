#pragma once

#include "EditorSearch.h"
#include "EditorSettings.h"
#include "SyntaxHighlighter.h"
#include "UiTheme.h"
#include "SyntaxRegistry.h"
#include "WinCpp.h"

#include <Scintilla.h>

class EditorView
{
public:
  EditorView();
  bool Create(HWND parent, HINSTANCE instance);
  HWND Hwnd() const { return hwnd_; }

  bool LoadFromFile(const std::wstring& path, std::string* error);
  bool SaveToFile(const std::wstring& path, std::string* error);
  void Clear();
  bool IsModified() const;
  void SetSavePoint();

  void* CreateDocument();
  void* GetDocumentPointer() const;
  void SetDocument(void* document);
  void SetDocumentIfNeeded(void* document);
  void ReleaseDocument(void* document);
  bool LoadFromFileIntoDocument(void* document, const std::wstring& path, std::string* error);
  int GetCurrentLine() const;
  int GetCurrentColumn() const;
  int GetSelectionLength() const;
  std::wstring GetSelectedText() const;

  void LoadSyntaxDirectory(const std::wstring& directory);
  void ApplySyntaxForPath(const std::wstring& filePath);
  void ApplySyntaxForLanguage(const std::wstring& languageName);
  std::wstring CurrentLanguageName() const;

  void ApplySettings(const EditorSettings& settings);
  void ApplyTheme(const AppUiTheme& theme);
  const EditorSettings& Settings() const { return settings_; }

  bool FindNext(const std::wstring& text, bool matchCase, bool wholeWord, bool forward);
  bool FindNext(const std::wstring& text, const SearchOptions& options);
  bool ReplaceSelection(const std::wstring& findText, const std::wstring& replaceText,
                        bool matchCase, bool wholeWord, bool regex = false);
  int ReplaceAll(const std::wstring& findText, const std::wstring& replaceText, bool matchCase,
                 bool wholeWord, bool regex = false);
  void GoToLine(int lineNumber);
  void SetWordWrap(bool enabled);
  bool IsWordWrapEnabled() const;

  void OnEditorNotify(const SCNotification& notification);
  void InsertTab();
  void ConvertSelectionTabsToSpaces();
  void ConvertSelectionSpacesToTabs();
  void SetBraceMatching(bool enabled);
  bool GotoMatchingBrace();
  void DuplicateLine();
  void DeleteLine();
  void MoveLineUp();
  void MoveLineDown();
  int TrimTrailingWhitespace();
  void ZoomDelta(int steps);
  int GetZoom() const;
  void ClearFindHighlights();
  void UpdateFindHighlights(const std::wstring& query, const SearchOptions& options);
  void EnableCodeFolding(bool enabled);

  std::string GetTextUtf8() const;

private:
  void InitializeScintilla();
  void UpdateLineNumberMarginWidth();
  void SetTextUtf8(const std::string& text);
  std::string GetFirstLineUtf8() const;
  bool FindNextInBuffer(const std::wstring& text, const SearchOptions& options);
  void ApplyAutoIndentForNewLine();
  void ConfigureFolding();

  HWND hwnd_;
  SyntaxHighlighter highlighter_;
  EditorSettings settings_;
  std::wstring languageName_;
};
