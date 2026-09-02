#include <windows.h>

constexpr int IDC_CHK_ANIMATION = 2001;

static BOOL g_origClientAreaAnim = TRUE;
static ANIMATIONINFO g_origAnimInfo = {};

void BackupSystemAnimations() {
	SystemParametersInfoW(
		SPI_GETCLIENTAREAANIMATION,
		0,
		&g_origClientAreaAnim,
		0
	);
	
	g_origAnimInfo.cbSize = sizeof(ANIMATIONINFO);
	SystemParametersInfoW(
		SPI_GETANIMATION,
		sizeof(ANIMATIONINFO),
		&g_origAnimInfo,
		0
	);
}

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

void RestoreSystemAnimations() {
	SystemParametersInfoW(
		SPI_SETCLIENTAREAANIMATION,
		0,
		(PVOID)(uintptr_t)g_origClientAreaAnim,
		SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
	);
	
	SystemParametersInfoW(
		SPI_SETANIMATION,
		sizeof(ANIMATIONINFO),
		&g_origAnimInfo,
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
			BackupSystemAnimations();
			
			HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			
			HWND hCheck = CreateWindowW(
				L"BUTTON", L"Turn off screen switching animation (Recommended)",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, 20, 360, 24, hwnd, (HMENU)IDC_CHK_ANIMATION, NULL, NULL
			);
			if (!hCheck) return -1;
			SendMessageW(hCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
			CheckDlgButton(hwnd, IDC_CHK_ANIMATION, BST_CHECKED);
			
			HWND hStatic = CreateWindowW(
				L"STATIC", L"- Alt + 1: Previous desktop\n- Alt + 2: Next desktop",
				WS_VISIBLE | WS_CHILD | SS_LEFT,
				20, 60, 280, 50, hwnd, NULL, NULL, NULL
			);
			if (!hStatic) return -1;
			SendMessageW(hStatic, WM_SETFONT, (WPARAM)hFont, TRUE);
			
			SetSystemAnimations(false);
			
			if (!RegisterHotKey(hwnd, 1, MOD_ALT | MOD_NOREPEAT, '1')) {
				MessageBoxW(hwnd, L"Failed to register Alt+1 hotkey!", L"Error", MB_ICONERROR);
				return -1;
			}
			if (!RegisterHotKey(hwnd, 2, MOD_ALT | MOD_NOREPEAT, '2')) {
				MessageBoxW(hwnd, L"Failed to register Alt+2 hotkey!", L"Error", MB_ICONERROR);
				UnregisterHotKey(hwnd, 1);
				return -1;
			}
			
			return 0;
		}
		case WM_COMMAND: {
			int id = LOWORD(wParam);
			if (id == IDC_CHK_ANIMATION) {
				bool checked = (IsDlgButtonChecked(hwnd, IDC_CHK_ANIMATION) == BST_CHECKED);
				if (checked) {
					SetSystemAnimations(false);
				} else {
					RestoreSystemAnimations();
				}
			}
			return 0;
		}
		case WM_HOTKEY: {
			if (wParam == 1) SwitchVirtualDesktop(false);
			if (wParam == 2) SwitchVirtualDesktop(true);
			return 0;
		}
		case WM_DESTROY:
			RestoreSystemAnimations();
			
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
	
	if (!RegisterClassW(&wc)) {
		MessageBoxW(NULL, L"Failed to register window class!", L"Error", MB_ICONERROR);
		return 1;
	}
	
	HWND hwnd = CreateWindowExW(
		WS_EX_DLGMODALFRAME,
		CLASS_NAME, L"Stealth Desktop Switcher",
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 420, 180,
		NULL, NULL, hInstance, NULL
	);
	
	if (!hwnd) {
		MessageBoxW(NULL, L"Failed to create window!", L"Error", MB_ICONERROR);
		return 1;
	}
	
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	
	MSG msg = {};
	BOOL bRet;
	while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0) {
		if (bRet == -1) {
			return 1;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}

