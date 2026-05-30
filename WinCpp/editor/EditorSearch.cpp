#include "EditorSearch.h"

#include "ConfigJson.h"

#include <cctype>

namespace
{
std::string WideToUtf8(const std::wstring& text)
{
  return ConfigJson::WideToUtf8(text);
}

bool IsWordChar(unsigned char ch)
{
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
         ch == '_';
}

bool IsWholeWordMatch(const std::string& text, size_t start, size_t end)
{
  if (start > 0 && IsWordChar(static_cast<unsigned char>(text[start - 1])))
  {
    return false;
  }
  if (end < text.size() && IsWordChar(static_cast<unsigned char>(text[end])))
  {
    return false;
  }
  return true;
}

size_t FindForward(const std::string& text, const std::string& needle, size_t start, bool matchCase)
{
  while (start <= text.size())
  {
    size_t found = std::string::npos;
    if (matchCase)
    {
      found = text.find(needle, start);
    }
    else
    {
      for (size_t i = start; i + needle.size() <= text.size(); ++i)
      {
        bool equal = true;
        for (size_t j = 0; j < needle.size(); ++j)
        {
          const unsigned char a = static_cast<unsigned char>(text[i + j]);
          const unsigned char b = static_cast<unsigned char>(needle[j]);
          if (tolower(a) != tolower(b))
          {
            equal = false;
            break;
          }
        }
        if (equal)
        {
          found = i;
          break;
        }
      }
    }

    if (found == std::string::npos)
    {
      return std::string::npos;
    }
    return found;
  }
  return std::string::npos;
}

size_t FindBackward(const std::string& text, const std::string& needle, size_t before,
                    bool matchCase)
{
  if (before == 0)
  {
    return std::string::npos;
  }

  for (size_t i = std::min(before, text.size()); i-- > 0;)
  {
    if (i + needle.size() > text.size())
    {
      continue;
    }

    bool equal = true;
    for (size_t j = 0; j < needle.size(); ++j)
    {
      const unsigned char a = static_cast<unsigned char>(text[i + j]);
      const unsigned char b = static_cast<unsigned char>(needle[j]);
      const unsigned char ca = matchCase ? a : static_cast<unsigned char>(tolower(a));
      const unsigned char cb = matchCase ? b : static_cast<unsigned char>(tolower(b));
      if (ca != cb)
      {
        equal = false;
        break;
      }
    }

    if (equal)
    {
      return i;
    }
  }

  return std::string::npos;
}

bool FindPlain(const std::string& text, size_t caretStart, size_t caretEnd, const std::string& needle,
               bool matchCase, bool wholeWord, bool forward, SearchMatch* match)
{
  if (needle.empty())
  {
    return false;
  }

  if (forward)
  {
    size_t start = caretEnd;
    while (true)
    {
      const size_t found = FindForward(text, needle, start, matchCase);
      if (found == std::string::npos)
      {
        break;
      }
      if (!wholeWord || IsWholeWordMatch(text, found, found + needle.size()))
      {
        match->start = found;
        match->end = found + needle.size();
        return true;
      }
      start = found + 1;
    }

    size_t wrapStart = 0;
    while (wrapStart < caretStart)
    {
      const size_t found = FindForward(text, needle, wrapStart, matchCase);
      if (found == std::string::npos || found >= caretStart)
      {
        break;
      }
      if (!wholeWord || IsWholeWordMatch(text, found, found + needle.size()))
      {
        match->start = found;
        match->end = found + needle.size();
        return true;
      }
      wrapStart = found + 1;
    }
    return false;
  }

  size_t start = caretStart == 0 ? 0 : caretStart - 1;
  while (true)
  {
    const size_t found = FindBackward(text, needle, start + 1, matchCase);
    if (found == std::string::npos)
    {
      break;
    }
    if (!wholeWord || IsWholeWordMatch(text, found, found + needle.size()))
    {
      match->start = found;
      match->end = found + needle.size();
      return true;
    }
    if (found == 0)
    {
      break;
    }
    start = found - 1;
  }

  size_t pos = text.size();
  while (pos > caretStart)
  {
    const size_t found = FindBackward(text, needle, pos, matchCase);
    if (found == std::string::npos || found < caretEnd)
    {
      break;
    }
    if (!wholeWord || IsWholeWordMatch(text, found, found + needle.size()))
    {
      match->start = found;
      match->end = found + needle.size();
      return true;
    }
    if (found == 0)
    {
      break;
    }
    pos = found;
  }
  return false;
}

bool FindPlainForwardNoWrap(const std::string& text, size_t start, const std::string& needle,
                            bool matchCase, bool wholeWord, SearchMatch* match)
{
  if (needle.empty())
  {
    return false;
  }

  while (start <= text.size())
  {
    const size_t found = FindForward(text, needle, start, matchCase);
    if (found == std::string::npos)
    {
      return false;
    }
    if (found > text.size() || needle.size() > text.size() - found)
    {
      return false;
    }
    if (!wholeWord || IsWholeWordMatch(text, found, found + needle.size()))
    {
      match->start = found;
      match->end = found + needle.size();
      return true;
    }
    start = found + 1;
  }

  return false;
}
}

bool EditorSearch::FindNextInText(const std::string& text, size_t caretStart, size_t caretEnd,
                                  const std::wstring& query, const SearchOptions& options,
                                  SearchMatch* match, std::string* error)
{
  if (!match || query.empty())
  {
    return false;
  }

  const std::string needle = WideToUtf8(query);
  if (needle.empty())
  {
    return false;
  }

  if (!options.regex)
  {
    return FindPlain(text, caretStart, caretEnd, needle, options.matchCase, options.wholeWord,
                     options.forward, match);
  }

  RegexPattern pattern;
  if (!pattern.Compile(needle, error))
  {
    return false;
  }

  const size_t searchStart = options.forward ? caretEnd : 0;
  const size_t searchEnd = options.forward ? text.size() : caretStart;
  const std::string slice = text.substr(searchStart, searchEnd - searchStart);

  SearchMatch candidate{};
  bool found = false;
  pattern.ForEachMatch(slice, [&](size_t s, size_t e) {
    if (found)
    {
      return;
    }
    candidate.start = searchStart + s;
    candidate.end = searchStart + e;
    if (!options.wholeWord || IsWholeWordMatch(text, candidate.start, candidate.end))
    {
      found = true;
    }
  });

  if (found && options.forward)
  {
    *match = candidate;
    return true;
  }

  if (!options.forward)
  {
    SearchMatch last{};
    bool any = false;
    pattern.ForEachMatch(text.substr(0, searchEnd), [&](size_t s, size_t e) {
      last.start = s;
      last.end = e;
      if (!options.wholeWord || IsWholeWordMatch(text, s, e))
      {
        any = true;
      }
    });
    if (any)
    {
      *match = last;
      return true;
    }
  }

  SearchOptions wrap = options;
  wrap.forward = !options.forward;
  return FindPlain(text, caretStart, caretEnd, needle, options.matchCase, options.wholeWord,
                   wrap.forward, match);
}

int EditorSearch::ReplaceAllInText(std::string& text, const std::wstring& findText,
                                   const std::wstring& replaceText, const SearchOptions& options,
                                   std::string* error)
{
  const std::string replacement = WideToUtf8(replaceText);
  int count = 0;

  if (!options.regex)
  {
    const std::string needle = WideToUtf8(findText);
    size_t start = 0;
    while (start <= text.size())
    {
      SearchMatch match{};
      if (!FindPlainForwardNoWrap(text, start, needle, options.matchCase, options.wholeWord,
                                  &match))
      {
        break;
      }
      text.replace(match.start, match.end - match.start, replacement);
      start = match.start + replacement.size();
      ++count;
    }
    return count;
  }

  RegexPattern pattern;
  const std::string needle = WideToUtf8(findText);
  if (!pattern.Compile(needle, error))
  {
    return 0;
  }

  std::string rebuilt;
  size_t cursor = 0;
  pattern.ForEachMatch(text, [&](size_t s, size_t e) {
    if (options.wholeWord && !IsWholeWordMatch(text, s, e))
    {
      return;
    }
    rebuilt.append(text, cursor, s - cursor);
    rebuilt.append(replacement);
    cursor = e;
    ++count;
  });
  rebuilt.append(text, cursor, text.size() - cursor);
  if (count > 0)
  {
    text = std::move(rebuilt);
  }
  return count;
}

std::vector<SearchMatch> EditorSearch::FindAllInText(const std::string& text,
                                                     const std::wstring& query,
                                                     const SearchOptions& options,
                                                     std::string* error)
{
  std::vector<SearchMatch> matches;
  if (query.empty())
  {
    return matches;
  }

  if (!options.regex)
  {
    const std::string needle = WideToUtf8(query);
    size_t start = 0;
    while (start <= text.size())
    {
      SearchMatch match{};
      if (!FindPlainForwardNoWrap(text, start, needle, options.matchCase, options.wholeWord,
                                  &match))
      {
        break;
      }
      matches.push_back(match);
      start = match.end;
    }
    return matches;
  }

  RegexPattern pattern;
  if (!pattern.Compile(WideToUtf8(query), error))
  {
    return matches;
  }

  pattern.ForEachMatch(text, [&](size_t s, size_t e) {
    if (!options.wholeWord || IsWholeWordMatch(text, s, e))
    {
      matches.push_back({s, e});
    }
  });
  return matches;
}
