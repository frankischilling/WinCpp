#pragma once

#include <string>

#include "WinCpp.h"

struct SyntaxDefinition;

class SyntaxHighlighter
{
public:
  SyntaxHighlighter();
  void Attach(HWND editor);
  void Apply(const SyntaxDefinition& definition);
  void Clear();

private:
  void EnsureBaseStyles();
  int StyleForGroup(const std::string& group) const;

  HWND editor_;
  bool baseStylesApplied_;
};
