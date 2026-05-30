#include "RegexPattern.h"

namespace
{
constexpr uint32_t kRegexOptions = PCRE2_UTF | PCRE2_UCP | PCRE2_MULTILINE;
}

RegexPattern::RegexPattern() : code_(nullptr) {}

RegexPattern::RegexPattern(RegexPattern&& other) noexcept
  : pattern_(std::move(other.pattern_)), code_(other.code_)
{
  other.code_ = nullptr;
}

RegexPattern& RegexPattern::operator=(RegexPattern&& other) noexcept
{
  if (this != &other)
  {
    Reset();
    pattern_ = std::move(other.pattern_);
    code_ = other.code_;
    other.code_ = nullptr;
  }
  return *this;
}

RegexPattern::~RegexPattern()
{
  Reset();
}

bool RegexPattern::Compile(const std::string& pattern, std::string* error)
{
  Reset();
  pattern_ = pattern;
  if (pattern.empty())
  {
    return true;
  }

  int errorCode = 0;
  PCRE2_SIZE errorOffset = 0;
  code_ = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.c_str()),
                        pattern.size(),
                        kRegexOptions,
                        &errorCode,
                        &errorOffset,
                        nullptr);

  if (!code_)
  {
    if (error)
    {
      PCRE2_UCHAR message[256] = {};
      pcre2_get_error_message(errorCode, message, sizeof(message));
      *error = "PCRE2 compile error at offset " + std::to_string(errorOffset) +
               ": " + reinterpret_cast<const char*>(message);
    }
    return false;
  }

  return true;
}

bool RegexPattern::Match(const std::string& text) const
{
  if (!code_)
  {
    return false;
  }

  pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(code_, nullptr);
  if (!matchData)
  {
    return false;
  }

  const int rc = pcre2_match(code_,
                             reinterpret_cast<PCRE2_SPTR>(text.c_str()),
                             text.size(),
                             0,
                             0,
                             matchData,
                             nullptr);
  pcre2_match_data_free(matchData);

  return rc >= 0;
}

bool RegexPattern::ForEachMatch(const std::string& text,
                                const std::function<void(size_t, size_t)>& onMatch) const
{
  if (!code_)
  {
    return false;
  }

  pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(code_, nullptr);
  if (!matchData)
  {
    return false;
  }

  bool matched = false;
  size_t startOffset = 0;

  while (startOffset <= text.size())
  {
    const int rc = pcre2_match(code_,
                               reinterpret_cast<PCRE2_SPTR>(text.c_str()),
                               text.size(),
                               startOffset,
                               0,
                               matchData,
                               nullptr);
    if (rc <= 0)
    {
      break;
    }

    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
    const size_t matchStart = static_cast<size_t>(ovector[0]);
    const size_t matchEnd = static_cast<size_t>(ovector[1]);

    if (matchEnd <= matchStart)
    {
      startOffset = matchStart + 1;
      continue;
    }

    matched = true;
    onMatch(matchStart, matchEnd);
    startOffset = matchEnd;
  }

  pcre2_match_data_free(matchData);
  return matched;
}

void RegexPattern::Reset()
{
  if (code_)
  {
    pcre2_code_free(code_);
    code_ = nullptr;
  }
}
