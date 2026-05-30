#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "RegexPattern.h"

struct SyntaxRule
{
  std::string group;
  std::string pattern;
  RegexPattern regex;
};

struct SyntaxDefinition
{
  std::string filetype;
  std::string detectFilename;
  std::string detectHeader;
  std::string detectSignature;

  RegexPattern detectFilenameRegex;
  RegexPattern detectHeaderRegex;
  RegexPattern detectSignatureRegex;

  std::vector<SyntaxRule> rules;
};

class SyntaxRegistry
{
public:
  // Loaded once for all editor panes.
  static bool LoadSharedDirectory(const std::filesystem::path& directory, std::string* error);
  static const SyntaxRegistry& Shared();

  bool LoadFromDirectory(const std::filesystem::path& directory, std::string* error);
  const SyntaxDefinition* DetectForPath(const std::wstring& filePath,
                                        const std::string& firstLine) const;
  const std::vector<SyntaxDefinition>& Definitions() const { return definitions_; }
  bool IsLoaded() const { return !definitions_.empty(); }

private:
  bool LoadFile(const std::filesystem::path& path, SyntaxDefinition* outDef, std::string* error);
  static std::string WideToUtf8(const std::wstring& text);

  std::vector<SyntaxDefinition> definitions_;
};
