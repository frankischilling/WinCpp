#pragma once

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <functional>
#include <string>

class RegexPattern
{
public:
  RegexPattern();
  RegexPattern(const RegexPattern&) = delete;
  RegexPattern& operator=(const RegexPattern&) = delete;
  RegexPattern(RegexPattern&& other) noexcept;
  RegexPattern& operator=(RegexPattern&& other) noexcept;
  ~RegexPattern();

  bool Compile(const std::string& pattern, std::string* error);
  bool IsValid() const { return code_ != nullptr; }
  bool Match(const std::string& text) const;
  bool ForEachMatch(const std::string& text,
                    const std::function<void(size_t, size_t)>& onMatch) const;

  const std::string& Pattern() const { return pattern_; }

private:
  void Reset();

  std::string pattern_;
  pcre2_code* code_;
};
