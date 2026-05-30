# WinCpp

A simple text editor for Windows, built with C++ and Win32. It uses [Scintilla](https://www.scintilla.org/) for editing.

![Local screenshot](img/demo.png)

## Features

- **Tabs** - Multiple open files, drag to reorder, close tab (`Ctrl+W`), pin tabs, and context menu (close / close others / close all)
- **Split editors** - VS Code-style editor groups: split right or down, drag tabs between panes with drop previews, merge by dropping in the center, close a group from the View menu (`Ctrl+\` splits right)
- **Editing** - Scintilla buffer per tab: undo/redo, cut/copy/paste, line numbers, caret line highlight, optional word wrap, auto-indent, tab/space conversion, brace matching, duplicate/delete/move line, trim trailing whitespace, zoom
- **Syntax highlighting** - Language detection from file extension and first-line hints; rules loaded from bundled [micro](https://github.com/zyedidia/micro) YAML syntax files (C/C++, Python, JSON, and many more); manual language override from the status bar menu
- **Find & replace** - Find, find previous, replace, replace all, match case, whole word, regex (PCRE2), forward/backward search, F3 / Shift+F3; go to line; open `path:line` from the project tree
- **Project pane** - Open a folder, filter files, double-click to open (supports `file:line`)
- **Output pane** - Toggleable panel; run a simple command (`cmd /c echo …`) and capture output
- **Session restore** - Open tabs and layout saved beside `%APPDATA%\WinCpp\config.json` as `config.json.session`
- **Code folding** - Brace-based fold margin (View menu toggle)
- **Themes (foundation)** - Parser for micro `.micro` colorscheme files (`ThemeLoader`) for future editor chrome styling
- **Command palette** - Minimal filterable command list (`Ctrl+Shift+P`)
- **Recent files** - Open Recent submenu; list and editor settings stored in `%APPDATA%\WinCpp\config.json`
- **Status bar** - Line/column, language, EOL, selection length, modified state

### Editor settings (`config.json` → `editor`)

| Setting | Default | Notes |
|---------|---------|--------|
| `tabsize` | 4 | Tab width |
| `tabstospaces` | false | Insert spaces when pressing Tab |
| `autoindent` | true | Copy indent on new line |
| `matchbrace` | true | Brace highlight |
| `wordwrap` | false | |
| `fontsize` | 11 | |
| `zoom` | 0 | Scintilla zoom level |
| `trimTrailingWhitespaceOnSave` | false | |

## Requirements

- Windows 10 or later
- Visual Studio 2022 or newer (with **Desktop development with C++**)
- CMake 3.10+
- Git (CMake downloads Scintilla, yaml-cpp, and PCRE2 automatically)

## Build

Open a **x64 Native Tools** command prompt (or run `vcvars64.bat`), then:

```bat
cd WinCpp
cmake -B out/build/x64-release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/x64-release --target WinCpp
```

The executable is at `WinCpp/out/build/x64-release/WinCpp/WinCpp.exe`.

You can also open `WinCpp/WinCpp.sln` in Visual Studio and build from there.

## Run

Run `WinCpp.exe`. Settings and recent files are stored in `%APPDATA%\WinCpp\config.json`.

## Shortcuts

| Action | Keys |
|--------|------|
| New file | Ctrl+N |
| Open file | Ctrl+O |
| Save | Ctrl+S |
| Save all | (File menu) |
| Find | Ctrl+F |
| Find next / previous | F3 / Shift+F3 |
| Replace | Ctrl+H |
| Go to line | Ctrl+G |
| Close tab | Ctrl+W |
| Duplicate line | Ctrl+D |
| Command palette | Ctrl+Shift+P |
| Split right | Ctrl+\ |
| Zoom in / out / reset | (View menu) |

Use **Help > Credits** for third-party acknowledgements (Micro syntax files, Scintilla, and others).

## Tests

Unit and integration tests use [Google Test](https://github.com/google/googletest). From the same build directory as the app (use the **x64 Native Tools** environment so MSVC can compile dependencies):

```bat
cmake --build out/build/x64-release --target WinCppTests
ctest --test-dir out/build/x64-release/WinCpp/tests --output-on-failure
```

Or run `WinCppTests.exe` directly from `out/build/x64-release/WinCpp/tests/`.

### Feature coverage matrix

| Feature | Test suite(s) |
|---------|----------------|
| Tabs | `TabBarLogic`, `TabBarMessage`, `TabBar`, `EditorWorkspace` |
| Split editors | `EditorSplitDrop`, `EditorWorkspace` |
| Editing | `EditorView`, `IndentLogic` |
| Editor settings | `EditorSettings` |
| Syntax highlighting | `SyntaxRegistry`, `SyntaxHighlighter` |
| Find & replace | `EditorView`, `EditorSearch` |
| Status bar | `StatusBarModel` |
| Save / close all | `DocumentCollectionLogic` |
| Path `file:line` | `PathLineParser` |
| Command palette filter | `CommandRegistry` |
| Project tree filter | `TreeFilterLogic` |
| Session restore | `SessionState` |
| Run command output | `ProcessOutput` |
| Code folding | `FoldingLogic` |
| Colorschemes | `ThemeLoader` |
| Recent files | `RecentFilesStore` |
| Tab / window titles | `DocumentNaming` |
| UI fonts | `UiHelpers` |
| Regex syntax rules | `RegexPattern` |

**139 automated tests** cover testable logic for tabs, splits, editing, settings, syntax, search, workspace, and themes.

### Manual testing only

These flows are not automated (file/folder dialogs, full main window, or live drag rendering):

- **Open / Save As** and **Open Folder** common dialogs
- **Project tree** population and double-click open (beyond unit-tested path parsing)
- **Output pane** layout toggling with the full chrome
- **Find/Replace** and **Go to Line** modal dialogs (logic is tested; UI is manual)
- Live **drag ghost** rendering during tab drag
- **Credits** dialog
- Applying micro colorschemes to the live Scintilla theme (parser only today)

## License

This project uses third-party libraries (Scintilla, yaml-cpp, PCRE2). See their respective licenses.
