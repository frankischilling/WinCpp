#include "SyntaxRegistry.h"

#include <windows.h>
#include <yaml-cpp/yaml.h>

namespace
{
bool TryGetScalar(const YAML::Node& node, std::string* value)
{
  if (!node || !node.IsScalar())
  {
    return false;
  }

  *value = node.as<std::string>();
  return true;
}

bool HasYamlExtension(const std::filesystem::path& path)
{
  const auto ext = path.extension().wstring();
  return _wcsicmp(ext.c_str(), L".yaml") == 0 || _wcsicmp(ext.c_str(), L".yml") == 0;
}

std::string WrapRegex(const std::string& expr)
{
  return "(?:" + expr + ")";
}

std::string BuildRegionPattern(const std::string& start,
                               const std::string& end,
                               const std::string& skip)
{
  if (start.empty() || end.empty())
  {
    return {};
  }

  const std::string startGroup = WrapRegex(start);
  const std::string endGroup = WrapRegex(end);
  const std::string anyChar = "(?!" + endGroup + ")[\\s\\S]";

  std::string inner;
  if (!skip.empty())
  {
    inner = "(?:" + WrapRegex(skip) + "|" + anyChar + ")*?";
  }
  else
  {
    inner = "(?:" + anyChar + ")*?";
  }

  return startGroup + inner + endGroup;
}

void AppendRule(std::vector<SyntaxRule>* rules,
                const std::string& group,
                const std::string& pattern)
{
  if (!rules || group.empty() || pattern.empty())
  {
    return;
  }

  SyntaxRule rule;
  rule.group = group;
  rule.pattern = pattern;

  std::string compileError;
  if (!rule.regex.Compile(rule.pattern, &compileError))
  {
    return;
  }

  rules->push_back(std::move(rule));
}

void AppendRulesFromNode(const YAML::Node& rulesNode,
                         std::vector<SyntaxRule>* rules)
{
  if (!rulesNode || !rulesNode.IsSequence())
  {
    return;
  }

  for (const auto& ruleNode : rulesNode)
  {
    if (!ruleNode.IsMap())
    {
      continue;
    }

    for (auto it = ruleNode.begin(); it != ruleNode.end(); ++it)
    {
      if (!it->first.IsScalar())
      {
        continue;
      }

      const std::string group = it->first.as<std::string>();
      const YAML::Node value = it->second;

      if (value.IsScalar())
      {
        AppendRule(rules, group, value.as<std::string>());
        continue;
      }

      if (!value.IsMap())
      {
        continue;
      }

      std::string start;
      std::string end;
      std::string skip;
      TryGetScalar(value["start"], &start);
      TryGetScalar(value["end"], &end);
      TryGetScalar(value["skip"], &skip);

      if (!start.empty() && !end.empty())
      {
        AppendRule(rules, group, BuildRegionPattern(start, end, skip));
      }
    }
  }
}
}

namespace
{
SyntaxRegistry g_sharedRegistry;
bool g_sharedLoaded = false;
std::filesystem::path g_sharedDirectory;
}

bool SyntaxRegistry::LoadSharedDirectory(const std::filesystem::path& directory, std::string* error)
{
  if (g_sharedLoaded && g_sharedDirectory == directory && g_sharedRegistry.IsLoaded())
  {
    return true;
  }

  g_sharedLoaded = g_sharedRegistry.LoadFromDirectory(directory, error);
  if (g_sharedLoaded)
  {
    g_sharedDirectory = directory;
  }
  return g_sharedLoaded;
}

const SyntaxRegistry& SyntaxRegistry::Shared()
{
  return g_sharedRegistry;
}

bool SyntaxRegistry::LoadFromDirectory(const std::filesystem::path& directory, std::string* error)
{
  definitions_.clear();

  if (!std::filesystem::exists(directory))
  {
    if (error)
    {
      *error = "Syntax directory does not exist.";
    }
    return false;
  }

  if (!std::filesystem::is_directory(directory))
  {
    if (error)
    {
      *error = "Syntax path is not a directory.";
    }
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(directory))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }

    if (!HasYamlExtension(entry.path()))
    {
      continue;
    }

    SyntaxDefinition definition;
    std::string loadError;
    if (LoadFile(entry.path(), &definition, &loadError))
    {
      definitions_.push_back(std::move(definition));
    }
  }

  if (definitions_.empty())
  {
    if (error)
    {
      *error = "No syntax definitions loaded.";
    }
    return false;
  }

  return true;
}

const SyntaxDefinition* SyntaxRegistry::DetectForPath(const std::wstring& filePath,
                                                      const std::string& firstLine) const
{
  const std::string pathUtf8 = WideToUtf8(filePath);

  for (const auto& definition : definitions_)
  {
    if (!definition.detectFilename.empty() &&
        definition.detectFilenameRegex.Match(pathUtf8))
    {
      return &definition;
    }
  }

  for (const auto& definition : definitions_)
  {
    if (!definition.detectHeader.empty() &&
        definition.detectHeaderRegex.Match(firstLine))
    {
      return &definition;
    }
  }

  for (const auto& definition : definitions_)
  {
    if (!definition.detectSignature.empty() &&
        definition.detectSignatureRegex.Match(firstLine))
    {
      return &definition;
    }
  }

  return nullptr;
}

bool SyntaxRegistry::LoadFile(const std::filesystem::path& path,
                              SyntaxDefinition* outDef,
                              std::string* error)
{
  if (!outDef)
  {
    return false;
  }

  YAML::Node root;
  try
  {
    root = YAML::LoadFile(WideToUtf8(path.wstring()));
  }
  catch (const YAML::Exception&)
  {
    if (error)
    {
      *error = "Failed to parse YAML syntax file.";
    }
    return false;
  }
  if (!root || !root["filetype"] || !root["filetype"].IsScalar())
  {
    if (error)
    {
      *error = "Missing filetype field.";
    }
    return false;
  }

  outDef->filetype = root["filetype"].as<std::string>();

  const YAML::Node detect = root["detect"];
  if (detect)
  {
    TryGetScalar(detect["filename"], &outDef->detectFilename);
    TryGetScalar(detect["header"], &outDef->detectHeader);
    TryGetScalar(detect["signature"], &outDef->detectSignature);
  }

  if (!outDef->detectFilename.empty())
  {
    outDef->detectFilenameRegex.Compile(outDef->detectFilename, error);
  }

  if (!outDef->detectHeader.empty())
  {
    outDef->detectHeaderRegex.Compile(outDef->detectHeader, error);
  }

  if (!outDef->detectSignature.empty())
  {
    outDef->detectSignatureRegex.Compile(outDef->detectSignature, error);
  }

  AppendRulesFromNode(root["rules"], &outDef->rules);

  return true;
}

std::string SyntaxRegistry::WideToUtf8(const std::wstring& text)
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
