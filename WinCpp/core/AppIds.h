#pragma once

#include "WinCpp.h"

// Menu command identifiers.
constexpr UINT kCmdFileNew = 1001;
constexpr UINT kCmdFileOpen = 1002;
constexpr UINT kCmdFileSave = 1003;
constexpr UINT kCmdFileSaveAs = 1004;
constexpr UINT kCmdFileExit = 1005;
constexpr UINT kCmdFileOpenFolder = 1006;
constexpr UINT kCmdFileSaveAll = 1007;
constexpr UINT kCmdFileCloseAll = 1008;

constexpr UINT kCmdFileOpenRecentBase = 1050;
constexpr UINT kCmdFileOpenRecentMax = 1059;
constexpr UINT kCmdFileOpenRecentClear = 1060;

constexpr UINT kCmdEditUndo = 1101;
constexpr UINT kCmdEditRedo = 1102;
constexpr UINT kCmdEditCut = 1103;
constexpr UINT kCmdEditCopy = 1104;
constexpr UINT kCmdEditPaste = 1105;
constexpr UINT kCmdEditFind = 1110;
constexpr UINT kCmdEditReplace = 1111;
constexpr UINT kCmdEditGoToLine = 1112;
constexpr UINT kCmdEditFindPrevious = 1113;
constexpr UINT kCmdEditDuplicateLine = 1114;
constexpr UINT kCmdEditDeleteLine = 1115;
constexpr UINT kCmdEditMoveLineUp = 1116;
constexpr UINT kCmdEditMoveLineDown = 1117;
constexpr UINT kCmdEditTrimTrailingWhitespace = 1118;
constexpr UINT kCmdEditConvertTabsToSpaces = 1119;
constexpr UINT kCmdEditGotoMatchingBrace = 1120;
constexpr UINT kCmdEditCommandPalette = 1121;

constexpr UINT kCmdViewProjectPane = 1201;
constexpr UINT kCmdViewOutputPane = 1202;
constexpr UINT kCmdViewWordWrap = 1203;
constexpr UINT kCmdViewSplitRight = 1204;
constexpr UINT kCmdViewSplitDown = 1205;
constexpr UINT kCmdViewCloseEditorGroup = 1206;
constexpr UINT kCmdViewZoomIn = 1207;
constexpr UINT kCmdViewZoomOut = 1208;
constexpr UINT kCmdViewZoomReset = 1209;
constexpr UINT kCmdViewCodeFolding = 1213;
constexpr UINT kCmdViewRunCommand = 1214;

constexpr UINT kCmdTabClose = 1210;
constexpr UINT kCmdTabCloseOthers = 1211;
constexpr UINT kCmdTabCloseAll = 1212;
constexpr UINT kCmdTabPin = 1215;

constexpr UINT kCmdHelpAbout = 1301;
constexpr UINT kCmdHelpCredits = 1302;

// Child control identifiers.
constexpr UINT kCtrlEditor = 2001;
constexpr UINT kCtrlTab = 2002;
constexpr UINT kCtrlProjectFilter = 2003;
