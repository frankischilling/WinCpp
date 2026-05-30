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
  : editor_(nullptr), baseStylesApplied_(false)
{
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

  const LRESULT length = SendMessage(editor_, SCI_GETTEXTLENGTH, 0, 0);
  if (length <= 0)
  {
    return;
  }

  std::string text(static_cast<size_t>(length) + 1, '\0');
  SendMessage(editor_, SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(text.data()));
  text.resize(static_cast<size_t>(length));

  std::vector<unsigned char> styles(text.size(), STYLE_DEFAULT);

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

void SyntaxHighlighter::EnsureBaseStyles()
{
  if (!editor_ || baseStylesApplied_)
  {
    return;
  }

  SendMessage(editor_, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>("Consolas"));
  SendMessage(editor_, SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
  SendMessage(editor_, SCI_STYLESETFORE, STYLE_DEFAULT, RGB(32, 32, 32));
  SendMessage(editor_, SCI_STYLESETBACK, STYLE_DEFAULT, RGB(255, 255, 255));
  SendMessage(editor_, SCI_STYLECLEARALL, 0, 0);

  SendMessage(editor_, SCI_STYLESETFORE, STYLE_LINENUMBER, RGB(96, 96, 96));
  SendMessage(editor_, SCI_STYLESETBACK, STYLE_LINENUMBER, RGB(248, 248, 248));

  SendMessage(editor_, SCI_STYLESETFORE, kStyleComment, RGB(0, 128, 0));
  SendMessage(editor_, SCI_STYLESETFORE, kStyleString, RGB(163, 21, 21));
  SendMessage(editor_, SCI_STYLESETFORE, kStyleNumber, RGB(9, 51, 166));
  SendMessage(editor_, SCI_STYLESETFORE, kStyleKeyword, RGB(0, 0, 160));
  SendMessage(editor_, SCI_STYLESETFORE, kStyleType, RGB(43, 145, 175));
  SendMessage(editor_, SCI_STYLESETFORE, kStylePreproc, RGB(128, 64, 0));
  SendMessage(editor_, SCI_STYLESETFORE, kStyleOperator, RGB(0, 0, 0));
  SendMessage(editor_, SCI_STYLESETFORE, kStyleIdentifier, RGB(0, 0, 0));

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
