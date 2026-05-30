#include "EditorView.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <Scintilla.h>

#include "AppIds.h"
#include "IndentLogic.h"
#include "UiHelpers.h"

namespace
{
constexpr int kLineNumberLeftInsetPx = 4;

std::wstring Utf8ToWide(const std::string& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0);
  if (length <= 0)
  {
    return {};
  }

  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
  return result;
}

std::string WideToUtf8(const std::wstring& text)
{
  if (text.empty())
  {
    return {};
  }

  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (length <= 0)
  {
    return {};
  }

  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      result.data(), length, nullptr, nullptr);
  return result;
}
}

EditorView::EditorView() : hwnd_(nullptr) {}

bool EditorView::Create(HWND parent, HINSTANCE instance)
{
  static bool s_scintillaRegistered = false;
  if (!s_scintillaRegistered)
  {
    Scintilla_RegisterClasses(instance);
    s_scintillaRegistered = true;
  }

  hwnd_ = CreateWindowExW(
      0,
      L"Scintilla",
      L"",
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP,
      0,
      0,
      0,
      0,
      parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtrlEditor)),
      instance,
      nullptr);

  if (!hwnd_)
  {
    return false;
  }

  InitializeScintilla();
  highlighter_.Attach(hwnd_);
  return true;
}

void EditorView::LoadSyntaxDirectory(const std::wstring& directory)
{
  std::string error;
  if (!SyntaxRegistry::LoadSharedDirectory(directory, &error))
  {
    OutputDebugStringA(error.c_str());
    OutputDebugStringA("\n");
  }
}

void EditorView::ApplySyntaxForPath(const std::wstring& filePath)
{
  if (!hwnd_ || !SyntaxRegistry::Shared().IsLoaded())
  {
    return;
  }

  if (filePath.empty())
  {
    languageName_.clear();
    highlighter_.Clear();
    return;
  }

  const std::string firstLine = GetFirstLineUtf8();
  const SyntaxDefinition* definition =
      SyntaxRegistry::Shared().DetectForPath(filePath, firstLine);
  if (!definition)
  {
    return;
  }

  highlighter_.Apply(*definition);
  languageName_ = Utf8ToWide(definition->filetype);
}

void EditorView::ApplySyntaxForLanguage(const std::wstring& languageName)
{
  if (!hwnd_ || !SyntaxRegistry::Shared().IsLoaded())
  {
    return;
  }

  languageName_ = languageName;
  if (languageName.empty())
  {
    highlighter_.Clear();
    return;
  }

  const std::string languageUtf8 = WideToUtf8(languageName);
  for (const SyntaxDefinition& definition : SyntaxRegistry::Shared().Definitions())
  {
    if (definition.filetype == languageUtf8)
    {
      highlighter_.Apply(definition);
      return;
    }
  }
}

std::wstring EditorView::CurrentLanguageName() const
{
  return languageName_;
}

bool EditorView::LoadFromFileIntoDocument(void* document, const std::wstring& path, std::string* error)
{
  if (!hwnd_)
  {
    if (error)
    {
      *error = "Editor is not initialized.";
    }
    return false;
  }

  if (!document)
  {
    if (error)
    {
      *error = "Failed to create document buffer.";
    }
    return false;
  }

  void* const previous = GetDocumentPointer();
  if (previous != nullptr)
  {
    SendMessage(hwnd_, SCI_ADDREFDOCUMENT, 0, reinterpret_cast<LPARAM>(previous));
  }

  SetDocument(document);
  const bool loaded = LoadFromFile(path, error);

  if (previous != nullptr)
  {
    SetDocument(previous);
    SendMessage(hwnd_, SCI_RELEASEDOCUMENT, 0, reinterpret_cast<LPARAM>(previous));
  }

  return loaded;
}

bool EditorView::LoadFromFile(const std::wstring& path, std::string* error)
{
  if (!hwnd_)
  {
    return false;
  }

  std::ifstream file(std::filesystem::path(path), std::ios::binary);
  if (!file)
  {
    if (error)
    {
      *error = "Failed to open file for reading.";
    }
    return false;
  }

  std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (!file && !file.eof())
  {
    if (error)
    {
      *error = "Failed while reading file.";
    }
    return false;
  }

  if (contents.size() >= 3 &&
      static_cast<unsigned char>(contents[0]) == 0xEF &&
      static_cast<unsigned char>(contents[1]) == 0xBB &&
      static_cast<unsigned char>(contents[2]) == 0xBF)
  {
    contents.erase(0, 3);
  }

  SetTextUtf8(contents);
  SetSavePoint();
  return true;
}

bool EditorView::SaveToFile(const std::wstring& path, std::string* error)
{
  if (!hwnd_)
  {
    return false;
  }

  std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
  if (!file)
  {
    if (error)
    {
      *error = "Failed to open file for writing.";
    }
    return false;
  }

  const std::string contents = GetTextUtf8();
  if (!contents.empty())
  {
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  }

  if (!file)
  {
    if (error)
    {
      *error = "Failed while writing file.";
    }
    return false;
  }

  SetSavePoint();
  return true;
}

void EditorView::Clear()
{
  if (!hwnd_)
  {
    return;
  }

  SetTextUtf8("");
  SetSavePoint();
}

bool EditorView::IsModified() const
{
  if (!hwnd_)
  {
    return false;
  }

  return SendMessage(hwnd_, SCI_GETMODIFY, 0, 0) != 0;
}

void EditorView::SetSavePoint()
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_SETSAVEPOINT, 0, 0);
}

void* EditorView::CreateDocument()
{
  if (!hwnd_)
  {
    return nullptr;
  }

  return reinterpret_cast<void*>(SendMessage(hwnd_, SCI_CREATEDOCUMENT, 0, 0));
}

void* EditorView::GetDocumentPointer() const
{
  if (!hwnd_)
  {
    return nullptr;
  }

  return reinterpret_cast<void*>(SendMessage(hwnd_, SCI_GETDOCPOINTER, 0, 0));
}

void EditorView::SetDocument(void* document)
{
  if (!hwnd_ || !document)
  {
    return;
  }

  SendMessage(hwnd_, SCI_SETDOCPOINTER, 0, reinterpret_cast<LPARAM>(document));
}

void EditorView::SetDocumentIfNeeded(void* document)
{
  if (!hwnd_ || !document)
  {
    return;
  }

  void* const current = GetDocumentPointer();
  if (current != document)
  {
    SetDocument(document);
  }
}

void EditorView::ReleaseDocument(void* document)
{
  if (!hwnd_ || !document)
  {
    return;
  }

  if (GetDocumentPointer() == document)
  {
    return;
  }

  SendMessage(hwnd_, SCI_RELEASEDOCUMENT, 0, reinterpret_cast<LPARAM>(document));
}

int EditorView::GetCurrentLine() const
{
  if (!hwnd_)
  {
    return 0;
  }

  const LRESULT pos = SendMessage(hwnd_, SCI_GETCURRENTPOS, 0, 0);
  const LRESULT line = SendMessage(hwnd_, SCI_LINEFROMPOSITION, pos, 0);
  return static_cast<int>(line) + 1;
}

int EditorView::GetCurrentColumn() const
{
  if (!hwnd_)
  {
    return 0;
  }

  const LRESULT pos = SendMessage(hwnd_, SCI_GETCURRENTPOS, 0, 0);
  const LRESULT col = SendMessage(hwnd_, SCI_GETCOLUMN, pos, 0);
  return static_cast<int>(col) + 1;
}

void EditorView::UpdateLineNumberMarginWidth()
{
  if (!hwnd_)
  {
    return;
  }

  const int lineCount = static_cast<int>(SendMessage(hwnd_, SCI_GETLINECOUNT, 0, 0));
  int digits = 1;
  for (int value = lineCount; value >= 10; value /= 10)
  {
    ++digits;
  }

  const std::string sample(static_cast<size_t>(digits), '9');
  const int charWidth = static_cast<int>(SendMessage(
      hwnd_, SCI_TEXTWIDTH, STYLE_DEFAULT, reinterpret_cast<LPARAM>(sample.c_str())));
  SendMessage(hwnd_, SCI_SETMARGINWIDTHN, 0, static_cast<LPARAM>((std::max)(charWidth + 6, 24)));
}

void EditorView::InitializeScintilla()
{
  SendMessage(hwnd_, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
  SendMessage(hwnd_, SCI_SETEOLMODE, SC_EOL_LF, 0);
  SendMessage(hwnd_, SCI_SETSTYLEBITS, 8, 0);
  const int lineNumberLeftInset =
      (std::max)(kLineNumberLeftInsetPx,
                 MulDiv(kLineNumberLeftInsetPx, static_cast<int>(GetDpiForWindow(hwnd_)), 96));
  SendMessage(hwnd_, SCI_SETMARGINLEFT, 0, lineNumberLeftInset);
  SendMessage(hwnd_, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
  SendMessage(hwnd_, SCI_SETMARGINSENSITIVEN, 0, 0);
  UpdateLineNumberMarginWidth();

  SendMessage(hwnd_, SCI_SETCARETLINEVISIBLE, 1, 0);
  SendMessage(hwnd_, SCI_SETWRAPMODE, SC_WRAP_NONE, 0);
  SendMessage(hwnd_, SCI_SETMODEVENTMASK, SC_MODEVENTMASKALL, 0);
  SendMessage(hwnd_, SCI_SETINDICATORVALUE, 0, 1);
  SendMessage(hwnd_, SCI_INDICSETSTYLE, 0, INDIC_FULLBOX);
  SendMessage(hwnd_, SCI_INDICSETALPHA, 0, 80);
  ApplySettings(EditorSettings::Defaults());
  ApplyTheme(UiTheme::Resolve(hwnd_));
}

void EditorView::ApplySettings(const EditorSettings& settings)
{
  settings_ = settings;
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_SETTABWIDTH, settings_.tabSize, 0);
  SendMessage(hwnd_, SCI_SETUSETABS, settings_.tabToSpaces ? 0 : 1, 0);
  SetWordWrap(settings_.wordWrap);
  SetBraceMatching(settings_.matchBrace);
  SendMessage(hwnd_, SCI_SETZOOM, settings_.zoom, 0);
  highlighter_.ApplyTheme(UiTheme::Resolve(hwnd_), settings_.fontSize);
  UpdateLineNumberMarginWidth();
}

void EditorView::ApplyTheme(const AppUiTheme& theme)
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_STYLESETFORE, STYLE_LINENUMBER, theme.editor.lineNumberFore);
  SendMessage(hwnd_, SCI_STYLESETBACK, STYLE_LINENUMBER, theme.editor.lineNumberBack);
  SendMessage(hwnd_, SCI_SETCARETLINEBACK, theme.editor.caretLineBack, 0);
  SendMessage(hwnd_, SCI_INDICSETFORE, 0, theme.editor.findIndicatorFore);
  highlighter_.ApplyTheme(theme, settings_.fontSize);
  UpdateLineNumberMarginWidth();
}

int EditorView::GetSelectionLength() const
{
  if (!hwnd_)
  {
    return 0;
  }

  const LRESULT start = SendMessage(hwnd_, SCI_GETSELECTIONSTART, 0, 0);
  const LRESULT end = SendMessage(hwnd_, SCI_GETSELECTIONEND, 0, 0);
  return static_cast<int>(end > start ? end - start : start - end);
}

std::wstring EditorView::GetSelectedText() const
{
  if (!hwnd_)
  {
    return {};
  }

  const int length = GetSelectionLength();
  if (length <= 0)
  {
    return {};
  }

  std::string utf8(static_cast<size_t>(length) + 1, '\0');
  SendMessage(hwnd_, SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(utf8.data()));
  utf8.resize(length);
  return Utf8ToWide(utf8);
}

void EditorView::SetTextUtf8(const std::string& text)
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
  UpdateLineNumberMarginWidth();
}

std::string EditorView::GetTextUtf8() const
{
  if (!hwnd_)
  {
    return {};
  }

  const LRESULT length = SendMessage(hwnd_, SCI_GETTEXTLENGTH, 0, 0);
  if (length <= 0)
  {
    return {};
  }

  std::string buffer(static_cast<size_t>(length) + 1, '\0');
  SendMessage(hwnd_, SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(buffer.data()));
  buffer.resize(static_cast<size_t>(length));
  return buffer;
}

bool EditorView::FindNext(const std::wstring& text, bool matchCase, bool wholeWord, bool forward)
{
  SearchOptions options;
  options.matchCase = matchCase;
  options.wholeWord = wholeWord;
  options.forward = forward;
  return FindNext(text, options);
}

bool EditorView::FindNext(const std::wstring& text, const SearchOptions& options)
{
  if (!hwnd_ || text.empty())
  {
    return false;
  }

  if (options.regex)
  {
    return FindNextInBuffer(text, options);
  }

  const std::string needle = WideToUtf8(text);
  if (needle.empty())
  {
    return false;
  }

  const LRESULT docLength = SendMessage(hwnd_, SCI_GETLENGTH, 0, 0);
  LRESULT start = SendMessage(hwnd_, SCI_GETSELECTIONEND, 0, 0);
  LRESULT end = docLength;

  if (!options.forward)
  {
    start = SendMessage(hwnd_, SCI_GETSELECTIONSTART, 0, 0);
    end = 0;
  }

  int flags = 0;
  if (options.matchCase)
  {
    flags |= SCFIND_MATCHCASE;
  }
  if (options.wholeWord)
  {
    flags |= SCFIND_WHOLEWORD;
  }

  SendMessage(hwnd_, SCI_SETSEARCHFLAGS, flags, 0);

  if (options.forward)
  {
    SendMessage(hwnd_, SCI_SETTARGETSTART, start, 0);
    SendMessage(hwnd_, SCI_SETTARGETEND, end, 0);
    const LRESULT found = SendMessage(hwnd_, SCI_SEARCHINTARGET,
                                      static_cast<WPARAM>(needle.size()),
                                      reinterpret_cast<LPARAM>(needle.c_str()));
    if (found < 0)
    {
      SendMessage(hwnd_, SCI_SETTARGETSTART, 0, 0);
      SendMessage(hwnd_, SCI_SETTARGETEND, docLength, 0);
      const LRESULT wrapped = SendMessage(hwnd_, SCI_SEARCHINTARGET,
                                          static_cast<WPARAM>(needle.size()),
                                          reinterpret_cast<LPARAM>(needle.c_str()));
      if (wrapped < 0)
      {
        return false;
      }

      const LRESULT matchEnd = SendMessage(hwnd_, SCI_GETTARGETEND, 0, 0);
      SendMessage(hwnd_, SCI_SETSEL, wrapped, matchEnd);
      SendMessage(hwnd_, SCI_SCROLLCARET, 0, 0);
      return true;
    }

    const LRESULT matchEnd = SendMessage(hwnd_, SCI_GETTARGETEND, 0, 0);
    SendMessage(hwnd_, SCI_SETSEL, found, matchEnd);
    SendMessage(hwnd_, SCI_SCROLLCARET, 0, 0);
    return true;
  }

  SendMessage(hwnd_, SCI_SETTARGETSTART, end, 0);
  SendMessage(hwnd_, SCI_SETTARGETEND, start, 0);
  const LRESULT found = SendMessage(hwnd_, SCI_SEARCHINTARGET,
                                    static_cast<WPARAM>(needle.size()),
                                    reinterpret_cast<LPARAM>(needle.c_str()));
  if (found < 0)
  {
    SendMessage(hwnd_, SCI_SETTARGETSTART, docLength, 0);
    SendMessage(hwnd_, SCI_SETTARGETEND, 0, 0);
    const LRESULT wrapped = SendMessage(hwnd_, SCI_SEARCHINTARGET,
                                        static_cast<WPARAM>(needle.size()),
                                        reinterpret_cast<LPARAM>(needle.c_str()));
    if (wrapped < 0)
    {
      return false;
    }

    const LRESULT matchStart = SendMessage(hwnd_, SCI_GETTARGETSTART, 0, 0);
    SendMessage(hwnd_, SCI_SETSEL, matchStart, wrapped);
    SendMessage(hwnd_, SCI_SCROLLCARET, 0, 0);
    return true;
  }

  const LRESULT matchStart = SendMessage(hwnd_, SCI_GETTARGETSTART, 0, 0);
  SendMessage(hwnd_, SCI_SETSEL, matchStart, found);
  SendMessage(hwnd_, SCI_SCROLLCARET, 0, 0);
  return true;
}

bool EditorView::FindNextInBuffer(const std::wstring& text, const SearchOptions& options)
{
  const std::string buffer = GetTextUtf8();
  const size_t caretStart = static_cast<size_t>(SendMessage(hwnd_, SCI_GETSELECTIONSTART, 0, 0));
  const size_t caretEnd = static_cast<size_t>(SendMessage(hwnd_, SCI_GETSELECTIONEND, 0, 0));
  SearchMatch match{};
  std::string error;
  if (!EditorSearch::FindNextInText(buffer, caretStart, caretEnd, text, options, &match, &error))
  {
    return false;
  }

  SendMessage(hwnd_, SCI_SETSEL, static_cast<WPARAM>(match.start), static_cast<LPARAM>(match.end));
  SendMessage(hwnd_, SCI_SCROLLCARET, 0, 0);
  return true;
}

bool EditorView::ReplaceSelection(const std::wstring& findText, const std::wstring& replaceText,
                                  bool matchCase, bool wholeWord, bool regex)
{
  if (!hwnd_)
  {
    return false;
  }

  const LRESULT selStart = SendMessage(hwnd_, SCI_GETSELECTIONSTART, 0, 0);
  const LRESULT selEnd = SendMessage(hwnd_, SCI_GETSELECTIONEND, 0, 0);
  if (selStart == selEnd)
  {
    SearchOptions options;
    options.matchCase = matchCase;
    options.wholeWord = wholeWord;
    options.regex = regex;
    options.forward = true;
    if (!FindNext(findText, options))
    {
      return false;
    }
  }

  const std::string replacement = WideToUtf8(replaceText);
  SendMessage(hwnd_, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(replacement.c_str()));
  return true;
}

int EditorView::ReplaceAll(const std::wstring& findText, const std::wstring& replaceText,
                           bool matchCase, bool wholeWord, bool regex)
{
  if (!hwnd_ || findText.empty())
  {
    return 0;
  }

  if (regex)
  {
    std::string buffer = GetTextUtf8();
    SearchOptions options;
    options.matchCase = matchCase;
    options.wholeWord = wholeWord;
    options.regex = true;
    std::string error;
    SendMessage(hwnd_, SCI_BEGINUNDOACTION, 0, 0);
    const int count =
        EditorSearch::ReplaceAllInText(buffer, findText, replaceText, options, &error);
    if (count > 0)
    {
      SetTextUtf8(buffer);
    }
    SendMessage(hwnd_, SCI_ENDUNDOACTION, 0, 0);
    return count;
  }

  const std::string needle = WideToUtf8(findText);
  const std::string replacement = WideToUtf8(replaceText);
  if (needle.empty())
  {
    return 0;
  }

  int flags = 0;
  if (matchCase)
  {
    flags |= SCFIND_MATCHCASE;
  }
  if (wholeWord)
  {
    flags |= SCFIND_WHOLEWORD;
  }

  SendMessage(hwnd_, SCI_SETSEARCHFLAGS, flags, 0);
  SendMessage(hwnd_, SCI_BEGINUNDOACTION, 0, 0);

  int count = 0;
  LRESULT start = 0;
  const LRESULT docLength = SendMessage(hwnd_, SCI_GETLENGTH, 0, 0);

  while (start <= docLength)
  {
    SendMessage(hwnd_, SCI_SETTARGETSTART, start, 0);
    SendMessage(hwnd_, SCI_SETTARGETEND, docLength, 0);
    const LRESULT found = SendMessage(hwnd_, SCI_SEARCHINTARGET,
                                      static_cast<WPARAM>(needle.size()),
                                      reinterpret_cast<LPARAM>(needle.c_str()));
    if (found < 0)
    {
      break;
    }

    const LRESULT matchEnd = SendMessage(hwnd_, SCI_GETTARGETEND, 0, 0);
    SendMessage(hwnd_, SCI_SETSEL, found, matchEnd);
    SendMessage(hwnd_, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(replacement.c_str()));
    ++count;
    start = found + static_cast<LRESULT>(replacement.size());
  }

  SendMessage(hwnd_, SCI_ENDUNDOACTION, 0, 0);
  return count;
}

void EditorView::GoToLine(int lineNumber)
{
  if (!hwnd_ || lineNumber < 1)
  {
    return;
  }

  const int lineIndex = lineNumber - 1;
  const LRESULT lineCount = SendMessage(hwnd_, SCI_GETLINECOUNT, 0, 0);
  const int targetLine = lineIndex >= lineCount ? static_cast<int>(lineCount - 1) : lineIndex;
  const LRESULT pos = SendMessage(hwnd_, SCI_POSITIONFROMLINE, targetLine, 0);
  SendMessage(hwnd_, SCI_GOTOPOS, pos, 0);
  SendMessage(hwnd_, SCI_SETSEL, pos, pos);
  SendMessage(hwnd_, SCI_SCROLLCARET, 0, 0);
}

void EditorView::SetWordWrap(bool enabled)
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_SETWRAPMODE, enabled ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
}

bool EditorView::IsWordWrapEnabled() const
{
  if (!hwnd_)
  {
    return false;
  }

  return SendMessage(hwnd_, SCI_GETWRAPMODE, 0, 0) != SC_WRAP_NONE;
}

std::string EditorView::GetFirstLineUtf8() const
{
  if (!hwnd_)
  {
    return {};
  }

  const LRESULT length = SendMessage(hwnd_, SCI_LINELENGTH, 0, 0);
  if (length <= 0)
  {
    return {};
  }

  std::string line(static_cast<size_t>(length), '\0');
  SendMessage(hwnd_, SCI_GETLINE, 0, reinterpret_cast<LPARAM>(line.data()));

  const size_t endPos = line.find_first_of("\r\n");
  if (endPos != std::string::npos)
  {
    line.resize(endPos);
  }

  return line;
}

void EditorView::OnEditorNotify(const SCNotification& notification)
{
  if (!hwnd_ || !settings_.autoIndent)
  {
    return;
  }

  if (notification.nmhdr.code == SCN_CHARADDED && notification.ch == '\n')
  {
    ApplyAutoIndentForNewLine();
  }
}

void EditorView::ApplyAutoIndentForNewLine()
{
  const LRESULT currentLine = SendMessage(hwnd_, SCI_LINEFROMPOSITION,
                                          SendMessage(hwnd_, SCI_GETCURRENTPOS, 0, 0), 0);
  if (currentLine <= 0)
  {
    return;
  }

  const LRESULT prevLength = SendMessage(hwnd_, SCI_LINELENGTH, currentLine - 1, 0);
  std::string previousLine(static_cast<size_t>(prevLength), '\0');
  SendMessage(hwnd_, SCI_GETLINE, currentLine - 1, reinterpret_cast<LPARAM>(previousLine.data()));
  const std::string indent = IndentLogic::ComputeNewLineIndent(
      previousLine, settings_.tabSize, settings_.tabToSpaces);
  if (!indent.empty())
  {
    SendMessage(hwnd_, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(indent.c_str()));
  }
}

void EditorView::InsertTab()
{
  if (!hwnd_)
  {
    return;
  }

  if (settings_.tabToSpaces)
  {
    const std::string spaces(settings_.tabSize, ' ');
    SendMessage(hwnd_, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(spaces.c_str()));
    return;
  }

  SendMessage(hwnd_, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("\t"));
}

void EditorView::ConvertSelectionTabsToSpaces()
{
  if (!hwnd_)
  {
    return;
  }

  const LRESULT start = SendMessage(hwnd_, SCI_GETSELECTIONSTART, 0, 0);
  const LRESULT end = SendMessage(hwnd_, SCI_GETSELECTIONEND, 0, 0);
  if (start == end)
  {
    return;
  }

  std::string text = GetTextUtf8();
  std::string segment = text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
  std::string replaced;
  replaced.reserve(segment.size() * settings_.tabSize);
  for (char ch : segment)
  {
    if (ch == '\t')
    {
      replaced.append(static_cast<size_t>(settings_.tabSize), ' ');
    }
    else
    {
      replaced.push_back(ch);
    }
  }

  SendMessage(hwnd_, SCI_SETSEL, start, end);
  SendMessage(hwnd_, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(replaced.c_str()));
}

void EditorView::ConvertSelectionSpacesToTabs()
{
  InsertTab();
}

void EditorView::SetBraceMatching(bool enabled)
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_BRACEHIGHLIGHT, enabled ? 1 : 0, 0);
}

bool EditorView::GotoMatchingBrace()
{
  if (!hwnd_)
  {
    return false;
  }

  const LRESULT pos = SendMessage(hwnd_, SCI_GETCURRENTPOS, 0, 0);
  const LRESULT match = SendMessage(hwnd_, SCI_BRACEMATCH, pos, 0);
  if (match < 0)
  {
    return false;
  }

  SendMessage(hwnd_, SCI_GOTOPOS, match, 0);
  SendMessage(hwnd_, SCI_SETSEL, match, match);
  return true;
}

void EditorView::DuplicateLine()
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_LINECOPY, 0, 0);
  const LRESULT line = SendMessage(hwnd_, SCI_LINEFROMPOSITION,
                                   SendMessage(hwnd_, SCI_GETCURRENTPOS, 0, 0), 0);
  const LRESULT lineEnd = SendMessage(hwnd_, SCI_GETLINEENDPOSITION, line, 0);
  SendMessage(hwnd_, SCI_SETSEL, lineEnd, lineEnd);
  SendMessage(hwnd_, SCI_PASTE, 0, 0);
}

void EditorView::DeleteLine()
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_LINEDELETE, 0, 0);
}

void EditorView::MoveLineUp()
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_MOVESELECTEDLINESUP, 0, 0);
}

void EditorView::MoveLineDown()
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_MOVESELECTEDLINESDOWN, 0, 0);
}

int EditorView::TrimTrailingWhitespace()
{
  if (!hwnd_)
  {
    return 0;
  }

  int trimmedLines = 0;
  const LRESULT lineCount = SendMessage(hwnd_, SCI_GETLINECOUNT, 0, 0);
  SendMessage(hwnd_, SCI_BEGINUNDOACTION, 0, 0);
  for (LRESULT line = 0; line < lineCount; ++line)
  {
    const LRESULT length = SendMessage(hwnd_, SCI_LINELENGTH, line, 0);
    if (length <= 0)
    {
      continue;
    }

    std::string text(static_cast<size_t>(length), '\0');
    SendMessage(hwnd_, SCI_GETLINE, line, reinterpret_cast<LPARAM>(text.data()));
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    {
      text.pop_back();
    }

    const LRESULT lineStart = SendMessage(hwnd_, SCI_POSITIONFROMLINE, line, 0);
    const LRESULT lineEnd = SendMessage(hwnd_, SCI_GETLINEENDPOSITION, line, 0);
    if (static_cast<size_t>(lineEnd - lineStart) != text.size())
    {
      SendMessage(hwnd_, SCI_SETTARGETSTART, lineStart, 0);
      SendMessage(hwnd_, SCI_SETTARGETEND, lineEnd, 0);
      SendMessage(hwnd_, SCI_REPLACETARGET, static_cast<WPARAM>(text.size()),
                  reinterpret_cast<LPARAM>(text.c_str()));
      ++trimmedLines;
    }
  }
  SendMessage(hwnd_, SCI_ENDUNDOACTION, 0, 0);
  return trimmedLines;
}

void EditorView::ZoomDelta(int steps)
{
  if (!hwnd_)
  {
    return;
  }

  if (steps > 0)
  {
    for (int i = 0; i < steps; ++i)
    {
      SendMessage(hwnd_, SCI_ZOOMIN, 0, 0);
    }
  }
  else if (steps < 0)
  {
    for (int i = 0; i < -steps; ++i)
    {
      SendMessage(hwnd_, SCI_ZOOMOUT, 0, 0);
    }
  }

  settings_.zoom = static_cast<int>(SendMessage(hwnd_, SCI_GETZOOM, 0, 0));
}

int EditorView::GetZoom() const
{
  if (!hwnd_)
  {
    return 0;
  }

  return static_cast<int>(SendMessage(hwnd_, SCI_GETZOOM, 0, 0));
}

void EditorView::ClearFindHighlights()
{
  if (!hwnd_)
  {
    return;
  }

  const LRESULT length = SendMessage(hwnd_, SCI_GETLENGTH, 0, 0);
  SendMessage(hwnd_, SCI_SETINDICATORCURRENT, 0, 0);
  SendMessage(hwnd_, SCI_INDICATORCLEARRANGE, 0, length);
}

void EditorView::UpdateFindHighlights(const std::wstring& query, const SearchOptions& options)
{
  ClearFindHighlights();
  if (!hwnd_ || query.empty())
  {
    return;
  }

  const LRESULT docLength = SendMessage(hwnd_, SCI_GETLENGTH, 0, 0);
  if (docLength <= 0)
  {
    return;
  }

  constexpr size_t kMaxHighlightBytes = 1024 * 1024;
  constexpr size_t kMaxRegexHighlightBytes = 256 * 1024;
  const size_t length = static_cast<size_t>(docLength);
  const size_t maxBytes = options.regex ? kMaxRegexHighlightBytes : kMaxHighlightBytes;
  if (length > maxBytes)
  {
    return;
  }

  std::string error;
  const std::vector<SearchMatch> matches =
      EditorSearch::FindAllInText(GetTextUtf8(), query, options, &error);
  SendMessage(hwnd_, SCI_SETINDICATORCURRENT, 0, 0);
  for (const SearchMatch& match : matches)
  {
    SendMessage(hwnd_, SCI_INDICATORFILLRANGE, static_cast<WPARAM>(match.start),
                static_cast<LPARAM>(match.end - match.start));
  }
}

void EditorView::ConfigureFolding()
{
  SendMessage(hwnd_, SCI_SETPROPERTY, reinterpret_cast<WPARAM>("fold"), reinterpret_cast<LPARAM>("1"));
  SendMessage(hwnd_, SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);
  SendMessage(hwnd_, SCI_SETMARGINWIDTHN, 1, 16);
  SendMessage(hwnd_, SCI_SETMARGINMASKN, 1, SC_MASK_FOLDERS);
  SendMessage(hwnd_, SCI_SETFOLDFLAGS, 0, SC_FOLDFLAG_LINEAFTER_CONTRACTED);
}

void EditorView::EnableCodeFolding(bool enabled)
{
  if (!hwnd_)
  {
    return;
  }

  if (enabled)
  {
    ConfigureFolding();
    const LRESULT lineCount = SendMessage(hwnd_, SCI_GETLINECOUNT, 0, 0);
    for (LRESULT line = 0; line < lineCount; ++line)
    {
      SendMessage(hwnd_, SCI_SETFOLDLEVEL, line, SC_FOLDLEVELBASE);
    }
  }
  else
  {
    SendMessage(hwnd_, SCI_SETMARGINWIDTHN, 1, 0);
  }
}
