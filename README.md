# WinCpp

WinCpp is a classic Win32 text editor written in C++. It uses Scintilla for the
editing surface and keeps the overall feel of a compact late-2000s Windows
utility/editor: menu bar, toolbar, tabs, split panes, project tree, output pane,
dialogs, and status bar.

![Local screenshot](img/demo.png)

## Highlights

- Native Windows desktop app built with C++20, Win32, common controls, and CMake.
- Scintilla-backed editor with one buffer per document.
- Multiple tabs with drag reorder, close, close others, close all, and pinning.
- VS Code-style editor groups with split right/down, drag-to-split, center-drop
  merge, empty group cleanup, and split-copy tab close behavior.
- Project tree with folder open, filtering, file icons, and `path:line` open.
- Find, replace, regex search, whole word, match case, find previous/next, and go
  to line.
- Output pane for captured command output.
- Syntax highlighting from bundled micro editor YAML syntax definitions.
- Session restore for open tabs, project root, pane sizes, and split layout.
- Classic toolbar and status bar with line/column, language, EOL, selection, and
  modified state.

## Feature Overview

### Editor

- Undo, redo, cut, copy, paste
- Line numbers with margin padding
- Current-line highlight
- Auto-indent
- Optional word wrap
- Tab-to-space settings
- Convert tabs to spaces and spaces to tabs
- Brace matching and go-to-matching-brace
- Duplicate line, delete line, move line up/down
- Trim trailing whitespace
- Zoom in, zoom out, and reset zoom
- Brace-based code folding

### Tabs And Splits

- Multiple open files
- Drag tabs to reorder
- Drag tabs between editor groups
- Split editor right or down
- Drop preview for split targets
- Center-drop merge behavior
- Last-tab group movement when dragging a whole group
- Empty editor group cleanup
- Close a split copy without closing the document when the document is still open
  in another group
- Tab context menu with close, close others, close all, pin, split right, and
  split down

### Search

- Find dialog
- Replace dialog
- Find next and find previous
- Replace current match
- Replace all
- Match case
- Whole word
- Regex search using PCRE2
- Find highlights across editor views
- Go to line dialog

### Project Pane

- Open folder
- Tree view with directory and file icons
- Filter project files
- Double-click to open a file
- Supports `file:line` style entries
- Resizable project pane
- Pane layout aligns with the editor and output pane

### Output Pane

- Toggleable output pane
- Resizable splitter
- Captures output from the Run Command dialog
- Uses a read-only multiline edit control
- Layout aligns with the project pane and editor area

### Syntax And Themes

- Syntax definitions loaded from bundled micro editor YAML files
- Language detection by extension, filename, header, and signature
- Status bar language display
- Status bar language override menu
- Built-in light/dark UI chrome derived from Windows colors
- Theme parser support for micro `.micro` color scheme files

### Persistence

WinCpp stores user state under:

```text
%APPDATA%\WinCpp\
```

Files:

- `config.json` stores recent files and editor settings.
- `config.json.session` stores open tabs, project root, pane sizes, and layout.

## Editor Settings

The `editor` object in `config.json` supports:

| Setting | Default | Description |
| --- | --- | --- |
| `tabsize` | `4` | Tab width in spaces |
| `tabstospaces` | `false` | Insert spaces when pressing Tab |
| `autoindent` | `true` | Copy indentation when creating a new line |
| `matchbrace` | `true` | Highlight matching braces |
| `wordwrap` | `false` | Enable Scintilla word wrap |
| `fontsize` | `11` | Editor font size |
| `zoom` | `0` | Scintilla zoom level |
| `trimTrailingWhitespaceOnSave` | `false` | Remove trailing whitespace on save |

## Keyboard Shortcuts

| Action | Shortcut |
| --- | --- |
| New file | `Ctrl+N` |
| Open file | `Ctrl+O` |
| Save | `Ctrl+S` |
| Find | `Ctrl+F` |
| Find next | `F3` |
| Find previous | `Shift+F3` |
| Replace | `Ctrl+H` |
| Go to line | `Ctrl+G` |
| Duplicate line | `Ctrl+D` |
| Close tab | `Ctrl+W` |
| Command palette | `Ctrl+Shift+P` |
| Split editor right | `Ctrl+\` |

Other commands are available from the menu bar, toolbar, tab context menu, and
command palette.

## Requirements

- Windows 10 or later
- Visual Studio 2022 or newer with Desktop development with C++
- CMake 3.10 or newer
- Git, because CMake fetches third-party dependencies

CMake downloads and builds these dependencies automatically:

- Scintilla
- yaml-cpp
- PCRE2
- GoogleTest for tests

## Build

From the repository parent directory, open a Visual Studio x64 developer command
prompt, then run:

```bat
cd WinCpp
cmake -B out/build/x64-release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/x64-release --target WinCpp
```

The app executable is created at:

```text
WinCpp/out/build/x64-release/WinCpp/WinCpp.exe
```

You can also open the CMake project in Visual Studio and build the `WinCpp`
target.

## Run

After building:

```bat
WinCpp\out\build\x64-release\WinCpp\WinCpp.exe
```

Or run the executable directly from Explorer.

## Tests

Build and run the test suite from the same Visual Studio x64 developer command
prompt:

```bat
cd WinCpp
cmake --build out/build/x64-release --target WinCppTests
ctest --test-dir out/build/x64-release/WinCpp/tests --output-on-failure
```

The current suite has 162 automated tests.

You can also run the test executable directly:

```bat
out\build\x64-release\WinCpp\tests\WinCppTests.exe
```

## Test Coverage Map

| Area | Test suite |
| --- | --- |
| Tab hit testing, insert positions, pinned ordering | `TabBarLogic` |
| Tab control behavior and messages | `TabBar`, `TabBarMessage` |
| Editor split drop zones and previews | `EditorSplitDrop` |
| Editor groups, tab movement, split/merge behavior | `EditorWorkspace` |
| Scintilla editor operations | `EditorView` |
| Find/replace engine | `EditorSearch`, `EditorView` |
| Regex wrapper | `RegexPattern` |
| Syntax registry and highlighting | `SyntaxRegistry`, `SyntaxHighlighter` |
| Editor settings | `EditorSettings` |
| Indentation and folding helpers | `IndentLogic`, `FoldingLogic` |
| Recent files | `RecentFilesStore` |
| Config JSON | `ConfigJson` |
| Session save/restore | `SessionState` |
| Status bar formatting | `StatusBarModel` |
| Document titles and save lists | `DocumentNaming`, `DocumentCollectionLogic` |
| Project path parsing and filtering | `PathLineParser`, `TreeFilterLogic` |
| Command palette filtering | `CommandRegistry` |
| Command output capture | `ProcessOutput` |
| Dialog layout | `DialogLayout` |
| UI helpers and DPI scaling | `UiHelpers`, `UiMetrics` |
| Main window pane geometry | `MainWindowTest` |
| Theme parsing | `ThemeLoader` |

## Source Layout

The C++ source is organized under `WinCpp/WinCpp/`:

```text
app/            Main window orchestration, dialogs, document commands, session IO
core/           Command IDs and command registry
editor/         Scintilla editor view, workspace groups, search, settings, folding
platform/       Windows process/output helpers
project/        Project tree, file filtering, path:line parsing
storage/        Config, recent files, session serialization
syntax_engine/  Syntax registry, highlighting, regex, theme parsing
ui/             Toolbar, tabs, dialogs, status bar, UI metrics, theme chrome
syntax/         Bundled micro syntax YAML files copied beside the executable
cmake/          Third-party build integration
```

Tests live under `WinCpp/tests/`.

The main window implementation is intentionally split by responsibility:

- `app/MainWindow.cpp` - creation and top-level window setup
- `app/MainWindowMessages.cpp` - Win32 message dispatch and command handling
- `app/MainWindowDialogs.cpp` - Find, Replace, Go To Line, Run Command, Credits
- `app/MainWindowDocuments.cpp` - document lifecycle and file commands
- `app/MainWindowSession.cpp` - session load/save
- `project/MainWindowProject.cpp` - project tree population and activation
- `ui/MainWindowChrome.cpp` - menu, toolbar, status bar, pane layout, splitters

The editor workspace is split into:

- `editor/EditorWorkspace.cpp` - group creation, split tree, layout
- `editor/EditorWorkspaceTabs.cpp` - tab/document synchronization
- `editor/EditorWorkspaceDrag.cpp` - drag tracking, split drop, overlay rendering

## Manual Checks

These flows depend on native Windows dialogs or live UI behavior and are best
checked manually after large UI changes:

- Open File and Save As common dialogs
- Open Folder and project tree population
- Live tab drag ghost rendering
- Splitter dragging for project and output panes
- Dark mode and high-DPI layouts
- Status bar language override menu
- Credits dialog
- Run Command dialog with real commands

## Third-Party Credits

WinCpp uses:

- Scintilla for the editor control
- yaml-cpp for YAML parsing
- PCRE2 for regex support
- GoogleTest for automated tests
- micro editor syntax files for bundled language definitions

Use `Help > Credits` inside the app for the in-app acknowledgements.

## License

This project includes third-party libraries and syntax definitions. See the
respective upstream projects for their license terms.
