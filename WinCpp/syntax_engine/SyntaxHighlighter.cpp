#include "SyntaxHighlighter.h"

#include <Scintilla.h>

#include <vector>

#include "SyntaxRegistry.h"

namespace
{
constexpr int kStyleComment = STYLE_LASTPREDEFINED + 1;
constexpr int kStyleString = STYLE_LASTPREDEFINED + 2;
constexpr int kStyleNumber = STYLE_LASTPREDEFINED + 3;
constexpr int kStyleKeyword = STYLE_LASTPREDEFINED + 4;
constexpr int kStyleType = STYLE_LASTPREDEFINED + 5;
constexpr int kStylePreproc = STYLE_LASTPREDEFINED + 6;
constexpr int kStyleOperator = STYLE_LASTPREDEFINED + 7;
constexpr int kStyleIdentifier = STYLE_LASTPREDEFINED + 8;

bool StartsWith(const std::string& value, const std::string& prefix)
{
  return value.rfind(prefix, 0) == 0;
}
}

SyntaxHighlighter::SyntaxHighlighter()
  : editor_(nullptr), baseStylesApplied_(false), fontSize_(11)
{
  theme_ = UiTheme::Resolve(nullptr);
}

void SyntaxHighlighter::Attach(HWND editor)
{
  editor_ = editor;
  baseStylesApplied_ = false;
  EnsureBaseStyles();
}

void SyntaxHighlighter::Apply(const SyntaxDefinition& definition)
{
  if (!editor_)
  {
    return;
  }

  EnsureBaseStyles();

  const LRESULT length = SendMessage(editor_, SCI_GETLENGTH, 0, 0);
  if (length <= 0)
  {
    return;
  }

  const size_t byteCount = static_cast<size_t>(length);
  std::string text(byteCount + 1, '\0');
  SendMessage(editor_, SCI_GETTEXT, static_cast<WPARAM>(length + 1),
              reinterpret_cast<LPARAM>(text.data()));
  text.resize(byteCount);

  std::vector<unsigned char> styles(byteCount, STYLE_DEFAULT);

  for (const auto& rule : definition.rules)
  {
    const int style = StyleForGroup(rule.group);
    if (style == STYLE_DEFAULT)
    {
      continue;
    }

    rule.regex.ForEachMatch(text, [&](size_t start, size_t end) {
      if (start >= styles.size())
      {
        return;
      }
      const size_t clampedEnd = end > styles.size() ? styles.size() : end;
      for (size_t i = start; i < clampedEnd; ++i)
      {
        styles[i] = static_cast<unsigned char>(style);
      }
    });
  }

  SendMessage(editor_, SCI_STARTSTYLING, 0, 0);
  SendMessage(editor_, SCI_SETSTYLINGEX, static_cast<WPARAM>(styles.size()),
              reinterpret_cast<LPARAM>(styles.data()));
  InvalidateRect(editor_, nullptr, FALSE);
}

void SyntaxHighlighter::Clear()
{
  if (!editor_)
  {
    return;
  }

  SendMessage(editor_, SCI_STARTSTYLING, 0, 0);
  SendMessage(editor_, SCI_SETSTYLINGEX, 0, 0);
}

void SyntaxHighlighter::ApplyTheme(const AppUiTheme& theme, int fontSize)
{
  theme_ = theme;
  if (fontSize >= 8)
  {
    fontSize_ = fontSize;
  }
  InvalidateBaseStyles();
  EnsureBaseStyles();
}

void SyntaxHighlighter::InvalidateBaseStyles()
{
  baseStylesApplied_ = false;
}

void SyntaxHighlighter::EnsureBaseStyles()
{
  if (!editor_ || baseStylesApplied_)
  {
    return;
  }

  SendMessage(editor_, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>("Consolas"));
  SendMessage(editor_, SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize_);
  SendMessage(editor_, SCI_STYLESETFORE, STYLE_DEFAULT, theme_.editor.defaultFore);
  SendMessage(editor_, SCI_STYLESETBACK, STYLE_DEFAULT, theme_.editor.defaultBack);
  SendMessage(editor_, SCI_STYLECLEARALL, 0, 0);

  SendMessage(editor_, SCI_STYLESETFORE, STYLE_LINENUMBER, theme_.editor.lineNumberFore);
  SendMessage(editor_, SCI_STYLESETBACK, STYLE_LINENUMBER, theme_.editor.lineNumberBack);

  SendMessage(editor_, SCI_STYLESETFORE, kStyleComment, theme_.syntax.comment);
  SendMessage(editor_, SCI_STYLESETFORE, kStyleString, theme_.syntax.stringColor);
  SendMessage(editor_, SCI_STYLESETFORE, kStyleNumber, theme_.syntax.number);
  SendMessage(editor_, SCI_STYLESETFORE, kStyleKeyword, theme_.syntax.keyword);
  SendMessage(editor_, SCI_STYLESETFORE, kStyleType, theme_.syntax.type);
  SendMessage(editor_, SCI_STYLESETFORE, kStylePreproc, theme_.syntax.preproc);
  SendMessage(editor_, SCI_STYLESETFORE, kStyleOperator, theme_.syntax.operatorColor);
  SendMessage(editor_, SCI_STYLESETFORE, kStyleIdentifier, theme_.syntax.identifier);

  baseStylesApplied_ = true;
}

int SyntaxHighlighter::StyleForGroup(const std::string& group) const
{
  if (StartsWith(group, "comment"))
  {
    return kStyleComment;
  }
  if (StartsWith(group, "constant.string"))
  {
    return kStyleString;
  }
  if (StartsWith(group, "constant.number"))
  {
    return kStyleNumber;
  }
  if (StartsWith(group, "constant.bool"))
  {
    return kStyleKeyword;
  }
  if (StartsWith(group, "statement"))
  {
    return kStyleKeyword;
  }
  if (StartsWith(group, "type"))
  {
    return kStyleType;
  }
  if (StartsWith(group, "preproc"))
  {
    return kStylePreproc;
  }
  if (StartsWith(group, "symbol.operator") || StartsWith(group, "symbol.brackets"))
  {
    return kStyleOperator;
  }
  if (StartsWith(group, "identifier"))
  {
    return kStyleIdentifier;
  }

  return STYLE_DEFAULT;
}
