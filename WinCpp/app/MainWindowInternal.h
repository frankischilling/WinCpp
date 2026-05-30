#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

extern std::wstring g_lastRunCommand;

std::filesystem::path GetExecutableDir();
std::wstring Utf8ToWide(const std::string &text);
std::string WideToUtf8(const std::wstring &text);
std::wstring BuildAboutText();
std::wstring BuildCreditsText();
std::wstring ShowOpenFileDialog(HWND owner);
std::wstring ShowSaveFileDialog(HWND owner, const std::wstring &initialPath);
std::wstring ShowFolderDialog(HWND owner);
HWND CreateLabeledEdit(HWND parent, int x, int y, int width, int id);
HWND CreateDialogPromptLabel(HWND parent, const wchar_t *text, int id);
HWND CreateDialogFieldLabel(HWND parent, const wchar_t *text, int id);
HWND CreateDialogButton(HWND parent, const wchar_t *label, int x, int y, int width, int id,
                        bool isDefault = false);
HWND CreateDialogCheck(HWND parent, const wchar_t *label, int x, int y, int width, int id);
std::wstring GetWindowTextString(HWND hwnd);
bool RegisterDialogClass(const wchar_t *className, WNDPROC procedure);
void CenterWindowOnParent(HWND dialog, HWND parent, int width, int height);
