#pragma once

#include "RegexPattern.h"

#include <string>
#include <vector>

struct SearchOptions
{
  bool matchCase = false;
  bool wholeWord = false;
  bool regex = false;
  bool forward = true;
};

struct SearchMatch
{
  size_t start = 0;
  size_t end = 0;
};

class EditorSearch
{
public:
  static bool FindNextInText(const std::string& text, size_t caretStart, size_t caretEnd,
                             const std::wstring& query, const SearchOptions& options,
                             SearchMatch* match, std::string* error);

  static int ReplaceAllInText(std::string& text, const std::wstring& findText,
                              const std::wstring& replaceText, const SearchOptions& options,
                              std::string* error);

  static std::vector<SearchMatch> FindAllInText(const std::string& text,
                                                const std::wstring& query,
                                                const SearchOptions& options,
                                                std::string* error);
};
