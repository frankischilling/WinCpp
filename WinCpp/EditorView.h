#pragma once

#include "SyntaxHighlighter.h"
#include "SyntaxRegistry.h"
#include "WinCpp.h"

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

  void LoadSyntaxDirectory(const std::wstring& directory);
  void ApplySyntaxForPath(const std::wstring& filePath);

  bool FindNext(const std::wstring& text, bool matchCase, bool wholeWord, bool forward);
  bool ReplaceSelection(const std::wstring& findText, const std::wstring& replaceText,
                        bool matchCase, bool wholeWord);
  int ReplaceAll(const std::wstring& findText, const std::wstring& replaceText,
                 bool matchCase, bool wholeWord);
  void GoToLine(int lineNumber);
  void SetWordWrap(bool enabled);
  bool IsWordWrapEnabled() const;

private:
  void InitializeScintilla();
  void SetTextUtf8(const std::string& text);
  std::string GetTextUtf8() const;
  std::string GetFirstLineUtf8() const;

  HWND hwnd_;
  SyntaxRegistry syntax_;
  SyntaxHighlighter highlighter_;
};
