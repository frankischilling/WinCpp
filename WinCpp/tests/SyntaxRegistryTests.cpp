#include <gtest/gtest.h>

#include "SyntaxRegistry.h"

#include <filesystem>
#include <string>

namespace
{
std::filesystem::path FixturesDir()
{
  return std::filesystem::path(WINCPP_TEST_FIXTURES_DIR);
}
}

TEST(SyntaxRegistry, LoadSharedDirectoryFromFixtures)
{
  SyntaxRegistry registry;
  std::string error;
  const auto syntaxDir = FixturesDir() / "syntax";
  ASSERT_TRUE(registry.LoadFromDirectory(syntaxDir, &error)) << error;
  EXPECT_FALSE(registry.Definitions().empty());
}

TEST(SyntaxRegistry, DetectByFilenameExtension)
{
  SyntaxRegistry registry;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", nullptr));

  const SyntaxDefinition* def =
      registry.DetectForPath(L"C:\\project\\file.testlang", "");
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->filetype, "testlang");
}

TEST(SyntaxRegistry, DetectCppFromBundledSyntaxIfPresent)
{
  SyntaxRegistry registry;
  const auto fullSyntax = FixturesDir() / "syntax_full";
  const auto bundledSyntax = FixturesDir() / ".." / ".." / "WinCpp" / "syntax";
  const std::filesystem::path syntaxPath =
      std::filesystem::exists(fullSyntax) ? fullSyntax : bundledSyntax;
  if (!std::filesystem::exists(syntaxPath))
  {
    GTEST_SKIP() << "Bundled syntax directory not found";
  }

  std::string error;
  ASSERT_TRUE(registry.LoadFromDirectory(syntaxPath, &error)) << error;

  const SyntaxDefinition* def = registry.DetectForPath(L"C:\\src\\main.cpp", "#include <cstdio>");
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->filetype, "c++");
}

TEST(SyntaxRegistry, SharedRegistryLoadsOnce)
{
  const auto syntaxDir = FixturesDir() / "syntax";
  ASSERT_TRUE(SyntaxRegistry::LoadSharedDirectory(syntaxDir, nullptr));
  EXPECT_TRUE(SyntaxRegistry::Shared().IsLoaded());
}

TEST(SyntaxRegistry, UnknownExtensionReturnsNull)
{
  SyntaxRegistry registry;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", nullptr));
  EXPECT_EQ(registry.DetectForPath(L"file.unknownext", ""), nullptr);
}

TEST(SyntaxRegistry, DetectByHeader)
{
  SyntaxRegistry registry;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", nullptr));

  const SyntaxDefinition* def =
      registry.DetectForPath(L"file.txt", "#!/usr/bin/env testlang_header");
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->filetype, "testlang_header");
}

TEST(SyntaxRegistry, DetectBySignature)
{
  SyntaxRegistry registry;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", nullptr));

  const SyntaxDefinition* def = registry.DetectForPath(L"data.bin", "SIGNATURE-MARK");
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->filetype, "testlang_signature");
}

TEST(SyntaxRegistry, FilenameTakesPriorityOverHeader)
{
  SyntaxRegistry registry;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", nullptr));

  const SyntaxDefinition* def =
      registry.DetectForPath(L"C:\\x\\file.testlang", "#!/usr/bin/env testlang_header");
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->filetype, "testlang");
}

TEST(SyntaxRegistry, InvalidYamlFileSkippedStillLoadsValid)
{
  SyntaxRegistry registry;
  std::string error;
  ASSERT_TRUE(registry.LoadFromDirectory(FixturesDir() / "syntax", &error)) << error;
  EXPECT_FALSE(registry.Definitions().empty());
  EXPECT_NE(registry.DetectForPath(L"file.testlang", ""), nullptr);
}

TEST(SyntaxRegistry, LoadSharedDirectoryMissingPathFails)
{
  std::string error;
  const auto missing = FixturesDir() / "no_such_syntax_dir";
  EXPECT_FALSE(SyntaxRegistry::LoadSharedDirectory(missing, &error));
  EXPECT_FALSE(error.empty());
}
