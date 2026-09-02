#include <windows.h>

constexpr int IDC_CHK_ANIMATION = 2001;

void SetSystemAnimations(bool enable) {
	SystemParametersInfoW(
		SPI_SETCLIENTAREAANIMATION,
		0,
		(PVOID)(uintptr_t)(enable ? TRUE : FALSE),
		SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
	);
	
	ANIMATIONINFO ai = {};
	ai.cbSize = sizeof(ANIMATIONINFO);
	ai.iMinAnimate = enable ? 1 : 0;
	
	SystemParametersInfoW(
		SPI_SETANIMATION,
		sizeof(ANIMATIONINFO),
		&ai,
		SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
	);
}

void SwitchVirtualDesktop(bool goRight) {
	INPUT releaseAlt = {};
	releaseAlt.type = INPUT_KEYBOARD;
	releaseAlt.ki.wVk = VK_MENU;
	releaseAlt.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &releaseAlt, sizeof(INPUT));
	
	INPUT inputs[6] = {};
	WORD keyArrow = goRight ? VK_RIGHT : VK_LEFT;
	
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_CONTROL;
	
	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_LWIN;
	
	inputs[2].type = INPUT_KEYBOARD;
	inputs[2].ki.wVk = keyArrow;
	
	inputs[3].type = INPUT_KEYBOARD;
	inputs[3].ki.wVk = keyArrow;
	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
	
	inputs[4].type = INPUT_KEYBOARD;
	inputs[4].ki.wVk = VK_LWIN;
	inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;
	
	inputs[5].type = INPUT_KEYBOARD;
	inputs[5].ki.wVk = VK_CONTROL;
	inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;
	
	SendInput(6, inputs, sizeof(INPUT));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			CreateWindowW(
				L"BUTTON", L"Turn off screen switching animation (Recommended)",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, 20, 360, 24, hwnd, (HMENU)IDC_CHK_ANIMATION, NULL, NULL
			);
			CheckDlgButton(hwnd, IDC_CHK_ANIMATION, BST_CHECKED);
			
			CreateWindowW(
				L"STATIC", L"- Alt + 1: Previous desktop\n- Alt + 2: Next desktop",
				WS_VISIBLE | WS_CHILD | SS_LEFT,
				20, 60, 280, 50, hwnd, NULL, NULL, NULL
			);
			
			SetSystemAnimations(false);
			
			if (!RegisterHotKey(hwnd, 1, MOD_ALT | MOD_NOREPEAT, '1') || !RegisterHotKey(hwnd, 2, MOD_ALT | MOD_NOREPEAT, '2')) {
				MessageBoxW(hwnd, L"Failed to register hotkey!", L"Error", MB_ICONERROR);
			}
			
			return 0;
		}
		case WM_COMMAND: {
			int id = LOWORD(wParam);
			if (id == IDC_CHK_ANIMATION) {
				bool checked = (IsDlgButtonChecked(hwnd, IDC_CHK_ANIMATION) == BST_CHECKED);
				SetSystemAnimations(!checked);
			}
			return 0;
		}
		case WM_HOTKEY: {
			if (wParam == 1) SwitchVirtualDesktop(false);
			if (wParam == 2) SwitchVirtualDesktop(true);
			return 0;
		}
		case WM_DESTROY:
			UnregisterHotKey(hwnd, 1);
			UnregisterHotKey(hwnd, 2);
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
	const wchar_t CLASS_NAME[] = L"StealthDesktopSwitchClass";
	
	WNDCLASSW wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	
	RegisterClassW(&wc);
	
	HWND hwnd = CreateWindowExW(
		WS_EX_DLGMODALFRAME,
		CLASS_NAME, L"Stealth Desktop Switcher",
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 420, 180,
		NULL, NULL, hInstance, NULL
	);
	
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	
	MSG msg = {};
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}

