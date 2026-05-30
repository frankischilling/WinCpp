# WinCpp

A simple text editor for Windows, built with C++ and Win32. It uses [Scintilla](https://www.scintilla.org/) for editing and supports multiple tabs, syntax highlighting, find/replace, and a project tree.

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

Run `WinCpp.exe`. Recent files are stored in `%APPDATA%\WinCpp\config.json`.

## Shortcuts

| Action        | Keys      |
|---------------|-----------|
| New file      | Ctrl+N    |
| Open file     | Ctrl+O    |
| Save          | Ctrl+S    |
| Find          | Ctrl+F    |
| Replace       | Ctrl+H    |
| Go to line    | Ctrl+G    |
| Close tab     | Ctrl+W    |

## License

This project uses third-party libraries (Scintilla, yaml-cpp, PCRE2). See their respective licenses.
