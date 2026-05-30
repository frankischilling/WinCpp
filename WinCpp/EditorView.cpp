#include "EditorView.h"

#include <fstream>
#include <Scintilla.h>

#include "AppIds.h"

namespace
{
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
    if (!Scintilla_RegisterClasses(instance))
    {
      return false;
    }
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
  if (!syntax_.LoadFromDirectory(directory, &error))
  {
    OutputDebugStringA(error.c_str());
    OutputDebugStringA("\n");
  }
}

void EditorView::ApplySyntaxForPath(const std::wstring& filePath)
{
  if (!hwnd_)
  {
    return;
  }

  const std::string firstLine = GetFirstLineUtf8();
  const SyntaxDefinition* definition = syntax_.DetectForPath(filePath, firstLine);
  if (!definition)
  {
    return;
  }

  highlighter_.Apply(*definition);
}

bool EditorView::LoadFromFileIntoDocument(void* document, const std::wstring& path, std::string* error)
{
  if (!hwnd_ || !document)
  {
    return false;
  }

  void* const previous = GetDocumentPointer();
  SetDocument(document);
  const bool loaded = LoadFromFile(path, error);
  if (previous != nullptr)
  {
    SetDocumentIfNeeded(previous);
  }
  return loaded;
}

bool EditorView::LoadFromFile(const std::wstring& path, std::string* error)
{
  if (!hwnd_)
  {
    return false;
  }

  std::ifstream file(path, std::ios::binary);
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

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
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

void EditorView::InitializeScintilla()
{
  SendMessage(hwnd_, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
  SendMessage(hwnd_, SCI_SETEOLMODE, SC_EOL_LF, 0);
  SendMessage(hwnd_, SCI_SETSTYLEBITS, 8, 0);
  SendMessage(hwnd_, SCI_SETMARGINLEFT, 0, 4);

  SendMessage(hwnd_, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
  SendMessage(hwnd_, SCI_SETMARGINWIDTHN, 0, 44);
  SendMessage(hwnd_, SCI_SETMARGINSENSITIVEN, 0, 0);

  SendMessage(hwnd_, SCI_SETCARETLINEVISIBLE, 1, 0);
  SendMessage(hwnd_, SCI_SETCARETLINEBACK, RGB(242, 242, 242), 0);
  SendMessage(hwnd_, SCI_SETWRAPMODE, SC_WRAP_NONE, 0);
  SendMessage(hwnd_, SCI_SETMODEVENTMASK, SC_MODEVENTMASKALL, 0);
}

void EditorView::SetTextUtf8(const std::string& text)
{
  if (!hwnd_)
  {
    return;
  }

  SendMessage(hwnd_, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
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
  if (!hwnd_ || text.empty())
  {
    return false;
  }

  const std::string needle = WideToUtf8(text);
  if (needle.empty())
  {
    return false;
  }

  const LRESULT docLength = SendMessage(hwnd_, SCI_GETLENGTH, 0, 0);
  LRESULT start = SendMessage(hwnd_, SCI_GETSELECTIONEND, 0, 0);
  LRESULT end = docLength;

  if (!forward)
  {
    start = SendMessage(hwnd_, SCI_GETSELECTIONSTART, 0, 0);
    end = 0;
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

  if (forward)
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

bool EditorView::ReplaceSelection(const std::wstring& findText, const std::wstring& replaceText,
                                  bool matchCase, bool wholeWord)
{
  if (!hwnd_)
  {
    return false;
  }

  const LRESULT selStart = SendMessage(hwnd_, SCI_GETSELECTIONSTART, 0, 0);
  const LRESULT selEnd = SendMessage(hwnd_, SCI_GETSELECTIONEND, 0, 0);
  if (selStart == selEnd)
  {
    if (!FindNext(findText, matchCase, wholeWord, true))
    {
      return false;
    }
  }

  const std::string replacement = WideToUtf8(replaceText);
  SendMessage(hwnd_, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(replacement.c_str()));
  return true;
}

int EditorView::ReplaceAll(const std::wstring& findText, const std::wstring& replaceText,
                           bool matchCase, bool wholeWord)
{
  if (!hwnd_ || findText.empty())
  {
    return 0;
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
