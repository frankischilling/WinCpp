#pragma once

#include <string>

#include "UiTheme.h"
#include "WinCpp.h"

struct SyntaxDefinition;

class SyntaxHighlighter
{
public:
  SyntaxHighlighter();
  void Attach(HWND editor);
  void Apply(const SyntaxDefinition& definition);
  void Clear();
  void ApplyTheme(const AppUiTheme& theme, int fontSize);
  void InvalidateBaseStyles();

private:
  void EnsureBaseStyles();
  int StyleForGroup(const std::string& group) const;

  HWND editor_;
  bool baseStylesApplied_;
  AppUiTheme theme_;
  int fontSize_;
};
