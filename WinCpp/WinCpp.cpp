// WinCpp.cpp : Defines the entry point for the application.
//

#include "WinCpp.h"
#include "MainWindow.h"

#include <objbase.h>

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int nCmdShow)
{
	const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(comInit))
	{
		return 1;
	}

	const bool comNeedsUninit = comInit == S_OK;

	MainWindow window;
	if (!window.Create(instance, nCmdShow))
	{
		if (comNeedsUninit)
		{
			CoUninitialize();
		}
		return 0;
	}

	MSG msg = {};
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		if (!window.TranslateAcceleratorMessage(&msg))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	if (comNeedsUninit)
	{
		CoUninitialize();
	}
	return static_cast<int>(msg.wParam);
}
